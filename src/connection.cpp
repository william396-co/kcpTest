#include "connection.h"

#include <stdexcept>
#include <cstring>

Connection::Connection(UdpSocket* socket, const char* remote_ip, uint16_t remote_port, uint32_t conv)
    : socket{ socket },  kcp {}, remoteIp{ remote_ip }, remotePort{ remote_port }, md{ 0 }
{
    kcp = ikcp_create( conv, this->socket );
    ikcp_setoutput( kcp, util::kcp_output );

	//util::ikcp_set_log(kcp, IKCP_LOG_INPUT | IKCP_LOG_OUTPUT);
}

Connection::~Connection()
{
    ikcp_release( kcp );
}

void Connection::setlostrate( int lostrate )
{
    socket->setLostrate( lostrate );
}

void Connection::update()
{
    ikcp_update( kcp, util::iclock() );
}

void Connection::recv(const char* data, size_t len)
{
    ikcp_input(kcp, data, (long)len);

    char buff[BUFFER_SIZE];
	std::memset(buff, 0, sizeof(buff));
    int rc = ikcp_recv( kcp, buff, sizeof( buff ) );
    if ( rc < 0 ) return;

    IUINT32 sn_ = *(IUINT32 *)( buff );
    //time_t timestamp = *(int*)(buff[4]);
    uint32_t sz_ = *(uint32_t *)( buff + 8 );

    if ( show_data )
        printf( "RECV mode=%d [%s:%d], sn:[%d] sz:[%u] string is:{ %s}\n", md, socket->getRemoteIp(), socket->getRemotePort(), sn_, sz_, &buff[12] );

    else
        printf( "RECV mode=%d [%s:%d], sn:[%d] sz:[%u]\n", md, socket->getRemoteIp(), socket->getRemotePort(), sn_, sz_ );
    

    // send data back to client
    ikcp_send( kcp, buff, rc );
    ikcp_update( kcp, util::iclock() );
}

void Connection::send(const char* data, size_t len)
{
	socket->send(data, len, remoteIp.c_str(), remotePort,kcp->conv,PKT_KCP_DATA);
	std::cout << "conv: " << kcp->conv << " datasize: " << len << " rempteIP: " << remoteIp << " remotePort: " << remotePort << "\n";
}

void Connection::send_shakehand_reply()
{
	socket->send((const char*)&kcp->conv, sizeof(kcp->conv), kcp->conv, PKT_HANDSHAKE_ACK);
}

