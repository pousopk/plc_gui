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

// Runs each logging rule on its own interval: reads the rule's tags from
// ValueCache and writes them to the rule's sinks. Owns its own thread and
// depends only on ValueCache and ILogSink, never on the UI, so it runs
// identically in GUI and headless mode.
class LogManager {
public:
    // `tags` supplies each logged tag's label and device. A rule tag that
    // isn't in `tags` is logged under its raw id with an empty device.
    LogManager(std::vector<LogRuleConfig> rules,
               std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById,
               const ValueCache& cache,
               const std::vector<TagConfig>& tags = {});
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    void start();
    void stop();

private:
    static constexpr int kLoopSleepMs = 10;

    // A rule plus the next time it should run.
    struct ScheduledRule {
        LogRuleConfig rule;
        std::chrono::steady_clock::time_point nextDue;
    };

    void run();
    void runRule(const LogRuleConfig& rule);

    std::vector<ScheduledRule> scheduledRules_;
    std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById_;
    const ValueCache& cache_;
    std::unordered_map<std::string, TagConfig> tagsById_;

    std::thread thread_;
    std::atomic<bool> running_{false};
};
