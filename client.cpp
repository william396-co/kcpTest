#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <queue>
#include <string>
#include <cstdlib>
#include <vector>
#include "src/random_util.h"

void rand_str( std::string & str, size_t max = 2000 )
{
    // Generate a random payload so the sample can exercise KCP with variable-sized messages.
    auto sz = random( 8, (int)max );
    str = random_string( sz );
}

Client::Client(const char* ip, uint16_t port)
    : socket { nullptr }, md { 0 }
{
    // The client uses one connected UDP socket and one KCP session after handshake completes.
    socket = std::make_unique<UdpSocket>();
    socket->setNonblocking();
    if (!socket->connect(ip, port)) {
        throw std::runtime_error( "socket connect failed" );
    }
}

Client::~Client()
{
    std::lock_guard lock(kcp_mtx);
    if (kcp) {
        ikcp_release(kcp);
        kcp = nullptr;
    }
}

void Client::setmode( int mode )
{       
    md = mode;
}

void Client::send_async(const char* data, size_t len)
{
    enqueue_payload(data, len);
}

void Client::parse_udp_data(const char* buf, size_t len)
{
    // First decode the outer UDP packet header, then hand the payload to the right client path.
    DecodedPacket pkt;
    if (!decode_packet(buf, len, pkt)) {
        return;
    }

    switch (pkt.type) {
    case PacketType::PKT_HANDSHAKE_ACK:
        // The server assigns conv during handshake; create the client-side KCP state here.
        recv_shakehand(pkt.conv);
        break;
    case PacketType::PKT_KCP_DATA:
    {
        std::vector<std::string> messages;
        {
            std::lock_guard lock(kcp_mtx);
            if (!kcp) {
                return;
            }

            last_recv_ms = util::now_ms();
            ikcp_input(kcp, pkt.payload, pkt.size);

            while (true) {
                char recvBuff[BUFFER_SIZE] = {};
                int rc = ikcp_recv(kcp, recvBuff, sizeof(recvBuff));
                if (rc < 0) {
                    break;
                }
                messages.emplace_back(recvBuff, recvBuff + rc);
            }
        }

        for (auto& message : messages) {
            handle_kcp_payload(message.data(), message.size());
        }
        break;
    }
    case PacketType::PKT_HANDSHAKE_REQ:
        std::cerr << "invalid PacketType: " << pkt.type << "\n";
        break;
    }
}

void Client::send_shakehand()
{
    // Handshake is sent as a raw UDP packet before KCP exists.
    socket->send(nullptr,0,0,PKT_HANDSHAKE_REQ);
    last_send_ms = util::now_ms();
    std::cout << __func__ << "\n";
}

void Client::recv_shakehand(uint32_t conv)
{
    // Replace any previous session with the server-assigned conv.
    {
        std::lock_guard lock(kcp_mtx);
        if (kcp) {
            ikcp_release(kcp);
        }
        kcp = ikcp_create(conv, socket.get());
        ikcp_setoutput(kcp, kcp_output);
        util::ikcp_set_mode(kcp, md);
    }
    handshake_done = true;
    last_recv_ms = util::now_ms();
    sendPing();
	std::cout << __func__ << " conv:" << conv << "\n";

    ConnectedHandler handler_copy;
    {
        std::lock_guard lock(handler_mtx);
        handler_copy = connected_handler;
    }
    if (handler_copy) {
        handler_copy(conv);
    }
}

void Client::keepAlive()
{
    // KCP only keeps the session alive if we continue to send application traffic or pings.
    if (util::now_ms() - last_send_ms >= PINGT_INTERVAL) {
        sendPing();
    }
}

void Client::sendPing()
{
	constexpr auto PING_MSG = "PING";
    // Ping is just normal application data wrapped in the test message header.
	send_async(PING_MSG, sizeof(PING_MSG));
}

