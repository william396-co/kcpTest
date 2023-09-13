#include "udpsocket.h"
#include <cstring> // memset

UdpSocket::UdpSocket()
    : m_fd { 0 }
{
    m_fd = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
}

UdpSocket::~UdpSocket()
{
    if ( m_fd )
        close();
}

bool UdpSocket::bind( uint16_t port )
{
    m_local_addr.sin_family = AF_INET;
    m_local_addr.sin_port = htons( port );
    m_local_addr.sin_addr.s_addr = htonl( INADDR_ANY ); // 接收任意IP发来的数据

    setSocketopt();

    if ( ::bind( m_fd, (struct sockaddr *)&m_local_addr, sizeof( m_local_addr ) ) == -1 ) {
        close();
        return false;
    }

    return true;
}

bool UdpSocket::connect( const char * ip, uint16_t port )
{
    m_remote_addr.sin_family = AF_INET;
    m_remote_addr.sin_port = htons( port );
    m_remote_addr.sin_addr.s_addr = inet_addr( ip );

    setSocketopt();
    int rc = ::connect( m_fd, (struct sockaddr *)&m_remote_addr, sizeof( m_remote_addr ) );
    if ( rc == -1 && errno != EINTR && errno != EINPROGRESS ) { // Ignore EINTR/EINPROGRESS
        close();
        return false;
    }

    return true;
}

void UdpSocket::close()
{
#ifdef __unix
    ::close( m_fd );
#else
    closesocket( m_fd );
#endif
    m_fd = 0;
}

int32_t UdpSocket::send( const char * bytes, uint32_t size )
{
    return sendto( m_fd, bytes, size, 0, (struct sockaddr *)&m_remote_addr, sizeof( m_remote_addr ) );
}

int32_t UdpSocket::send( const char * bytes, uint32_t size, const char * ip, uint16_t port )
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr( ip );
    addr.sin_port = htons( port );
    return sendto( m_fd, bytes, size, 0, (struct sockaddr *)&addr, sizeof( addr ) );
}

int32_t UdpSocket::recv()
{
    m_recvSize = 0;
    memset( m_recvBuffer, 0, sizeof( m_recvBuffer ) );
    socklen_t addr_len = sizeof( m_remote_addr );
    m_recvSize = ::recvfrom( m_fd, m_recvBuffer, sizeof( m_recvBuffer ), 0, (struct sockaddr *)&m_remote_addr, &addr_len );
    return m_recvSize;
}

void UdpSocket::setSocketopt()
{
#ifdef __unix
    int flag = 1;
    setsockopt( m_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof( flag ) );
    flag = 1;
    setsockopt( m_fd, SOL_SOCKET, SO_REUSEPORT, (void *)&flag, sizeof( flag ) );
#else
#endif
}

int UdpSocket::setNonblocking( bool isNonblocking )
{
#ifdef __unix
    return isNonblocking ? fcntl( m_fd, F_SETFL, fcntl( m_fd, F_GETFL ) | O_NONBLOCK ) : fcntl( m_fd, F_SETFL, fcntl( m_fd, F_GETFL ) & ~O_NONBLOCK );
#else
    unsigned long imode = isNonblocking ? 1 : 0;
    return ioctlsocket( m_fd, FIONBIO, &imode );
#endif
}
