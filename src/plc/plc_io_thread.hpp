#pragma once
#include "config/config_types.hpp"
#include "plc/plc_client.hpp"
#include "plc/poll_group.hpp"
#include "plc/value_cache.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

enum class ConnectionState { Disconnected, Connecting, Connected, Reconnecting };

// Owns the single Snap7 connection and the background poll/write loop.
// Not copyable or movable — construct once, call start()/stop() around its
// lifetime. Safe to call requestWrite()/connectionState() from any thread.
class PlcIoThread {
public:
    PlcIoThread(PlcConnectionConfig plcConfig, std::vector<TagConfig> tags, ValueCache& cache);
    ~PlcIoThread();
    PlcIoThread(const PlcIoThread&) = delete;
    PlcIoThread& operator=(const PlcIoThread&) = delete;

    void start();
    void stop();

    void requestWrite(const std::string& tagId, PlcValue value);
    ConnectionState connectionState() const;

private:
    void run();
    bool tryConnect();
    void pollGroup(const PollGroup& group);
    void drainWriteQueue();
    void markAllStale();

    PlcConnectionConfig plcConfig_;
    std::vector<TagConfig> tags_;
    std::unordered_map<std::string, TagConfig> tagsById_;
    std::vector<PollGroup> pollGroups_;
    std::vector<std::chrono::steady_clock::time_point> nextDue_;
    ValueCache& cache_;

    PlcClient client_;
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex writeQueueMutex_;
    std::vector<std::pair<std::string, PlcValue>> writeQueue_;

    size_t backoffIndex_ = 0; // only touched by the I/O thread
    std::chrono::steady_clock::time_point nextConnectAttempt_{};
};
