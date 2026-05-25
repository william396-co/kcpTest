#include "connection.h"

#include <stdexcept>
#include <cstring>

Connection::Connection(UdpSocket* socket, const char* remote_ip, uint16_t remote_port, uint32_t conv)
    : socket{ socket },  kcp {}, remoteIp{ remote_ip }, remotePort{ remote_port }, md{ 0 }
{
	kcp = ikcp_create(conv, this);
	ikcp_setoutput(kcp, kcp_output);

    last_recv_ms = util::now_ms();
    created_ms = util::now_ms();
	//util::ikcp_set_log(kcp, IKCP_LOG_INPUT | IKCP_LOG_OUTPUT);
}

Connection::~Connection()
{
    std::cout << __PRETTY_FUNCTION__ << " conv: " << kcp->conv << "\n";
    ikcp_release( kcp );
}

void Connection::setlostrate( int lostrate )
{
    socket->setLostrate( lostrate );
}

bool Connection::dead_link() const
{ 
    return kcp ? kcp->state == (uint32_t)-1 : false; 
}

bool Connection::handshake_overtime() const
{
    return !handshake_done && util::now_ms() - created_ms > HANSHAKE_TIME;
}

bool Connection::deactive() const
{
    return util::now_ms() - last_recv_ms > PINGT_INTERVAL;
}

void Connection::update()
{
    {
        // consume recv_queue
        std::lock_guard lock(rcv_q_mtx);
        while (!recv_queue.empty()) {
            auto q = recv_queue.front();
            recv_queue.pop();
            recv_data(q.data(), q.size());
        }
    }
    {
        // consume send_queue
        std::lock_guard lock(snd_q_mtx);
        while (!send_queue.empty()) {
            auto q = send_queue.front();
            send_queue.pop();
            send_data(q.data(), q.size());
        }
    }  
}

void Connection::push_snd_queu(const char* data, size_t len)
{
    std::lock_guard lock(snd_q_mtx);
    send_queue.push(std::string(data, data + len));
}

void Connection::push_rcv_queue(const char* data, size_t len)
{
    std::lock_guard lock(rcv_q_mtx);
    recv_queue.push(std::string(data, data + len));
}

void Connection::recv_data(const char* data, size_t len)
{
    last_recv_ms = util::now_ms();
    handshake_done = true;

    ikcp_input(kcp, data, (long)len);

	std::memset(recvBuff, 0, sizeof(recvBuff));    
    int rc = ikcp_recv( kcp, recvBuff, sizeof(recvBuff) );
    if ( rc < 0 ) return;

    MsgHeader* msgHeader = (MsgHeader*)recvBuff;
    msgHeader->sn = ntohl(msgHeader->sn);
    msgHeader->ts = ntohll(msgHeader->ts);
    msgHeader->sz = ntohl(msgHeader->sz);


    // TODO app logic about message
    if ( show_data )
		printf("RECV mode=%d [%s:%d], sn:[%d] sz:[%u] string is:{ %s}\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz, (char*)(recvBuff + MsgHeaderSize));

    else
        printf( "RECV mode=%d [%s:%d], sn:[%d] sz:[%u]\n", md, socket->getRemoteIp(), socket->getRemotePort(), msgHeader->sn, msgHeader->sz );
    

    // send data back to client
    push_snd_queu(recvBuff + MsgHeaderSize, msgHeader->sz);    
}

void Connection::send_data(const char* data, size_t len)
{
    last_send_ms = util::now_ms();

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
    last_recv_ms = util::now_ms();
	socket->send((const char*)&kcp->conv, sizeof(kcp->conv), kcp->conv, PKT_HANDSHAKE_ACK);
}


int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    Connection* con = (Connection*)user;
    if (con)
        return con->send(buf, len);
    return -1;
}