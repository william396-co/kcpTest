#include "server.h"

#include "src/connection.h"

#include <cstdio>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

Server::Server(uint16_t port, uint32_t work_thread_cnt)
    : listen{ std::make_unique<UdpSocket>() }, md{ 0 }, listen_port{ port }, work_thread_cnt{ work_thread_cnt == 0 ? 1u : work_thread_cnt }
{
    listen->setNonblocking();
    if (!listen->bind(listen_port)) {
        throw std::runtime_error( "listen socket bind error" );
    }
    listen->setLostrate(lost_rate);
    // Pre-create shards so worker threads only need to lock their own shard map.
    shards.reserve(this->work_thread_cnt);
    for (uint32_t i = 0; i < this->work_thread_cnt; ++i) {
        shards.push_back(std::make_unique<ConnectionShard>());
    }
}

Server::~Server()
{
    stopService();
}

bool Server::startService()
{
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return false;
    }

    // One worker thread owns periodic updates/reclaim for each shard.
    work_threads.reserve(work_thread_cnt);
    for (uint32_t i = 0; i < work_thread_cnt; ++i) {
        work_threads.emplace_back(&Server::worker_loop, this, i);
    }
    recv_thread = joining_thread(std::thread(&Server::recv_work, this));
    return true;
}

bool Server::stopService()
{
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false)) {
        return false;
    }

    recv_thread = joining_thread{};
    work_threads.clear();
    return true;
}

void Server::set_application_message_handler(ApplicationMessageHandler handler)
{
    std::lock_guard lock(handler_mutex);
    application_message_handler = std::move(handler);
}

void Server::setlostrate(int lostrate)
{
    lost_rate = lostrate / 2;
    listen->setLostrate(lost_rate);
}

void Server::setmode( int mode )
{    
    md = mode;
}

uint32_t Server::alloc_conv()
{
    while (true) {
        const auto conv = ++nextConv;
        if (!findConn(conv)) {
            return conv;
        }
    }
}

ConnectionPtr Server::createConn(const UdpEndpoint& endpoint, uint32_t conv)
{
    const auto idx = shard_index(conv);
    auto& shard = *shards[idx];
    ConnectionPtr conn;

    {
        std::lock_guard shard_lock(shard.mutex);
        auto it = shard.connections.find(conv);
        if (it != shard.connections.end()) {
            return it->second;
        }

        conn = std::make_shared<Connection>(listen.get(), endpoint, conv);
        conn->setmode( md );
        conn->set_show( show );
        conn->set_payload_dispatch_handler([this](Connection& connection, const char* data, size_t len) {
            invoke_application_handler(connection, data, len);
        });
        shard.connections.emplace(conv, conn);
    }

    {
        std::lock_guard conn_id_lock(conn_id_mutex);
        connIDConvMap[endpoint] = conv;
    }

    printf("new Connection:[%s:%d] accepted\n", endpoint.ip.c_str(), endpoint.port);
    return conn;
}

ConnectionPtr Server::findConn(uint32_t conv) const
{
    return findConn_unlocked(conv, shard_index(conv));
}

ConnectionPtr Server::findConn(const UdpEndpoint& endpoint) const
{
    uint32_t conv = 0;
    {
        std::lock_guard lock(conn_id_mutex);
        auto it = connIDConvMap.find(endpoint);
        if (it == connIDConvMap.end()) {
            return {};
        }
        conv = it->second;
    }
    return findConn_unlocked(conv, shard_index(conv));
}

void Server::recv_work()
{
    while ( running ) {
        util::isleep( 1 );

        // Receive one raw UDP datagram, then hand off to protocol decoding.
        UdpEndpoint remote;
        if ( listen->recv(&remote) <= 0 ) {
            continue;
        }
        parse_udp_data(remote, listen->getRecvBuffer(), listen->getRecvSize());
    }
}

