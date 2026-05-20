#pragma once
#include <memory>
#include <iostream>
#include <string>

#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"

class Client
{
public:
    Client(const char * ip, uint16_t port );
    ~Client();

    void recv_work();
    void send_work();

    void rand_send_work();// for multiple client

    void setmode( int mode );
    void setauto( bool _auto, int test_times = 100, int max_len = 2000 )
    {
        auto_test = _auto;
        test_count = test_times;
        str_max_len = max_len;
    }
    void setlostrate( int lostrate )
    {
        lost_rate = lostrate;
        socket->setLostrate( lostrate / 2 );
    }
    void setsendinterval( int sendinterval )
    {
        send_interval = sendinterval;
    }
    void set_show_info( bool show_ )
    {
        show_info = show_;
    }
    void terminate()
    {
        is_running = false;
    }
    void set_index( int idx_ )
    {
        idx = idx_;
    }

private:
    void send( const char * data, size_t len );
    void recv( const char * data, size_t len );
public:
    void parse_udp_data(const char* buf, size_t len);// parse data from plain udp
        
    void send_shakehand();     // first send shakehand
    void recv_shakehand(uint32_t conv);// recieve shakehand from server

private:
    std::unique_ptr<UdpSocket> socket;
    ikcpcb* kcp{};
    int md;
    int str_max_len;
    bool auto_test = false;
    uint32_t test_count = 10;
    int lost_rate = 0;
    int send_interval = 20;
    uint32_t sn = 0;
    uint32_t next = 0;
    uint32_t sumrtt = 0;
    uint32_t count = 0;
    uint32_t maxrtt = 0;
    bool show_info = false;
    bool is_running = true;
    int idx = 0;

    char sendBuff[BUFFER_SIZE] = {};
    char recvBuff[BUFFER_SIZE] = {};
};

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);