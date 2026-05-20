#pragma once

#include <memory>

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

    void recv_data(const char * data, size_t len );
    void send_data(const char* data, size_t len);

    void send_shakehand_reply();
public:
    int send(const char* data, int len);

    void set_show( bool _b ) { show_data = _b; }
    void setlostrate( int lostrate );

private:    
    UdpSocket* socket{};
    ikcpcb* kcp{};
    std::string remoteIp{};
    uint16_t remotePort{};
    int md;
    bool show_data = false;
    uint32_t sn{};

    char recvBuff[BUFFER_SIZE] = {};
    char sendBuff[BUFFER_SIZE] = {};
};

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);