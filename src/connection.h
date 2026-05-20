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

    void recv(const char * data, size_t len );
    void send(const char* data, size_t len);
    void send_shakehand_reply();

    void set_show( bool _b ) { show_data = _b; }
    void setlostrate( int lostrate );

private:    
    UdpSocket* socket{};
    ikcpcb* kcp{};
    std::string remoteIp{};
    uint16_t remotePort{};
    int md;
    bool show_data = false;
};
