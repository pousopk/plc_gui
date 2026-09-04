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
