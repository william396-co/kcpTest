#include "packet.h"

std::string encode_packet(PacketType type, uint32_t conv, const char* payload, uint32_t size)
{
    std::string out;
    out.resize(HEADER_SIZE + size);

    // Every UDP datagram starts with a fixed magic so the receiver can reject junk early.
    write_u32(&out[0], MAGIC_VAL);
    out[4] = static_cast<char>(type);
    write_u32(&out[5], conv);
    write_u32(&out[9], size);

    if (size > 0) {
        std::memcpy(&out[HEADER_SIZE], payload, size);
    }
    return out;
}

bool decode_packet(const char* data, size_t len, DecodedPacket& out)
{
    if (len < HEADER_SIZE) {
        return false;
    }

    // Reject packets that do not belong to this protocol before touching conv or payload.
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

std::string encode_app_message(uint32_t sn, time_t ts, const char* payload, uint32_t size)
{
    std::string out;
    out.resize(MsgHeaderSize + size);

    auto* header = reinterpret_cast<MsgHeader*>(out.data());
    header->sn = htonl(sn);
    header->ts = htonll(ts);
    header->sz = htonl(size);

    if (size > 0) {
        std::memcpy(out.data() + MsgHeaderSize, payload, size);
    }
    return out;
}

bool decode_app_message(const char* data, size_t len, DecodedAppMessage& out)
{
    if (len < MsgHeaderSize) {
        return false;
    }

    const auto* header = reinterpret_cast<const MsgHeader*>(data);
    out.sn = ntohl(header->sn);
    out.ts = ntohll(header->ts);
    out.size = ntohl(header->sz);

    if (len < MsgHeaderSize + out.size) {
        return false;
    }

    out.payload = data + MsgHeaderSize;
    return true;
}
