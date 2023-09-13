#include "connection.h"

#include <stdexcept>
#include <cstring>

Connection::Connection( uint16_t local_port, const char * remote_ip, uint16_t remote_port, uint32_t conv )
    : socket { nullptr }, kcp { nullptr }, md { 0 }
{
    socket = std::make_unique<UdpSocket>();
    socket->setNonblocking();
    if ( !socket->bind( local_port ) ) {
        throw std::runtime_error( "connection bind failed" );
    }
    if ( !socket->connect( remote_ip, remote_port ) ) {
        throw std::runtime_error( "connection connect failed" );
    }

    kcp = ikcp_create( conv, socket.get() );
    ikcp_setoutput( kcp, util::kcp_output );
}

Connection::~Connection()
{
    ikcp_release( kcp );
}

void Connection::update()
{
    ikcp_update( kcp, util::iclock() );
    if ( socket->recv() > 0 ) {
        recv_data( socket->getRecvBuffer(), socket->getRecvSize() );
    }
}

void Connection::recv_data( const char * data, size_t len )
{
    ikcp_input( kcp, data, len );

    char buff[BUFFER_SIZE];
    bzero( buff, sizeof( buff ) );
    int rc = ikcp_recv( kcp, buff, sizeof( buff ) );
    if ( rc < 0 ) return;

    IUINT32 sn = *(IUINT32 *)( buff );

    if ( show_data )
        printf( "RECV [%s:%d] -> [ %s:%d], sn:[%d] string is:{ %s}\n",
            socket->getRemoteIp(),
            socket->getRemotePort(),
            socket->getLocalIp(),
            socket->getLocalPort(),
            sn + 1,
            &buff[8] );

    else
        printf( "RECV [%s:%d] -> [ %s:%d], sn:[%d]  ts{ %ld}\n",
            socket->getRemoteIp(),
            socket->getRemotePort(),
            socket->getLocalIp(),
            socket->getLocalPort(),
            sn + 1,
            util::now_ms() );

    ikcp_send( kcp, buff, rc );
    ikcp_update( kcp, util::iclock() );
}

