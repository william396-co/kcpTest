#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "src/random_util.h"

void rand_str( std::string & str, size_t max = 2000 )
{
    size_t sz = random( 8, max );
    str = random_string( sz );
}

Client::Client( const char * ip, uint16_t port, uint32_t conv )
    : socket { nullptr }, md { 0 }
{
    socket = std::make_unique<UdpSocket>();
    socket->setNonblocking();
    if ( !socket->connect( ip, port ) ) {
        throw std::runtime_error( "socket connect failed" );
    }

    kcp = ikcp_create( conv, socket.get() );
    ikcp_setoutput( kcp, util::kcp_output );
}

Client::~Client()
{
    ikcp_release( kcp );
}

void Client::setmode( int mode )
{
    util::ikcp_set_mode( kcp, mode );
    md = mode;
}

void Client::send( const char * data, size_t len )
{
    char buff[BUFFER_SIZE] = {};
    ( (uint32_t *)buff )[0] = sn++;
    ( (uint32_t *)buff )[1] = util::iclock();
    ( (uint32_t *)buff )[2] = (uint32_t)len;

    if ( show_info ) {
        printf( "Send idx:%u sn:%u size:%lu content {%s}\n", idx, sn - 1, size_t( len + 12 ), data );
    } else {
        printf( "Send idx:%u sn:%u size:%lu\n", idx, sn - 1, size_t( len + 12 ) );
    }
    memcpy( &buff[12], data, len );
    ikcp_send( kcp, buff, len + 12 );
    ikcp_update( kcp, util::iclock() );
}

void Client::input()
{
    std::string writeBuffer;
    while ( is_running ) {
        printf( "Please enter a string to send to server(%s:%d):\n", socket->getRemoteIp(), socket->getRemotePort() );

        writeBuffer.clear();
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            send( writeBuffer.data(), writeBuffer.size() );
        }
    }
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

void Client::run()
{
    auto current_ = util::now_ms();
    char buff[BUFFER_SIZE];
    while ( is_running ) {
        //  util::isleep( 1 );
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

        // recv pack
        if ( socket->recv() < 0 ) {
            continue;
        }
        ikcp_input( kcp, socket->getRecvBuffer(), socket->getRecvSize() );
        bzero( buff, sizeof( buff ) );
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
