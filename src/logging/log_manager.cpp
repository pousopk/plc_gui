#include "logging/log_manager.hpp"
#include <iostream>

LogManager::LogManager(std::vector<LogRuleConfig> rules,
                        std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinksById,
                        const ValueCache& cache,
                        std::unordered_map<std::string, std::string> tagLabels,
                        std::unordered_map<std::string, std::string> tagDevices)
    : rules_(std::move(rules)), sinksById_(std::move(sinksById)), cache_(cache),
      tagLabels_(std::move(tagLabels)), tagDevices_(std::move(tagDevices)) {
    nextDue_.assign(rules_.size(), std::chrono::steady_clock::now());
}

LogManager::~LogManager() { stop(); }

void LogManager::start() {
    for (auto& [id, sink] : sinksById_) sink->open();
    running_ = true;
    thread_ = std::thread(&LogManager::run, this);
}

void LogManager::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    for (auto& [id, sink] : sinksById_) sink->close();
}

void LogManager::runRule(const LogRuleConfig& rule) {
    auto snap = cache_.snapshot();
    std::vector<LogValue> values;
    values.reserve(rule.tags.size());
    for (const auto& tagId : rule.tags) {
        auto it = snap->find(tagId);
        if (it == snap->end()) continue; // not polled yet
        auto labelIt = tagLabels_.find(tagId);
        const std::string& displayName = (labelIt != tagLabels_.end()) ? labelIt->second : tagId;
        auto deviceIt = tagDevices_.find(tagId);
        std::string device = (deviceIt != tagDevices_.end()) ? deviceIt->second : std::string();
        values.push_back(LogValue{displayName, it->second.value, it->second.stale, device});
    }
    if (values.empty()) return;

    auto now = std::chrono::system_clock::now();
    for (const auto& sinkId : rule.sinks) {
        auto it = sinksById_.find(sinkId);
        if (it == sinksById_.end()) continue;
        try {
            it->second->writeRow(now, values);
        } catch (const std::exception& e) {
            std::cerr << "LogManager: sink '" << sinkId << "' write failed: " << e.what() << "\n";
        }
    }
}

void LogManager::run() {
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < rules_.size(); ++i) {
            if (now >= nextDue_[i]) {
                nextDue_[i] = now + std::chrono::milliseconds(rules_[i].intervalMs);
                runRule(rules_[i]);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
