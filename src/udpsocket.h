#pragma once

#include "util.h"
#include "packet.h"
#include <string>
constexpr auto RECV_BUF_SIZE = 1024 * 4;

struct UdpEndpoint
{
    std::string ip;
    uint16_t port{};

    bool operator==(const UdpEndpoint& other) const
    {
        return ip == other.ip && port == other.port;
    }
};

namespace std {
template<>
struct hash<UdpEndpoint>
{
    size_t operator()(const UdpEndpoint& endpoint) const
    {
        size_t value = 17;
        value = 31 * value + std::hash<std::string>{}(endpoint.ip);
        value = 31 * value + std::hash<uint16_t>{}(endpoint.port);
        return value;
    }
};
} // namespace std

class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    // server function
    bool bind( uint16_t port );

    // client function
    bool connect( const char * ip, uint16_t port );

    void close();
	int32_t send(const char* bytes, uint32_t size,uint32_t conv, PacketType type);
    int32_t send(const char * bytes, uint32_t size, const char * ip, uint16_t port, uint32_t conv, PacketType type);
    int32_t send_to(const UdpEndpoint& endpoint, const char* bytes, uint32_t size, uint32_t conv, PacketType type);
    int32_t recv(UdpEndpoint* remote = nullptr);
    int32_t recv(uint32_t& conv);

    const char * getRecvBuffer() const { return m_recvBuffer; }
    uint32_t getRecvSize() const { return m_recvSize; }
    int setNonblocking( bool isNonblocking = true );
    void setSocketopt();

    const char * getRemoteIp() const { return inet_ntoa( m_remote_addr.sin_addr ); }
    uint16_t getRemotePort() const { return ntohs( m_remote_addr.sin_port ); }
    UdpEndpoint getRemoteEndpoint() const;
    UdpEndpoint getLastRemoteEndpoint() const;

    const char * getLocalIp() const { return inet_ntoa( m_local_addr.sin_addr ); }
    uint16_t getLocalPort() const { return ntohs( m_local_addr.sin_port ); }

    void setLostrate( int rate ) { lost_rate = rate; }
    int getFd() const { return m_fd; }

private:
    int m_fd;
    struct sockaddr_in m_local_addr;
    struct sockaddr_in m_remote_addr;
    struct sockaddr_in m_last_remote_addr;
    char m_recvBuffer[RECV_BUF_SIZE];
    int m_recvSize;
    int lost_rate; // lost package rate
};
