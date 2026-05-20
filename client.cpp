#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "src/random_util.h"

void rand_str( std::string & str, size_t max = 2000 )
{
    auto sz = random( 8, (int)max );
    str = random_string( sz );
}

Client::Client(const char* ip, uint16_t port)
    : socket { nullptr }, md { 0 }
{
    socket = std::make_unique<UdpSocket>();
    socket->setNonblocking();
    if (!socket->connect(ip, port)) {
        throw std::runtime_error( "socket connect failed" );
    }
}

Client::~Client()
{
    if (kcp) {
        ikcp_release(kcp);
    }
}

void Client::setmode( int mode )
{   
    
    md = mode;
}

void Client::send( const char * data, size_t len )
{
    // use kcp send data to endpoint
	MsgHeader* msgHeader = (MsgHeader*)sendBuff;
	msgHeader->sn = htonl(++sn);
    msgHeader->ts = htonll(util::now_ms());    
	msgHeader->sz = htonl(len);

    auto totalSize = len + MsgHeaderSize;

    if ( show_info ) {
        printf( "Send idx:%u sn:%u size:%llu content {%s}\n", idx, sn, totalSize, data );
    } else {
        printf( "Send idx:%u sn:%u size:%llu\n", idx, sn, totalSize );
    }
	memcpy(sendBuff + MsgHeaderSize, data, len);
    ikcp_send( kcp, sendBuff, totalSize);
    ikcp_update( kcp, util::iclock() );
}

void Client::recv( const char * data, size_t len )
{
    ikcp_input(kcp, data, len);

    std::memset(recvBuff, 0, sizeof(recvBuff));
    int rc = ikcp_recv(kcp, recvBuff, sizeof(recvBuff));
    if (rc < 0) return;

    char* ptr_ = (char*)recvBuff;
    while (size_t(ptr_ - recvBuff) < len) {

        MsgHeader* msgHeader = (MsgHeader*)ptr_;
        msgHeader->sn = ntohl(msgHeader->sn);
        msgHeader->ts = ntohll(msgHeader->ts);
        msgHeader->sz = ntohl(msgHeader->sz);

        uint32_t rtt_ = util::iclock() - msgHeader->ts;
        if (msgHeader->sn != (uint32_t)next) {
            printf("ERROR sn %u<-> next=%d\n", sn, next);
            is_running = false;
        }
        ++next;
        sumrtt += rtt_;
        ++count;
        maxrtt = rtt_ > maxrtt ? rtt_ : maxrtt;


        if (show_info)
            printf("RECV mode=%d [%s:%d], sn:[%d] sz:[%u] string is:{ %s}\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz, (char*)(recvBuff + MsgHeaderSize));

        else
            printf("RECV mode=%d [%s:%d], sn:[%d] sz:[%u]\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz);

        if (next >= test_count) {
            printf("Finished %d times test\n", test_count);
            is_running = false;
        }
        ptr_ += msgHeader->sz;
    }
}

void Client::parse_udp_data(const char* buf, size_t len)
{
    DecodedPacket pkt;
    if (!decode_packet(buf, len, pkt)) {
        return;
    }

    switch (pkt.type) {
    case PacketType::PKT_HANDSHAKE_ACK:
        // client: create kcp with pkt.conv
        recv_shakehand(pkt.conv);
        break;
    case PacketType::PKT_KCP_DATA:
        // pass pkt.payload / pkt.size to ikcp_input()
        recv(pkt.payload, pkt.size);
        break;
    case PacketType::PKT_HANDSHAKE_REQ:
        std::cerr << "invalid PacketType: " << pkt.type << "\n";
        break;
    }
}

void Client::send_shakehand()
{
    // use socket direct send
    socket->send(nullptr,0,0,PKT_HANDSHAKE_REQ);
    std::cout << __PRETTY_FUNCTION__ << "\n";
}

void Client::recv_shakehand(uint32_t conv)
{
    kcp = ikcp_create(conv, socket.get());
    ikcp_setoutput(kcp, kcp_output);
    util::ikcp_set_mode(kcp, md);
   // util::ikcp_set_log(kcp, IKCP_LOG_INPUT | IKCP_LOG_OUTPUT);
	std::cout << __PRETTY_FUNCTION__ << "conv:" << conv << "\n";
}

void Client::rand_send_work()
{   
    auto current_ = util::now_ms();
    while (is_running) 
    {        
        if (!kcp) {
            send_shakehand();
            std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
            continue;
        }

        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // auto input test
        if ( auto_test && util::now_ms() - current_ >= send_interval ) {
            if ( sn >= test_count ) {
                printf( "finished auto send times=%d\n", sn );
                ikcp_update( kcp, util::iclock() );
                auto_test = false;
            }

            current_ = util::now_ms();
            std::string writeBuffer;
            rand_str( writeBuffer, str_max_len );
            if ( !writeBuffer.empty() ) {
                send( writeBuffer.data(), writeBuffer.size() );
            }
        }
    };
}
void Client::send_work()
{
    std::string writeBuffer;
    while (is_running) {

        if (!kcp) {
            send_shakehand();
            std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
            continue;
        }

        util::isleep(1);
        ikcp_update(kcp, util::iclock());

        printf("Please enter a string to send to server(%s:%d):\n", socket->getRemoteIp(), socket->getRemotePort());

        writeBuffer.clear();
        std::getline(std::cin, writeBuffer);
        if (!writeBuffer.empty()) {
            send(writeBuffer.data(), writeBuffer.size());
        }
    }
}

void Client::recv_work()
{
    char buff[BUFFER_SIZE];
    while ( is_running ) {

        if (kcp) {
            ikcp_update(kcp, util::iclock());
        }

        // recv pack
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

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    UdpSocket* s = (UdpSocket*)user;
    if (s)
        return s->send(buf, len, kcp->conv, PKT_KCP_DATA);
    return -1;
}