void Client::rand_send_work()
{   
    auto current_ = util::now_ms();
    while (is_running) 
    {        
        if (!kcp) {
            // Retry handshake until the server replies with a valid conv.
            send_shakehand();
            std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
            continue;
        }

        util::isleep( 1 );
        keepAlive();
        // Flush any queued user payloads into KCP on the same thread that owns the session.
        std::vector<std::string> pending;
        {
            std::lock_guard lock(send_q_mtx);
            while (!send_queue.empty()) {
                pending.push_back(std::move(send_queue.front()));
                send_queue.pop();
            }
        }
        {
            std::lock_guard lock(kcp_mtx);
            if (kcp) {
                for (auto& payload : pending) {
                    ikcp_send(kcp, payload.data(), static_cast<int>(payload.size()));
                }
                ikcp_update( kcp, util::iclock() );
            }
        }

        // auto input test
        if ( auto_test && util::now_ms() - current_ >= send_interval ) {
            if ( sn.load() >= test_count ) {
                printf( "finished auto send times=%d\n", sn.load() );
                std::lock_guard lock(kcp_mtx);
                if (kcp) {
                    ikcp_update( kcp, util::iclock() );
                }
                auto_test = false;
            }

            current_ = util::now_ms();
            std::string writeBuffer;
            rand_str( writeBuffer, str_max_len );
            if ( !writeBuffer.empty() ) {
                send_test_payload( writeBuffer.data(), writeBuffer.size() );
            }
        }
    };
}
void Client::send_work()
{
    while (is_running) {

        if (!kcp) {
            // Interactive send loop uses the same handshake retry path.
            send_shakehand();
            std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
            continue;
        }

        util::isleep(1);
        keepAlive();
        std::vector<std::string> pending;
        {
            std::lock_guard lock(send_q_mtx);
            while (!send_queue.empty()) {
                pending.push_back(std::move(send_queue.front()));
                send_queue.pop();
            }
        }
        {
            std::lock_guard lock(kcp_mtx);
            if (kcp) {
                for (auto& payload : pending) {
                    ikcp_send(kcp, payload.data(), static_cast<int>(payload.size()));
                }
                ikcp_update(kcp, util::iclock());
            }
        }
    }
}

void Client::input_work()
{
    std::string writeBuffer;
    while (is_running) {
        // Simple stdin loop for manual testing.
        printf("Please enter a string to send to server(%s:%d):\n", socket->getRemoteIp(), socket->getRemotePort());

        writeBuffer.clear();
        std::getline(std::cin, writeBuffer);
        if (!writeBuffer.empty()) {
            send_test_payload(writeBuffer.data(), writeBuffer.size());
        }
    }
}

void Client::recv_work()
{
    while ( is_running ) {

        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });

        // Poll the UDP socket, then route the decoded packet based on its type.
        if ( socket->recv() < 0 ) {
            continue;
        }
        parse_udp_data(socket->getRecvBuffer(), socket->getRecvSize());
    }

    /* summary */
    if ( count > 0 )
        printf( "\nIDX=[%u] MODE=[%d] DATASIZE=[%d] LOSTRATE=[%d] avgrtt=%d maxrtt=%d count=%d \n",
            idx,
            md,
            str_max_len,
            lost_rate,
            int( sumrtt / count ),
            maxrtt,
            count );
}

void Client::send_test_payload(const char* data, size_t len)
{
    if (!handshake_done) {
        return;
    }

    const auto next_sn = sn.fetch_add(1) + 1;
    // Prep the sample message header used for ordering and RTT measurement.
    std::string payload = encode_app_message(next_sn, util::now_ms(), data, static_cast<uint32_t>(len));

    if ( show_info ) {
        printf( "Send idx:%u sn:%u size:%llu content {%s}\n", idx, next_sn, static_cast<unsigned long long>(payload.size()), data );
    } else {
        printf( "Send idx:%u sn:%u size:%llu\n", idx, next_sn, static_cast<unsigned long long>(payload.size()) );
    }

    enqueue_payload(payload.data(), payload.size());
}

void Client::handle_kcp_payload(const char* data, size_t len)
{
    // Allow callers to override message handling; otherwise parse the sample header and print stats.
    MessageHandler handler_copy;
    {
        std::lock_guard lock(handler_mtx);
        handler_copy = message_handler;
    }
    if (handler_copy) {
        handler_copy(data, len);
        return;
    }

    DecodedAppMessage message{};
    if (!decode_app_message(data, len, message)) {
        return;
    }

    uint32_t rtt_ = util::iclock() - static_cast<uint32_t>(message.ts);
    if (message.sn != next) {
        printf("ERROR sn %u<-> next=%d\n", message.sn, next);
        is_running = false;
    }
    ++next;
    sumrtt += rtt_;
    ++count;
    maxrtt = rtt_ > maxrtt ? rtt_ : maxrtt;

    if (show_info) {
        printf("RECV mode=%d [%s:%d], sn:[%u] sz:[%u] string is:{ %s}\n",
            md,
            socket->getRemoteIp(),
            socket->getRemotePort(),
            message.sn,
            message.size,
            message.payload);
    } else {
        printf("RECV mode=%d [%s:%d], sn:[%u] sz:[%u]\n",
            md,
            socket->getRemoteIp(),
            socket->getRemotePort(),
            message.sn,
            message.size);
    }

    if (next >= test_count) {
        printf("Finished %d times test\n", test_count);
        is_running = false;
    }
}

void Client::enqueue_payload(const char* data, size_t len)
{
    if (!handshake_done) {
        return;
    }

    // Queue data so the worker thread can own the KCP session and flush it safely.
    last_send_ms = util::now_ms();
    std::lock_guard lock(send_q_mtx);
    send_queue.push(std::string(data, data + len));
}

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    UdpSocket* s = (UdpSocket*)user;
    if (s)
        return s->send(buf, len, kcp->conv, PKT_KCP_DATA);
    return -1;
}
