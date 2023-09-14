#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "src/random_util.h"

static int g_sn = 0;

void rand_str( std::string & str, size_t max = 2000 )
{
    size_t sz = random( 8, max );
    str = random_string( sz );
}

Client::Client( const char * ip, uint16_t port, uint32_t conv )
    : client { nullptr }, md { 0 }
{
    client = std::make_unique<UdpSocket>();
    client->setNonblocking();
    if ( !client->connect( ip, port ) ) {
        throw std::runtime_error( "client connect failed" );
    }

    kcp = ikcp_create( conv, client.get() );
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

void Client::auto_input()
{
    std::string str;
    auto current = util::now_ms();
    while ( is_running ) {
        if ( !auto_test )
            break;
        util::isleep( 1 );

        if ( g_sn >= test_count ) {
            printf( "finished auto send times=%d\n", g_sn );
            break;
        }

        if ( util::now_ms() - current < 20 )
            continue;

        current = util::now_ms();
        rand_str( str, str_max_len );
        if ( !str.empty() ) {
            char buff[BUFFER_SIZE];
            ( (IUINT32 *)buff )[0] = g_sn++;
            ( (IUINT32 *)buff )[1] = util::iclock();

            if ( auto_test )
                printf( "[%ld]auto_input sn:%u size:%lu\n", util::now_ms(), g_sn, str.size() );
            else
                printf( "[%ld]auto_input sn:%u content:{%s}\n", util::now_ms(), g_sn, str.c_str() );

            memcpy( &buff[8], str.data(), str.size() );
            ikcp_send( kcp, buff, str.size() + 8 );
            ikcp_update( kcp, util::iclock() );
        }
    }
}

void Client::input()
{
    std::string writeBuffer;
    char buff[BUFFER_SIZE];
    while ( is_running ) {
        printf( "Please enter a string to send to server(%s:%d):", client->getRemoteIp(), client->getRemotePort() );

        writeBuffer.clear();
        bzero( buff, sizeof( buff ) );
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            ( (IUINT32 *)buff )[0] = g_sn++;
            ( (IUINT32 *)buff )[1] = util::iclock();

            memcpy( &buff[8], writeBuffer.data(), writeBuffer.size() );
            ikcp_send( kcp, buff, writeBuffer.size() + 8 );
            ikcp_update( kcp, util::iclock() );
        }
    }
}

void Client::run()
{
    char buff[BUFFER_SIZE];
    uint32_t next = 0;
    uint32_t sumrtt = 0;
    uint32_t count = 0;
    uint32_t maxrtt = 0;

    while ( is_running ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // udp pack received
        if ( client->recv() < 0 ) {
            continue;
        }
        ikcp_input( kcp, client->getRecvBuffer(), client->getRecvSize() );

        bzero( buff, sizeof( buff ) );
        // user/logic package received
        int rc = ikcp_recv( kcp, buff, BUFFER_SIZE );
        if ( rc < 0 )
            continue;

        IUINT32 sn = *(IUINT32 *)( buff + 0 );
        IUINT32 ts = *(IUINT32 *)( buff + 4 );
        IUINT32 rtt = util::iclock() - ts;

        if ( sn != next ) {
            printf( "ERROR sn %d<->%d\n", count, next );
            break;
        }
        ++next;
        sumrtt += rtt;
        ++count;
        maxrtt = rtt > maxrtt ? rtt : maxrtt;

        if ( !auto_test )
            printf( "[RECV] mode=%d sn:%d rrt:%d  content: {%s}\n", md, sn + 1, rtt, (char *)&buff[8] );
        else
            printf( "[RECV] mode=%d sn:%d rrt:%d\n", md, sn + 1, rtt );
        if ( next >= test_count ) break;
    }

    /* summary */
    if ( count > 0 )
        printf( "\n MODE=[%d] DATASIZE=[%d] LOSTRATE=[%d%] avgrtt=%d maxrtt=%d count=%d \n", md, str_max_len, lost_rate, int( sumrtt / count ), maxrtt, count );
}
