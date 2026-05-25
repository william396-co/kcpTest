#pragma once

#include <memory>
#include <queue>
#include <mutex>

#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

class Connection
{
public:
    Connection( UdpSocket* socket, const char * remote_ip, uint16_t remote_port, uint32_t conv );
    ~Connection();

    void setmode( int mode )
    {
        util::ikcp_set_mode( kcp, mode );
        md = mode;
    }

    void update();
    void push_snd_queu(const char* data, size_t len);
    void push_rcv_queue(const char* data, size_t len);

    void recv_data(const char * data, size_t len );
    void send_data(const char* data, size_t len);

    void send_shakehand_reply();

	uint32_t getConv()const { return kcp ? kcp->conv : 0; }
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
    std::string remoteIp{};
    uint16_t remotePort{};
    int md;
    bool show_data = false;
    uint32_t sn{};

    char recvBuff[BUFFER_SIZE] = {};
    char sendBuff[BUFFER_SIZE] = {}; 

    // connection state 
    time_t last_recv_ms{};
    time_t last_send_ms{};
    time_t created_ms{};
    bool handshake_done{};
    bool closing{};
};

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);