#pragma once
#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"
#include "src/joining_thread.h"

#include <unordered_map>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <memory>

class Connection;
using ConnID = std::pair<std::string, uint16_t>;

namespace std {
template<>
struct hash<ConnID>
{
    size_t operator()( ConnID const & cid ) const
    {
        size_t val = 17;
        val = 31 * val + std::hash<std::string> {}( cid.first );
        val = 31 * val + std::hash<uint16_t> {}( cid.second );
        return val;
    }
};
} // namespace std

using ConnectionPtr = Connection*;
using ConnMap = std::unordered_map<uint32_t, ConnectionPtr>;
using ConnMapVec = std::vector<ConnMap>;

using ConnIDConvMap = std::unordered_map<ConnID, uint32_t>;// ConnID,conv

extern bool is_running;

class Server
{
public:
	Server(uint16_t port, uint32_t work_thread_cnt = 4);
    ~Server();

    bool startService();
    bool stopService();

    ConnectionPtr createConn(UdpSocket* socket, const char * ip, uint16_t port, uint32_t conv );

    ConnectionPtr findConn(uint32_t conv)const;

    ConnectionPtr findConn(const char* ip, uint16_t port)const;

    void recv_work();
    void send_work(int idx);
    void parse_udp_data(const char* buf, size_t len); // parse data from plain udp

    void setmode( int mode );
    void show_data( bool _show ) { show = _show; }

    void setlostrate( int lostrate ) { lost_rate = lostrate / 2; }

    uint32_t alloc_conv()const;
    
private:
    std::unique_ptr<UdpSocket> listen;
    std::vector<joining_thread> work_threads;
    joining_thread recv_thread;
    joining_thread send_thread;
    ConnMapVec conMapVec;
    ConnIDConvMap connIDConvMap;
    int md;
    uint16_t listen_port;
    bool show = false;
    int lost_rate = 0;
    const uint32_t work_thread_cnt{};
	mutable uint32_t nextConv{ 1000 };
    time_t last_reclaim_ms{};
};

