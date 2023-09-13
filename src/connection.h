#pragma once

#include <memory>

#include "util.h"
#include "udpsocket.h"
#include "ikcp.h"

class Connection
{
public:
    Connection( uint16_t local_port, const char * remote_ip, uint16_t remote_port, uint32_t conv );
    ~Connection();

    void setmode( int mode )
    {
        util::ikcp_set_mode( kcp, mode );
        md = mode;
    }

    void update();

    void recv_data( const char * data, size_t len );

    void set_show( bool _b ) { show_data = _b; }
    void setlostrate( int lostrate );

private:
    std::unique_ptr<UdpSocket> socket;
    ikcpcb * kcp;
    int md;
    bool show_data = false;
};
