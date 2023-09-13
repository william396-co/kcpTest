#include "server.h"

#include "src/connection.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

Server::Server( uint16_t port, uint32_t conv )
    : listen { nullptr }, md { 0 }, listen_port { port }
{
    listen = std::make_unique<UdpSocket>();
    listen->setNonblocking();
    if ( !listen->bind( port ) ) {
        throw std::runtime_error( "listen socket bind error" );
    }

    kcp = ikcp_create( conv, listen.get() );
    ikcp_setoutput( kcp, util::kcp_output );

    ikcp_wndsize( kcp, 128, 128 );
    ikcp_nodelay( kcp, 0, 10, 0, 0 );
}

Server::~Server()
{
    for ( auto & it : connections ) {
        delete it.second;
    }

    ikcp_release( kcp );
}

void Server::setmode( int mode )
{
    util::ikcp_set_mode( kcp, mode );
    md = mode;
}

Connection * Server::findConn( const char * remote_ip, uint16_t remote_port )
{
    auto it = connections.find( std::make_pair( remote_ip, remote_port ) );
    if ( it != connections.end() ) {
        return it->second;
    }
    Connection * conn = new Connection( listen_port, remote_ip, remote_port, conv );
    conn->setmode( md );
    conn->set_show( show );
    if ( conn ) {
        connections.emplace( std::make_pair( remote_ip, remote_port ), conn );
        printf( "new Connection:[%s:%d] accepted\n", remote_ip, remote_port );
        return conn;
    }
    delete conn;
    return nullptr;
}

void Server::accept()
{
    while ( is_running ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // lower level recv
        if ( listen->recv() <= 0 ) {
            continue;
        }

        auto conn = findConn( listen->getRemoteIp(), listen->getRemotePort() );
        if ( conn ) {
            conn->recv_data( listen->getRecvBuffer(), listen->getRecvSize() );
            continue;
        }
    }
}

void Server::run()
{
    while ( is_running ) {

        for ( auto & it : connections ) {
            it.second->update();
        }
    }
}
