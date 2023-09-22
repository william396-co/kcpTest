#pragma once

#include "util.h"
constexpr auto RECV_BUF_SIZE = 1024 * 4;

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
    int32_t send( const char * bytes, uint32_t size );
    int32_t send( const char * bytes, uint32_t size, const char * ip, uint16_t port );
    int32_t recv();

    const char * getRecvBuffer() const { return m_recvBuffer; }
    uint32_t getRecvSize() const { return m_recvSize; }
    int setNonblocking( bool isNonblocking = true );
    void setSocketopt();

    const char * getRemoteIp() const { return inet_ntoa( m_remote_addr.sin_addr ); }
    uint16_t getRemotePort() const { return ntohs( m_remote_addr.sin_port ); }

    const char * getLocalIp() const { return inet_ntoa( m_local_addr.sin_addr ); }
    uint16_t getLocalPort() const { return ntohs( m_local_addr.sin_port ); }

    void setLostrate( int rate ) { lost_rate = rate; }
    int getFd() const { return m_fd; }

private:
    int m_fd;
    struct sockaddr_in m_local_addr;
    struct sockaddr_in m_remote_addr;
    char m_recvBuffer[RECV_BUF_SIZE];
    uint32_t m_recvSize;
    int lost_rate; // lost package rate
};
