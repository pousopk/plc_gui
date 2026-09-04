#include "plc/plc_value.hpp"
#include <cstring>

namespace {

int32_t readBigEndian16(const uint8_t* p) {
    return static_cast<int16_t>((p[0] << 8) | p[1]);
}

void writeBigEndian16(uint8_t* p, int32_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

int32_t readBigEndianU16(const uint8_t* p) {
    return static_cast<int32_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}

void writeBigEndianU16(uint8_t* p, int32_t v) {
    uint16_t u = static_cast<uint16_t>(v);
    p[0] = static_cast<uint8_t>((u >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(u & 0xFF);
}

float readBigEndianReal(const uint8_t* p) {
    uint32_t be = (static_cast<uint32_t>(p[0]) << 24) |
                  (static_cast<uint32_t>(p[1]) << 16) |
                  (static_cast<uint32_t>(p[2]) << 8) |
                   static_cast<uint32_t>(p[3]);
    float f;
    std::memcpy(&f, &be, sizeof(f));
    return f;
}

void writeBigEndianReal(uint8_t* p, float f) {
    uint32_t be;
    std::memcpy(&be, &f, sizeof(be));
    p[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((be >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(be & 0xFF);
}

} // namespace

PlcValue decodePlcValue(PlcType type, const uint8_t* buf, int byteOffset, int bit) {
    const uint8_t* p = buf + byteOffset;
    switch (type) {
        case PlcType::Bool:
            return static_cast<bool>((p[0] >> bit) & 0x01);
        case PlcType::Int:
            return readBigEndian16(p);
        case PlcType::UInt:
            return readBigEndianU16(p);
        case PlcType::USInt:
            return static_cast<int32_t>(p[0]);
        case PlcType::Real:
            return readBigEndianReal(p);
    }
    return false; // unreachable
}

void encodePlcValue(PlcType type, PlcValue value, uint8_t* buf, int byteOffset, int bit) {
    uint8_t* p = buf + byteOffset;
    switch (type) {
        case PlcType::Bool: {
            bool b = std::get<bool>(value);
            if (b) p[0] = static_cast<uint8_t>(p[0] | (1 << bit));
            else   p[0] = static_cast<uint8_t>(p[0] & ~(1 << bit));
            break;
        }
        case PlcType::Int:
            writeBigEndian16(p, std::get<int32_t>(value));
            break;
        case PlcType::UInt:
            writeBigEndianU16(p, std::get<int32_t>(value));
            break;
        case PlcType::USInt:
            p[0] = static_cast<uint8_t>(std::get<int32_t>(value));
            break;
        case PlcType::Real:
            writeBigEndianReal(p, std::get<float>(value));
            break;
    }
}

int plcTypeByteSize(PlcType type) {
    switch (type) {
        case PlcType::Bool:  return 1;
        case PlcType::USInt: return 1;
        case PlcType::Int:   return 2;
        case PlcType::UInt:  return 2;
        case PlcType::Real:  return 4;
    }
    return 0;
}
