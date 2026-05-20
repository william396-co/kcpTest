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

struct PacketHeader {
    uint32_t magic;   // fixed value, for example 0x4B435030
    uint32_t conv;    // 0 before handshake, assigned conv after handshake
    uint8_t  type;    // handshake req / ack / kcp data
};

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

constexpr auto HEADER_SIZE = sizeof(PacketHeader) + 1;

inline std::string encode_packet(PacketType type, uint32_t conv, const char* payload, uint32_t size)
{
    std::string out;
    out.resize(HEADER_SIZE + size);

    write_u32(&out[0], MAGIC_VAL);
    out[4] = static_cast<char>(type);
    write_u32(&out[5], conv);
    write_u32(&out[9], size);

    if (size > 0) {
        std::memcpy(&out[HEADER_SIZE], payload, size);
    }
    return out;
}

struct DecodedPacket {
    PacketType type{};
    uint32_t conv{};
    const char* payload{};
    uint32_t size{};
};

inline bool decode_packet(const char* data, size_t len, DecodedPacket& out)
{
    if (len < HEADER_SIZE) {
        return false;
    }

    uint32_t magic = read_u32(data);
    if (magic != MAGIC_VAL) {
        return false;
    }

    out.type = static_cast<PacketType>(static_cast<uint8_t>(data[4]));
    out.conv = read_u32(data + 5);
    out.size = read_u32(data + 9);

    if (len < HEADER_SIZE + out.size) {
        return false;
    }

    out.payload = data + HEADER_SIZE;
    return true;
}