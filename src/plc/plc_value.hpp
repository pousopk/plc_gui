#pragma once
#include <cstdint>
#include <variant>

enum class PlcType { Bool, Int, Real };

using PlcValue = std::variant<bool, int32_t, float>;

// buf points at the start of the tag's DB; byteOffset/bit are relative to it.
// S7 word/dword/real values are big-endian on the wire.
PlcValue decodePlcValue(PlcType type, const uint8_t* buf, int byteOffset, int bit);
void encodePlcValue(PlcType type, PlcValue value, uint8_t* buf, int byteOffset, int bit);
