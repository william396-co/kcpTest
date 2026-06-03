#pragma once

#include <memory>
#include <functional>
#include <mutex>
#include <queue>

#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

class Connection
{
public:
    // Internal hook used by the server to dispatch a reassembled payload.
    using PayloadDispatchHandler = std::function<void(Connection&, const char*, size_t)>;

    Connection( UdpSocket* socket, UdpEndpoint remote_endpoint, uint32_t conv );
    ~Connection();

    void setmode( int mode )
    {
        util::ikcp_set_mode( kcp, mode );
        md = mode;
    }

    void set_payload_dispatch_handler(PayloadDispatchHandler handler)
    {
        payload_dispatch_handler = std::move(handler);
    }

    void update();
    // Queue data so the worker thread can feed it into KCP without blocking the UDP receive thread.
    void push_snd_queu(const char* data, size_t len);
    void push_snd_queu(std::string data);
    // Queue inbound KCP bytes for later processing on the shard worker thread.
    void push_rcv_queue(const char* data, size_t len);

    void recv_data(const char * data, size_t len );
    void send_data(const char* data, size_t len);

    // Reply to the handshake request with the assigned conv.
    void send_shakehand_reply();

	uint32_t getConv()const { return kcp ? kcp->conv : 0; }
    const UdpEndpoint& getEndpoint() const { return remoteEndpoint; }
public:
    int send(const char* data, int len);

    void set_show( bool _b ) { show_data = _b; }
    void setlostrate( int lostrate );

    bool dead_link()const;
    bool handshake_overtime()const;
    bool deactive()const;
    void set_close() { closing = true; }
private:
    std::mutex snd_q_mtx;
    std::queue<std::string> send_queue;
    std::mutex rcv_q_mtx;
    std::queue<std::string> recv_queue;
    UdpSocket* socket{};
    ikcpcb* kcp{};
    UdpEndpoint remoteEndpoint{};
    PayloadDispatchHandler payload_dispatch_handler;
    int md;
    bool show_data = false;

    // connection state 
    time_t last_recv_ms{};
    time_t last_send_ms{};
    time_t created_ms{};
    bool handshake_done{};
    bool closing{};
};

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);
