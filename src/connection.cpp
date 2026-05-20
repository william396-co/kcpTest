#include "connection.h"

#include <stdexcept>
#include <cstring>

Connection::Connection(UdpSocket* socket, const char* remote_ip, uint16_t remote_port, uint32_t conv)
    : socket{ socket },  kcp {}, remoteIp{ remote_ip }, remotePort{ remote_port }, md{ 0 }
{
	kcp = ikcp_create(conv, this);
	ikcp_setoutput(kcp, kcp_output);

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

void Connection::recv_data(const char* data, size_t len)
{
    ikcp_input(kcp, data, (long)len);

	std::memset(recvBuff, 0, sizeof(recvBuff));    
    int rc = ikcp_recv( kcp, recvBuff, sizeof(recvBuff) );
    if ( rc < 0 ) return;

    MsgHeader* msgHeader = (MsgHeader*)recvBuff;
    msgHeader->sn = ntohl(msgHeader->sn);
    msgHeader->ts = ntohll(msgHeader->ts);
    msgHeader->sz = ntohl(msgHeader->sz);

    if ( show_data )
		printf("RECV mode=%d [%s:%d], sn:[%d] sz:[%u] string is:{ %s}\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz, (char*)(recvBuff + MsgHeaderSize));

    else
        printf( "RECV mode=%d [%s:%d], sn:[%d] sz:[%u]\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz );
    

    // send data back to client
    send_data(recvBuff + MsgHeaderSize, msgHeader->sz);
}

void Connection::send_data(const char* data, size_t len)
{
    std::memset(sendBuff, 0, sizeof(sendBuff));
    MsgHeader* msgHeader = (MsgHeader*)sendBuff;
	msgHeader->sn = htonl(sn++);
    msgHeader->ts = htonll(util::now_ms());
    msgHeader->sz = len;
    auto totalSize = len + MsgHeaderSize;
    
    memcpy(sendBuff + MsgHeaderSize, data, len);
    ikcp_send(kcp, sendBuff, totalSize);
    ikcp_update(kcp, util::iclock());
}


int Connection::send(const char* data, int len)
{
    std::cout << "conv: " << kcp->conv << " datasize: " << len << " rempteIP: " << remoteIp << " remotePort: " << remotePort << "\n";
    return socket->send(data, len, remoteIp.c_str(), remotePort, kcp->conv, PKT_KCP_DATA);
}

void Connection::send_shakehand_reply()
{
	socket->send((const char*)&kcp->conv, sizeof(kcp->conv), kcp->conv, PKT_HANDSHAKE_ACK);
}


int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    Connection* con = (Connection*)user;
    if (con)
        return con->send(buf, len);
    return -1;
}