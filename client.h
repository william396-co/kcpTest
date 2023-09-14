#pragma once
#include <memory>
#include <iostream>
#include <string>

#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"

extern bool is_running;

class Client
{
public:
    Client( const char * ip, uint16_t port, uint32_t conv );
    ~Client();

    void run();
    void input();
    void auto_input();

    void setmode( int mode );
    void setauto( bool _auto, uint32_t test_times = 100, size_t max_len = 2000 )
    {
        auto_test = _auto;
        test_count = test_times;
        str_max_len = max_len;
    }
    void setlostrate( int lostrate )
    {
        lost_rate = lostrate;
        client->setLostrate( lostrate / 2 );
    }
    void setsendinterval( int sendinterval )
    {
        send_interval = sendinterval;
    }

private:
    std::unique_ptr<UdpSocket> client;
    ikcpcb * kcp;
    int md;
    size_t str_max_len;
    bool auto_test = false;
    uint32_t test_count = 10;
    int lost_rate = 0;
    int send_interval = 20;
};
