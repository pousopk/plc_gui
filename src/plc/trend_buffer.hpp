#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct TrendSample {
    std::chrono::system_clock::time_point timestamp;
    PlcValue value;
};

// Thread-safe, per-tag ring buffer of recent samples for the Trends
// screen's live view. Fixed sample-count capacity per tag (not a time
// window), so a fast-polling tag can't blow out memory. PlcIoThread
// appends on every fresh decode; the UI thread reads a snapshot once per
// frame per selected tag — a single mutex is enough since both sides touch
// it infrequently relative to frame rate.
//
// Keyed by each tag's raw TagConfig::id — the same key PlcIoThread already
// uses for ValueCache. This is NOT the same key SQLite log rows use (see
// TrendHistoryQuery) — see trends_screen.hpp for why that distinction
// matters.
class TrendBuffer {
public:
    explicit TrendBuffer(size_t capacityPerTag = 300);

    void append(const std::string& tagId, std::chrono::system_clock::time_point timestamp, PlcValue value);
    std::vector<TrendSample> snapshot(const std::string& tagId) const;

private:
    size_t capacityPerTag_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::deque<TrendSample>> buffers_;
};
