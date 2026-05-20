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
    char buff[BUFFER_SIZE] = {};
    ( (uint32_t *)buff )[0] = sn++;
    ( (uint32_t *)buff )[1] = util::iclock();
    ( (uint32_t *)buff )[2] = (uint32_t)len;

    if ( show_info ) {
        printf( "Send idx:%u sn:%u size:%llu content {%s}\n", idx, sn - 1, size_t( len + 12 ), data );
    } else {
        printf( "Send idx:%u sn:%u size:%llu\n", idx, sn - 1, size_t( len + 12 ) );
    }
    memcpy( &buff[12], data, len );
    ikcp_send( kcp, buff, int(len + 12) );
    ikcp_update( kcp, util::iclock() );
}

void Client::recv( const char * data, size_t len )
{
    char * ptr_ = (char *)data;
    while ( size_t( ptr_ - data ) < len ) {
        uint32_t sn_ = *(uint32_t *)( ptr_ );
        uint32_t ts_ = *(uint32_t *)( ptr_ + 4 );
        uint32_t sz_ = *(uint32_t *)( ptr_ + 8 ) + 12;

        uint32_t rtt_ = util::iclock() - ts_;
        if ( sn_ != (uint32_t)next ) {
            printf( "ERROR sn %u<-> next=%d\n", sn, next );
            is_running = false;
        }
        ++next;
        sumrtt += rtt_;
        ++count;
        maxrtt = rtt_ > maxrtt ? rtt_ : maxrtt;

        if ( show_info )
            printf( "[RECV] idx:%u mode=%d sn:%d rrt:%d size:%u  content: {%s}\n", idx, md, sn_, rtt_, sz_, (char *)&ptr_[12] );
        else
            printf( "[RECV] idx:%u mode=%d sn:%d rrt:%d size:%u \n", idx, md, sn_, rtt_, sz_ );

        if ( next >= test_count ) {
            printf( "Finished %d times test\n", test_count );
            is_running = false;
        }
        ptr_ += sz_;
    }
}

void Client::recv_data(const char* buf, size_t len)
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
        ikcp_input(kcp, pkt.payload, pkt.size);
        // pass pkt.payload / pkt.size to ikcp_input()
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
    ikcp_setoutput(kcp, util::kcp_output);
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
        recv_data(socket->getRecvBuffer(), socket->getRecvSize());
		
      
        std::memset(buff, 0, sizeof(buff));
        int rc = ikcp_recv( kcp, buff, sizeof( buff ) );
        if ( rc < 0 ) continue;
        recv( buff, rc );
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
