#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <string>
#include <vector>

// One tag's value in a logged row.
struct LogValue {
    // The tag's display label (see tagDisplayLabel), not TagConfig::id.
    // Sinks store it in their "tag_id" column/tag for schema compatibility.
    std::string tagLabel;
    PlcValue value;
    bool stale;
    std::string device; // optional; empty if the tag has none (see TagConfig::device)
};

// Persists rows handed to it. Knows nothing about YAML, panels, or the PLC.
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void open() = 0;
    virtual void writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) = 0;
    virtual void close() = 0;
};
