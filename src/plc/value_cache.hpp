#pragma once
#include "plc/plc_value.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

struct CachedValue {
    PlcValue value;
    bool stale = false;
    std::chrono::steady_clock::time_point lastUpdate{};
};

using ValueMap = std::unordered_map<std::string, CachedValue>;

class ValueCache {
public:
    ValueCache() : current_(std::make_shared<const ValueMap>()) {}

    void publish(std::shared_ptr<const ValueMap> next) {
        std::atomic_store(&current_, std::move(next));
    }

    std::shared_ptr<const ValueMap> snapshot() const {
        return std::atomic_load(&current_);
    }

private:
    std::shared_ptr<const ValueMap> current_;
};
