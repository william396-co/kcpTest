#pragma once
#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"

#include <unordered_map>
#include <functional>
#include <string>
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

using ConnMap = std::unordered_map<uint32_t, Connection *>;

extern bool is_running;

class Server
{
public:
    Server(uint16_t port);
    ~Server();

    Connection * createConn(UdpSocket* socket, const char * ip, uint16_t port, uint32_t conv );

    Connection* findConn(uint32_t conv)const;

    void recv_work();
    void send_work();
    void recv_data(const char* buf, size_t len); // low leve recv

    void setmode( int mode );
    void show_data( bool _show ) { show = _show; }

    void setlostrate( int lostrate ) { lost_rate = lostrate / 2; }

    uint32_t alloc_conv()const;
private:
    std::unique_ptr<UdpSocket> listen;
    ConnMap connections;
    int md;
    uint16_t listen_port;
    bool show = false;
    int lost_rate = 0;
	mutable uint32_t nextConv{ 1000 };
};

