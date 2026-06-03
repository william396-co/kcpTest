#include "connection.h"

#include <stdexcept>
#include <cstring>
#include <utility>
#include <vector>

Connection::Connection(UdpSocket* socket, UdpEndpoint remote_endpoint, uint32_t conv)
    : socket{ socket },  kcp {}, remoteEndpoint{ std::move(remote_endpoint) }, md{ 0 }
{
    // One Connection represents one client-side UDP endpoint plus one KCP state machine.
	kcp = ikcp_create(conv, this);
	ikcp_setoutput(kcp, kcp_output);

    last_recv_ms = util::now_ms();
    created_ms = util::now_ms();
	//util::ikcp_set_log(kcp, IKCP_LOG_INPUT | IKCP_LOG_OUTPUT);
}

Connection::~Connection()
{
    std::cout << __func__ << " conv: " << kcp->conv << "\n";
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
    return closing || util::now_ms() - last_recv_ms > PINGT_INTERVAL;
}

void Connection::update()
{
    std::vector<std::string> recv_batch;
    {
        // Drain queued UDP payloads first so inbound KCP processing stays single-threaded.
        std::lock_guard lock(rcv_q_mtx);
        while (!recv_queue.empty()) {
            recv_batch.push_back(std::move(recv_queue.front()));
            recv_queue.pop();
        }
    }
    for (auto& payload : recv_batch) {
        recv_data(payload.data(), payload.size());
    }

    std::vector<std::string> send_batch;
    {
        // Drain queued application sends next and hand them to KCP for segmentation/output.
        std::lock_guard lock(snd_q_mtx);
        while (!send_queue.empty()) {
            send_batch.push_back(std::move(send_queue.front()));
            send_queue.pop();
        }
    }
    for (auto& payload : send_batch) {
        send_data(payload.data(), payload.size());
    }

    // Advance KCP timers once per worker tick.
    ikcp_update(kcp, util::iclock());
}

void Connection::push_snd_queu(const char* data, size_t len)
{
    std::lock_guard lock(snd_q_mtx);
    // Keep the payload alive until the worker thread is ready to send it.
    send_queue.push(std::string(data, data + len));
}

void Connection::push_snd_queu(std::string data)
{
    std::lock_guard lock(snd_q_mtx);
    send_queue.push(std::move(data));
}

void Connection::push_rcv_queue(const char* data, size_t len)
{
    std::lock_guard lock(rcv_q_mtx);
    // Defer KCP input so the UDP receive thread only copies bytes and returns quickly.
    recv_queue.push(std::string(data, data + len));
}

void Connection::recv_data(const char* data, size_t len)
{
    last_recv_ms = util::now_ms();
    handshake_done = true;

    // Feed one raw KCP segment into the state machine.
    ikcp_input(kcp, data, (long)len);

    char recvBuff[BUFFER_SIZE] = {};
    while (true) {
        std::memset(recvBuff, 0, sizeof(recvBuff));
        int rc = ikcp_recv(kcp, recvBuff, sizeof(recvBuff));
        if (rc < 0) {
            return;
        }

        if (show_data) {
            // This project treats the reassembled KCP payload as a simple binary message.
            printf("RECV mode=%d [%s:%d], size:[%d]\n",
                md,
                remoteEndpoint.ip.c_str(),
                remoteEndpoint.port,
                rc);
        }

        if (payload_dispatch_handler) {
            payload_dispatch_handler(*this, recvBuff, static_cast<size_t>(rc));
        }
    }
}

void Connection::send_data(const char* data, size_t len)
{
    last_send_ms = util::now_ms();
    // Application bytes are passed to KCP; the actual UDP send happens in kcp_output().
    ikcp_send(kcp, data, static_cast<int>(len));
}


int Connection::send(const char* data, int len)
{
    std::cout << "conv: " << kcp->conv << " datasize: " << len << " remoteIP: " << remoteEndpoint.ip << " remotePort: " << remoteEndpoint.port << "\n";
    return socket->send_to(remoteEndpoint, data, len, kcp->conv, PKT_KCP_DATA);
}

void Connection::send_shakehand_reply()
{
    last_recv_ms = util::now_ms();
    // Handshake ACK carries the server-assigned conv so the client can create matching KCP state.
	socket->send_to(remoteEndpoint, nullptr, 0, kcp->conv, PKT_HANDSHAKE_ACK);
}


int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    Connection* con = (Connection*)user;
    if (con)
        return con->send(buf, len);
    return -1;
}
