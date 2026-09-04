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
