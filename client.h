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
    void setauto( bool _auto, uint32_t count = 100 )
    {
        auto_test = _auto;
        test_count = count;
    }

private:
    std::unique_ptr<UdpSocket> client;
    ikcpcb * kcp;
    int md;
    bool auto_test = false;
    uint32_t test_count = 10;
};
