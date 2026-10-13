#include "logging/log_manager.hpp"
#include <iostream>

LogManager::LogManager(std::vector<LogRuleConfig> rules,
                       std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById,
                       const ValueCache& cache,
                       const std::vector<TagConfig>& tags)
    : sinksById_(std::move(sinksById)), cache_(cache) {
    for (const auto& tag : tags) tagsById_[tag.id] = tag;

    auto now = std::chrono::steady_clock::now();
    for (auto& rule : rules) {
        scheduledRules_.push_back(ScheduledRule{std::move(rule), now});
    }
}

LogManager::~LogManager() { stop(); }

void LogManager::start() {
    for (auto& [sinkId, sink] : sinksById_) sink->open();
    running_ = true;
    thread_ = std::thread(&LogManager::run, this);
}

void LogManager::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    for (auto& [sinkId, sink] : sinksById_) sink->close();
}

void LogManager::runRule(const LogRuleConfig& rule) {
    ValueMap snapshot = cache_.snapshot();
    std::vector<LogValue> values;
    values.reserve(rule.tags.size());
    for (const auto& tagId : rule.tags) {
        auto cachedIt = snapshot.find(tagId);
        if (cachedIt == snapshot.end()) continue; // not polled yet
        const CachedValue& cached = cachedIt->second;

        std::string tagLabel = tagId;
        std::string device;
        auto tagIt = tagsById_.find(tagId);
        if (tagIt != tagsById_.end()) {
            tagLabel = tagDisplayLabel(tagIt->second);
            device = tagIt->second.device;
        }

        values.push_back(LogValue{tagLabel, cached.value, cached.stale, device});
    }
    if (values.empty()) return;

    auto now = std::chrono::system_clock::now();
    for (const auto& sinkId : rule.sinks) {
        auto sinkIt = sinksById_.find(sinkId);
        if (sinkIt == sinksById_.end()) continue;
        try {
            sinkIt->second->writeRow(now, values);
        } catch (const std::exception& e) {
            std::cerr << "LogManager: sink '" << sinkId << "' write failed: " << e.what() << "\n";
        }
    }
}

// Runs every rule whose interval has elapsed, then sleeps kLoopSleepMs.
void LogManager::run() {
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        for (auto& scheduled : scheduledRules_) {
            if (now >= scheduled.nextDue) {
                scheduled.nextDue = now + std::chrono::milliseconds(scheduled.rule.intervalMs);
                runRule(scheduled.rule);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs));
    }
}
