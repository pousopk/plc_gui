#include <doctest/doctest.h>
#include "plc/plc_value.hpp"
#include <cstring>

TEST_CASE("decodePlcValue reads a big-endian S7 REAL") {
    uint8_t buf[4];
    float expected = 3.5f;
    uint32_t be;
    std::memcpy(&be, &expected, 4);
    be = __builtin_bswap32(be);
    std::memcpy(buf, &be, 4);

    PlcValue v = decodePlcValue(PlcType::Real, buf, 0, 0);
    CHECK(std::get<float>(v) == doctest::Approx(3.5f));
}

TEST_CASE("decodePlcValue reads a big-endian S7 INT (16-bit)") {
    uint8_t buf[2] = {0x00, 0x2A}; // 42, big-endian
    PlcValue v = decodePlcValue(PlcType::Int, buf, 0, 0);
    CHECK(std::get<int32_t>(v) == 42);
}

TEST_CASE("decodePlcValue reads a big-endian S7 UINT (16-bit unsigned) without sign extension") {
    uint8_t buf[2] = {0xC3, 0x50}; // 50000, which would be negative if read as signed Int
    PlcValue v = decodePlcValue(PlcType::UInt, buf, 0, 0);
    CHECK(std::get<int32_t>(v) == 50000);
}

TEST_CASE("decodePlcValue reads a S7 USINT (8-bit unsigned)") {
    uint8_t buf[1] = {0xFF}; // 255
    PlcValue v = decodePlcValue(PlcType::USInt, buf, 0, 0);
    CHECK(std::get<int32_t>(v) == 255);
}

TEST_CASE("decodePlcValue reads a specific bit for BOOL") {
    uint8_t buf[1] = {0b00000100}; // bit 2 set
    CHECK(std::get<bool>(decodePlcValue(PlcType::Bool, buf, 0, 2)) == true);
    CHECK(std::get<bool>(decodePlcValue(PlcType::Bool, buf, 0, 1)) == false);
}

TEST_CASE("encodePlcValue round-trips REAL, INT, BOOL") {
    uint8_t buf[4] = {0, 0, 0, 0};

    encodePlcValue(PlcType::Real, 12.25f, buf, 0, 0);
    CHECK(std::get<float>(decodePlcValue(PlcType::Real, buf, 0, 0)) == doctest::Approx(12.25f));

    encodePlcValue(PlcType::Int, int32_t{-5}, buf, 0, 0);
    CHECK(std::get<int32_t>(decodePlcValue(PlcType::Int, buf, 0, 0)) == -5);

    buf[0] = 0;
    encodePlcValue(PlcType::Bool, true, buf, 0, 3);
    CHECK(std::get<bool>(decodePlcValue(PlcType::Bool, buf, 0, 3)) == true);
    encodePlcValue(PlcType::Bool, false, buf, 0, 3);
    CHECK(std::get<bool>(decodePlcValue(PlcType::Bool, buf, 0, 3)) == false);
}

TEST_CASE("encodePlcValue round-trips UINT and USINT") {
    uint8_t buf[2] = {0, 0};

    encodePlcValue(PlcType::UInt, int32_t{50000}, buf, 0, 0);
    CHECK(std::get<int32_t>(decodePlcValue(PlcType::UInt, buf, 0, 0)) == 50000);

    encodePlcValue(PlcType::USInt, int32_t{255}, buf, 0, 0);
    CHECK(std::get<int32_t>(decodePlcValue(PlcType::USInt, buf, 0, 0)) == 255);
}

TEST_CASE("plcTypeByteSize reports correct sizes for every type") {
    CHECK(plcTypeByteSize(PlcType::Bool) == 1);
    CHECK(plcTypeByteSize(PlcType::USInt) == 1);
    CHECK(plcTypeByteSize(PlcType::Int) == 2);
    CHECK(plcTypeByteSize(PlcType::UInt) == 2);
    CHECK(plcTypeByteSize(PlcType::Real) == 4);
}
