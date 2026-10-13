#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

struct CachedValue {
    PlcValue value;
    bool stale = false; // true when the last read failed or the PLC is disconnected
    std::chrono::steady_clock::time_point lastUpdate{};
};

using ValueMap = std::unordered_map<std::string, CachedValue>; // keyed by TagConfig::id

// The latest known value of every polled tag. Written by PlcIoThread, read
// by the UI and LogManager. Thread-safe: every method takes the same mutex,
// and readers get their own copy, so they never see a half-applied update.
class ValueCache {
public:
    // A copy of every tag's current value.
    ValueMap snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_;
    }

    // Sets the given tags' values; tags not in `changedValues` are untouched.
    void update(const ValueMap& changedValues) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [tagId, cached] : changedValues) values_[tagId] = cached;
    }

    // Flags every value as stale (e.g. after the PLC connection drops).
    void markAllStale() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [tagId, cached] : values_) cached.stale = true;
    }

private:
    mutable std::mutex mutex_;
    ValueMap values_;
};
