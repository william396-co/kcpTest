#pragma once
#include <atomic>
#include <memory>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>

#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"

class Client
{
public:
    using MessageHandler = std::function<void(const char*, size_t)>;
    using ConnectedHandler = std::function<void(uint32_t)>;

    Client(const char * ip, uint16_t port );
    ~Client();

    void recv_work();
    void send_work();
    void input_work();// for client test

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
    bool running() const
    {
        return is_running.load();
    }
    void set_index( int idx_ )
    {
        idx = idx_;
    }
    void set_message_handler(MessageHandler handler)
    {
        std::lock_guard lock(handler_mtx);
        message_handler = std::move(handler);
    }
    void set_connected_handler(ConnectedHandler handler)
    {
        std::lock_guard lock(handler_mtx);
        connected_handler = std::move(handler);
    }
    void send_async(const char* data, size_t len);

private:
    void send_test_payload( const char * data, size_t len );
    void handle_kcp_payload( const char * data, size_t len );
    void enqueue_payload(const char* data, size_t len);
public:
    void parse_udp_data(const char* buf, size_t len);// parse data from plain udp
        
    void send_shakehand();     // first send shakehand
    void recv_shakehand(uint32_t conv);// recieve shakehand from server

    void keepAlive();
    void sendPing();
private:
    std::unique_ptr<UdpSocket> socket;
    ikcpcb* kcp{};
    std::mutex kcp_mtx;
    std::mutex send_q_mtx;
    std::queue<std::string> send_queue;
    std::mutex handler_mtx;
    MessageHandler message_handler;
    ConnectedHandler connected_handler;
    int md;
    int str_max_len;
    bool auto_test = false;
    uint32_t test_count = 10;
    int lost_rate = 0;
    int send_interval = 20;
    std::atomic<uint32_t> sn{ 0 };
    uint32_t next = 1;
    uint32_t sumrtt = 0;
    uint32_t count = 0;
    uint32_t maxrtt = 0;
    bool show_info = false;
    std::atomic_bool is_running{ true };
    int idx = 0;
    std::atomic_bool handshake_done{ false };
    std::atomic<time_t> last_recv_ms{ 0 };
    std::atomic<time_t> last_send_ms{ 0 };
};

int32_t kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);
