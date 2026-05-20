#include "server.h"

#include "src/connection.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

Server::Server(uint16_t port)
    : listen{ nullptr }, md{ 0 }, listen_port{ port }
{
    listen = std::make_unique<UdpSocket>();
    listen->setNonblocking();
    if (!listen->bind(port)) {
        throw std::runtime_error( "listen socket bind error" );
    }
}

Server::~Server()
{
    for ( auto & it : connections ) {
        delete it.second;
    }
}

void Server::setmode( int mode )
{    
    md = mode;
}

uint32_t Server::alloc_conv() const
{
    auto conv = 0;
    do
    {
        conv = ++nextConv;
    } while (connections.count(conv));
    return conv;
}

Connection* Server::createConn(UdpSocket* socket, const char* remote_ip, uint16_t remote_port, uint32_t conv)
{
    auto it = connections.find(conv);
    if ( it != connections.end() ) {
        return it->second;
    }
    Connection* conn = new Connection(socket, remote_ip, remote_port, conv);
    conn->setmode( md );
    conn->set_show( show );
    conn->setmode(md);
    if ( conn ) {
        connections.emplace(conv, conn);
        printf("new Connection:[%s:%d] accepted\n", remote_ip, remote_port);
        return conn;
    }
    delete conn;
    return nullptr;
}

Connection* Server::findConn(uint32_t conv) const
{
    auto it = connections.find(conv);
    if (it != connections.end()) {
        return it->second;
    }
    return nullptr;
}

void Server::recv_work()
{
    while ( is_running ) {
        util::isleep( 1 );

        // lower level recv
        if ( listen->recv() <= 0 ) {
            continue;
        }

        recv_data(listen->getRecvBuffer(), listen->getRecvSize());
    }
}

void Server::send_work()
{
    while ( is_running ) {
        util::isleep( 1 );
        for ( auto & it : connections ) {
            it.second->update();
        }
    }
}

void Server::recv_data(const char* buf, size_t len)
{
    DecodedPacket pkt{};
    if (!decode_packet(buf, len, pkt)) {
        return;
    }

    switch (pkt.type) {
    case PacketType::PKT_HANDSHAKE_REQ:        
    {
        // handle sharehand
        // server: allocate conv and reply
        uint32_t conv = alloc_conv();
		std::cout << "conv: " << conv << "remoteIp: " << listen->getRemoteIp() << "remotePort: " << listen->getRemotePort() << "\n";
        auto conn = createConn(listen.get(), listen->getRemoteIp(), listen->getRemotePort(), conv);
        if (conn) {
            conn->send_shakehand_reply();
        }
        break;
    }
    case PacketType::PKT_KCP_DATA:
    {
        // pass pkt.payload / pkt.size to ikcp_input()
        auto conn = findConn(pkt.conv);
        if (!conn) {
            std::cout << " cannot find connection,conv:" << pkt.conv << "\n";
        }
        else {
            conn->recv(pkt.payload, pkt.size);
        }
        break;
    }
    case PacketType::PKT_HANDSHAKE_ACK:
    {
        std::cerr << "invalid PacketType: " << pkt.type << "\n";
        break;
    }
    }
}