void Server::worker_loop(uint32_t idx)
{
    time_t last_reclaim_ms = 0;
	while (running) {
		util::isleep(1);

        std::vector<ConnectionPtr> snapshot;
        {
            auto& shard = *shards[idx];
            std::lock_guard lock(shard.mutex);
            // Copy shared_ptrs out of the map so update() runs without holding the shard lock.
            snapshot.reserve(shard.connections.size());
            for (auto& [conv, connection] : shard.connections) {
                snapshot.push_back(connection);
            }
        }

		for (auto& connection : snapshot) {
			connection->update();
		}

        auto now_ms = util::now_ms();
        if (now_ms - last_reclaim_ms > 1000) {
            last_reclaim_ms = now_ms;
            // Cleanup is periodic rather than per-packet to keep the hot path cheap.
            reclaim_inactive(*shards[idx]);
        }
	}
}

void Server::parse_udp_data(const UdpEndpoint& remote, const char* buf, size_t len)
{
    DecodedPacket pkt{};
    if (!decode_packet(buf, len, pkt)) {
        return;
    }

    switch (pkt.type) {
    case PacketType::PKT_HANDSHAKE_REQ:
    {
		auto conn = findConn(remote);
		if (!conn) {
			// First packet from a client has no usable KCP session yet.
            // The server allocates a conv, creates a Connection, then replies with ACK.
			uint32_t conv = alloc_conv();
			std::cout << "conv: " << conv << " remoteIp: " << remote.ip << " remotePort: " << remote.port << "\n";
			conn = createConn(remote, conv);
		}
        if (conn) {            
            // Re-send ACK for duplicate handshake requests so the client can recover from lost packets.
            conn->send_shakehand_reply();
        }
        break;
    }
    case PacketType::PKT_KCP_DATA:
    {
        // KCP payloads are routed entirely by conv once the handshake is complete.
        auto conn = findConn(pkt.conv);
        if (!conn) {
            std::cout << " cannot find connection,conv:" << pkt.conv << "\n";
        }
        else {
            // Network receive stays lightweight; actual KCP input runs on the shard worker thread.
            conn->push_rcv_queue(pkt.payload, pkt.size);
        }
        break;
    }
    case PacketType::PKT_HANDSHAKE_ACK:
    {
        std::cerr << "invalid PacketType: " << pkt.type << "\n";
        break;
    }
    }
}

ConnectionPtr Server::findConn_unlocked(uint32_t conv, uint32_t idx) const
{
    auto& shard = *shards[idx];
    std::lock_guard lock(shard.mutex);
    auto it = shard.connections.find(conv);
    if (it != shard.connections.end()) {
        return it->second;
    }
    return {};
}

void Server::reclaim_inactive(ConnectionShard& shard)
{
    std::vector<UdpEndpoint> endpoints_to_remove;
    {
        std::lock_guard lock(shard.mutex);
        for (auto it = shard.connections.begin(); it != shard.connections.end();) {
            const auto& connection = it->second;
            // Remove sessions that have timed out, failed the handshake, or were explicitly closed.
            if (connection->dead_link() || connection->handshake_overtime() || connection->deactive()) {
                endpoints_to_remove.push_back(connection->getEndpoint());
                it = shard.connections.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (endpoints_to_remove.empty()) {
        return;
    }

    std::lock_guard lock(conn_id_mutex);
    for (const auto& endpoint : endpoints_to_remove) {
        connIDConvMap.erase(endpoint);
    }
}

void Server::invoke_application_handler(Connection& connection, const char* data, size_t len)
{
    ApplicationMessageHandler application_handler_copy;
    {
        std::lock_guard lock(handler_mutex);
        // Copy the std::function while holding the mutex, then invoke it after unlocking.
        // Calling user code under the lock would make deadlocks and lock contention much easier.
        application_handler_copy = application_message_handler;
    }

    if (application_handler_copy) {
        // A custom handler fully owns the message path.
        application_handler_copy(connection, data, len);
    }
}

uint32_t Server::shard_index(uint32_t conv) const
{
    return work_thread_cnt == 0 ? 0 : conv % work_thread_cnt;
}
