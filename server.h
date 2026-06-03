#pragma once
#include "src/util.h"
#include "src/udpsocket.h"
#include "src/ikcp.h"
#include "src/joining_thread.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;

class Server
{
public:
    // Optional application-level hook for payloads delivered by a Connection.
    // Meaning:
    // - "application-level" means code above the transport/session layer.
    // - By the time this handler runs, UDP parsing and KCP reassembly are already done.
    // Function:
    // - Lets callers plug in business logic for each decoded payload.
    // - If no handler is installed, the server does not apply any default app behavior.
    using ApplicationMessageHandler = std::function<void(Connection&, const char*, size_t)>;

	Server(uint16_t port, uint32_t work_thread_cnt = 4);
    ~Server();

    bool startService();
    bool stopService();

    // Register or replace the optional application callback.
    // Pass an empty handler to disable custom logic and make payload dispatch a no-op.
    void set_application_message_handler(ApplicationMessageHandler handler);

    ConnectionPtr findConn(uint32_t conv)const;
    ConnectionPtr findConn(const UdpEndpoint& endpoint)const;

    void recv_work();
    void worker_loop(uint32_t idx);
    // Decode one UDP datagram, route handshake packets, and dispatch KCP payloads.
    void parse_udp_data(const UdpEndpoint& remote, const char* buf, size_t len);

    void setmode( int mode );
    void show_data( bool _show ) { show = _show; }

    void setlostrate( int lostrate );

    uint32_t alloc_conv();
    
private:
    struct ConnectionShard
    {
        mutable std::mutex mutex;
        std::unordered_map<uint32_t, ConnectionPtr> connections;
    };

    ConnectionPtr createConn(const UdpEndpoint& endpoint, uint32_t conv);
    ConnectionPtr findConn_unlocked(uint32_t conv, uint32_t idx) const;
    void reclaim_inactive(ConnectionShard& shard);
    void invoke_application_handler(Connection& connection, const char* data, size_t len);
    uint32_t shard_index(uint32_t conv) const;

    std::unique_ptr<UdpSocket> listen;
    std::vector<joining_thread> work_threads;
    joining_thread recv_thread;
    std::vector<std::unique_ptr<ConnectionShard>> shards;
    mutable std::mutex conn_id_mutex;
    std::unordered_map<UdpEndpoint, uint32_t> connIDConvMap;
    mutable std::mutex handler_mutex;
    // Stored application callback used by invoke_application_handler().
    // This is optional; an empty handler means "do not perform any app-level action".
    ApplicationMessageHandler application_message_handler;
    int md;
    uint16_t listen_port;
    bool show = false;
    int lost_rate = 0;
    const uint32_t work_thread_cnt;
    uint32_t nextConv{ 1000 };
    std::atomic_bool running{ false };
};

