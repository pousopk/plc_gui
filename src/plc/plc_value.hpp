#pragma once
#include <cstdint>
#include <variant>

enum class PlcType { Bool, Int, UInt, USInt, Real };

// UInt (16-bit unsigned) and USInt (8-bit unsigned) are both decoded/encoded
// as plain non-negative int32_t values — no separate variant alternative is
// needed since int32_t already covers their full range without ambiguity;
// only the byte-level decode differs (no sign extension).
using PlcValue = std::variant<bool, int32_t, float>;

// buf points at the start of the tag's DB; byteOffset/bit are relative to it.
// S7 word/dword/real values are big-endian on the wire.
PlcValue decodePlcValue(PlcType type, const uint8_t* buf, int byteOffset, int bit);
void encodePlcValue(PlcType type, PlcValue value, uint8_t* buf, int byteOffset, int bit);
int plcTypeByteSize(PlcType type); // 1 for Bool/USInt, 2 for Int/UInt, 4 for Real
