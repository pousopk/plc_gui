#pragma once
#include "config/config_types.hpp"
#include "logging/log_sink.hpp"
#include "plc/value_cache.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

// Owns its own thread; depends only on ValueCache and ILogSink, never on
// the UI. This is what lets it run identically in GUI and headless mode.
class LogManager {
public:
    // tagLabels maps tag id -> display name, for tags whose config gave
    // them a `label` (an id absent from this map just logs under its own
    // id). Defaults to empty so existing call sites need no changes.
    LogManager(std::vector<LogRuleConfig> rules,
               std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById,
               const ValueCache& cache,
               std::unordered_map<std::string, std::string> tagLabels = {});
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    void start();
    void stop();

private:
    void run();
    void runRule(const LogRuleConfig& rule);

    std::vector<LogRuleConfig> rules_;
    std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById_;
    const ValueCache& cache_;
    std::unordered_map<std::string, std::string> tagLabels_;

    std::vector<std::chrono::steady_clock::time_point> nextDue_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
