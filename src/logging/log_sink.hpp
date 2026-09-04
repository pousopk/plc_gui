#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <string>
#include <vector>

struct LogValue {
    std::string tagId;
    PlcValue value;
    bool stale;
};

// Persists rows handed to it. Knows nothing about YAML, panels, or the PLC.
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void open() = 0;
    virtual void writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) = 0;
    virtual void close() = 0;
};
