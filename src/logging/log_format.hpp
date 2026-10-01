#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <type_traits>

inline std::string formatPlcValue(const PlcValue& v) {
    return std::visit([](auto&& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return std::to_string(val);
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%g", val);
            return std::string(buf);
        }
    }, v);
}

inline std::string formatTimestamp(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, static_cast<int>(ms.count()));
    return std::string(buf);
}

// Inverse of formatPlcValue. `type` resolves which variant alternative to
// parse into — formatPlcValue's text alone doesn't distinguish, e.g. Int,
// UInt, USInt, and Time all format as a plain decimal integer string (see
// decodePlcValue in plc_value.cpp for the same type-to-variant mapping).
// Throws std::invalid_argument/std::out_of_range on malformed text — the
// caller (TrendHistoryQuery) decides whether to skip or propagate.
inline PlcValue parsePlcValue(PlcType type, const std::string& text) {
    if (type == PlcType::Bool) return text == "true";
    if (type == PlcType::Real) return std::stof(text);
    return static_cast<int32_t>(std::stol(text)); // Int, UInt, USInt, Time
}

// Inverse of formatTimestamp. Uses timegm (POSIX, not standard C++) to
// match formatTimestamp's use of gmtime_r — consistent with this being a
// Linux-only application.
inline std::chrono::system_clock::time_point parseTimestamp(const std::string& s) {
    std::tm tmBuf{};
    int ms = 0;
    std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d.%dZ",
                &tmBuf.tm_year, &tmBuf.tm_mon, &tmBuf.tm_mday,
                &tmBuf.tm_hour, &tmBuf.tm_min, &tmBuf.tm_sec, &ms);
    tmBuf.tm_year -= 1900;
    tmBuf.tm_mon -= 1;
    std::time_t t = timegm(&tmBuf);
    return std::chrono::system_clock::from_time_t(t) + std::chrono::milliseconds(ms);
}
