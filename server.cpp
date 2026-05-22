#include "server.h"

#include "src/connection.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

Server::Server(uint16_t port, uint32_t work_thread_cnt)
    : listen{ nullptr }, md{ 0 }, listen_port{ port }, work_thread_cnt{work_thread_cnt}
{
    listen = std::make_unique<UdpSocket>();
    listen->setNonblocking();
    if (!listen->bind(listen_port)) {
        throw std::runtime_error( "listen socket bind error" );
    }

    for (int i = 0;i != work_thread_cnt;++i) {
        work_threads.emplace_back(&Server::send_work, this, i);
    }
    recv_thread = std::thread(&Server::recv_work, this);
    conMapVec.resize(work_thread_cnt);
}

Server::~Server()
{
    for ( auto & v : conMapVec ) {
        for (auto& c : v) {
            delete c.second;
        }
    }
}

bool Server::startService()
{
    is_running = true;
    return true;
}

bool Server::stopService()
{
    is_running = false;
    return true;
}


void Server::setmode( int mode )
{    
    md = mode;
}

uint32_t Server::alloc_conv() const
{
    auto conv = 0;
    do
    {
        conv = ++nextConv;
	} while (conMapVec[conv % work_thread_cnt].count(conv));
    return conv;
}

ConnectionPtr  Server::createConn(UdpSocket* socket, const char* remote_ip, uint16_t remote_port, uint32_t conv)
{
    auto idx = conv % work_thread_cnt;
    auto it = conMapVec[idx].find(conv);
    if ( it != conMapVec[idx].end()) {
        return it->second;
    }
    auto conn = new Connection(socket, remote_ip, remote_port, conv);
    conn->setmode( md );
    conn->set_show( show );
    conn->setmode(md);
    if ( conn ) {
		conMapVec[idx].emplace(conv, conn);
		connIDConvMap[ConnID(remote_ip, remote_port)] = conv;
        printf("new Connection:[%s:%d] accepted\n", remote_ip, remote_port);
        return conn;
    }
    delete conn;
    return nullptr;
}

ConnectionPtr Server::findConn(uint32_t conv) const
{
    auto idx = conv % work_thread_cnt;
    auto it = conMapVec[idx].find(conv);
    if (it != conMapVec[idx].end()) {
        return it->second;
    }
    return nullptr;
}

ConnectionPtr Server::findConn(const char* ip, uint16_t port) const
{
    auto it = connIDConvMap.find({ ip,port });
    if (it != connIDConvMap.end()) {
        return findConn(it->second);
    }
    return {};
}

void Server::recv_work()
{
    while ( is_running ) {
        util::isleep( 1 );

        // lower level recv
        if ( listen->recv() <= 0 ) {
            continue;
        }
        parse_udp_data(listen->getRecvBuffer(), listen->getRecvSize());
    }
}

void Server::send_work(int idx)
{
	while (is_running) {
		util::isleep(1);
                
        auto now_ms = util::now_ms();
        auto& conMap = conMapVec[idx];

        // reclaim connection
        if (now_ms - last_reclaim_ms > 1000) {
            last_reclaim_ms = now_ms;
            for (auto it = conMap.begin(); it != conMap.end();) {
                if (it->second->dead_link()
                    || it->second->handshake_overtime()
                    || it->second->deactive()) {
                    delete it->second;
                    it = conMap.erase(it);
                }
                else
                    ++it;
            }
        }

        // connection update
		for (auto& it : conMap) {			
			it.second->update();
		}
	}
}

void Server::parse_udp_data(const char* buf, size_t len)
{
    DecodedPacket pkt{};
    if (!decode_packet(buf, len, pkt)) {
        return;
    }

    switch (pkt.type) {
    case PacketType::PKT_HANDSHAKE_REQ:
    {
		auto conn = findConn(listen->getRemoteIp(), listen->getRemotePort());
		if (!conn) {
			// handle sharehand
			// server: allocate conv and reply
			uint32_t conv = alloc_conv();
			std::cout << "conv: " << conv << "remoteIp: " << listen->getRemoteIp() << "remotePort: " << listen->getRemotePort() << "\n";
			conn = createConn(listen.get(), listen->getRemoteIp(), listen->getRemotePort(), conv);
		}
        if (conn) {            
            conn->send_shakehand_reply();
        }
        break;
    }
    case PacketType::PKT_KCP_DATA:
    {
        // pass pkt.payload / pkt.size to ikcp_input()
        auto conn = findConn(pkt.conv);
        if (!conn) {
            std::cout << " cannot find connection,conv:" << pkt.conv << "\n";
        }
        else {
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
