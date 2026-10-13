#pragma once
#include "config/config_types.hpp"
#include "plc/plc_client.hpp"
#include "plc/poll_group.hpp"
#include "plc/trend_buffer.hpp"
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
    PlcIoThread(PlcConnectionConfig plcConfig, std::vector<TagConfig> tags, ValueCache& cache,
                TrendBuffer* trendBuffer = nullptr);
    ~PlcIoThread();
    PlcIoThread(const PlcIoThread&) = delete;
    PlcIoThread& operator=(const PlcIoThread&) = delete;

    void start();
    void stop();

    void requestWrite(const std::string& tagId, PlcValue value);
    ConnectionState connectionState() const;

private:
    static constexpr int kLoopSleepMs = 10;
    static constexpr int kFallbackReconnectDelayMs = 1000; // used if reconnect_backoff_ms is empty

    // A poll group plus the next time it should be read.
    struct ScheduledPollGroup {
        PollGroup group;
        std::chrono::steady_clock::time_point nextDue;
    };

    void run();
    bool tryConnect();
    int nextReconnectDelayMs();
    void handleConnectionLost();
    void pollGroup(const PollGroup& group);
    void drainWriteQueue();

    PlcConnectionConfig plcConfig_;
    std::vector<TagConfig> tags_;
    std::unordered_map<std::string, TagConfig> tagsById_;
    std::vector<ScheduledPollGroup> scheduledGroups_;
    ValueCache& cache_;
    TrendBuffer* trendBuffer_ = nullptr;

    PlcClient client_;
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex writeQueueMutex_;
    std::vector<std::pair<std::string, PlcValue>> writeQueue_;

    size_t backoffIndex_ = 0; // only touched by the I/O thread
    std::chrono::steady_clock::time_point nextConnectAttempt_{};

    // Queried fresh from the PLC on every successful connect (PDU size is
    // negotiated per-connection); falls back to kDefaultPduLength if the
    // query fails. Read/write batches are sized against this, never a
    // fixed item count alone — see buildByteSafeBatches.
    int pduLength_ = kDefaultPduLength;
};
