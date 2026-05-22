#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
#include <windows.h>
#elif !defined( __unix )
#define __unix
#endif

#ifdef __unix
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

constexpr uint32_t MAGIC_VAL = 0x34b569df;

enum PacketType : uint8_t {
    PKT_HANDSHAKE_REQ = 1,
    PKT_HANDSHAKE_ACK = 2,
    PKT_KCP_DATA      = 3,
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;   // fixed value, for example 0x4B435030
    uint8_t  type;    // handshake req / ack / kcp data
    uint32_t conv;    // 0 before handshake, assigned conv after handshake
    uint32_t size;    // size of paylod
};
#pragma pack(pop)

 inline void write_u32(char* p, uint32_t v)
{
    uint32_t n = htonl(v);
    std::memcpy(p, &n, sizeof(n));
}

inline uint32_t read_u32(const char* p)
{
    uint32_t n = 0;
    std::memcpy(&n, p, sizeof(n));
    return ntohl(n);
}

constexpr auto HEADER_SIZE = sizeof(PacketHeader);

std::string encode_packet(PacketType type, uint32_t conv, const char* payload, uint32_t size);

struct DecodedPacket {
    PacketType type{};
    uint32_t conv{};
    const char* payload{};
    uint32_t size{};
};

bool decode_packet(const char* data, size_t len, DecodedPacket& out);


#pragma pack(push, 1)
struct MsgHeader {
    uint32_t sn; // sn
    time_t ts;   // timestamp
    uint32_t sz; // datasize;
   // char* data;
};
#pragma pack(pop)

constexpr auto MsgHeaderSize = sizeof(MsgHeader);


constexpr auto PINGT_INTERVAL = 30000;// PING interval millisecond
constexpr auto HANSHAKE_TIME = 5000;