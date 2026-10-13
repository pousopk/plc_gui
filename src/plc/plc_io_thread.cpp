#include "plc/plc_io_thread.hpp"
#include <algorithm>
#include <iostream>

namespace {

// Returns a Snap7 item that reads `tag`'s bytes into `buffer` (resized to fit).
TS7DataItem makeReadItem(const TagConfig& tag, std::vector<uint8_t>& buffer) {
    int size = plcTypeByteSize(tag.type);
    buffer.assign(static_cast<size_t>(size), 0);

    TS7DataItem item{};
    item.Area = S7AreaDB;
    item.WordLen = S7WLByte;
    item.DBNumber = tag.db;
    item.Start = tag.offset;
    item.Amount = size;
    item.pdata = buffer.data();
    return item;
}

// Encodes `value` into `buffer` and returns a Snap7 item that writes it to `tag`.
TS7DataItem makeWriteItem(const TagConfig& tag, const PlcValue& value, std::vector<uint8_t>& buffer) {
    TS7DataItem item{};
    item.Area = S7AreaDB;
    item.DBNumber = tag.db;

    if (tag.type == PlcType::Bool) {
        // Bit-level write: a whole-byte write would also overwrite every
        // other bool packed into the same byte (e.g. writing "reset" at
        // bit 1 would clear "in" at bit 0). For S7WLBit, Start is a bit
        // address and the payload is one 0/1 byte.
        buffer.assign(1, std::get<bool>(value) ? 1 : 0);
        item.WordLen = S7WLBit;
        item.Start = tag.offset * 8 + tag.bit;
        item.Amount = 1;
    } else {
        int size = plcTypeByteSize(tag.type);
        buffer.assign(static_cast<size_t>(size), 0);
        encodePlcValue(tag.type, value, buffer.data(), 0, tag.bit);
        item.WordLen = S7WLByte;
        item.Start = tag.offset;
        item.Amount = size;
    }

    item.pdata = buffer.data();
    return item;
}

} // namespace

PlcIoThread::PlcIoThread(PlcConnectionConfig plcConfig, std::vector<TagConfig> tags, ValueCache& cache,
                          TrendBuffer* trendBuffer)
    : plcConfig_(std::move(plcConfig)), tags_(std::move(tags)), cache_(cache), trendBuffer_(trendBuffer) {
    for (const auto& tag : tags_) tagsById_[tag.id] = tag;

    auto now = std::chrono::steady_clock::now();
    for (auto& group : buildPollGroups(tags_)) {
        scheduledGroups_.push_back(ScheduledPollGroup{std::move(group), now});
    }
}

PlcIoThread::~PlcIoThread() { stop(); }

void PlcIoThread::start() {
    running_ = true;
    thread_ = std::thread(&PlcIoThread::run, this);
}

void PlcIoThread::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (client_.isConnected()) client_.disconnect();
}

void PlcIoThread::requestWrite(const std::string& tagId, PlcValue value) {
    std::lock_guard<std::mutex> lock(writeQueueMutex_);
    writeQueue_.emplace_back(tagId, value);
}

ConnectionState PlcIoThread::connectionState() const { return state_.load(); }

bool PlcIoThread::tryConnect() {
    auto now = std::chrono::steady_clock::now();
    if (now < nextConnectAttempt_) return false;

    state_ = ConnectionState::Connecting;
    try {
        client_.connect(plcConfig_.ip, plcConfig_.rack, plcConfig_.slot);
        state_ = ConnectionState::Connected;
        backoffIndex_ = 0;
        int negotiated = client_.pduLength();
        pduLength_ = negotiated > 0 ? negotiated : kDefaultPduLength;
        return true;
    } catch (const PlcError& e) {
        state_ = ConnectionState::Reconnecting;
        std::cerr << "PlcIoThread: connect to " << plcConfig_.ip << " rack=" << plcConfig_.rack
                  << " slot=" << plcConfig_.slot << " failed: " << e.what() << " (code " << e.code() << ")\n";
        cache_.markAllStale();
        nextConnectAttempt_ = now + std::chrono::milliseconds(nextReconnectDelayMs());
        return false;
    }
}

// Walks through plc.reconnect_backoff_ms one step per failed attempt, then
// keeps using the last entry. A successful connect resets backoffIndex_.
int PlcIoThread::nextReconnectDelayMs() {
    const std::vector<int>& steps = plcConfig_.reconnectBackoffMs;
    if (steps.empty()) return kFallbackReconnectDelayMs;

    int delayMs = steps[std::min(backoffIndex_, steps.size() - 1)];
    if (backoffIndex_ + 1 < steps.size()) ++backoffIndex_;
    return delayMs;
}

// After a failed read/write: show every value as stale in the UI and drop
// the connection, so run() goes back to tryConnect() on its next iteration.
void PlcIoThread::handleConnectionLost() {
    state_ = ConnectionState::Reconnecting;
    cache_.markAllStale();
    client_.disconnect();
}

void PlcIoThread::drainWriteQueue() {
    std::vector<std::pair<std::string, PlcValue>> pending;
    {
        std::lock_guard<std::mutex> lock(writeQueueMutex_);
        pending.swap(writeQueue_);
    }
    if (pending.empty()) return;

    // Resolve to TagConfig first so writes can reuse the same byte-safe
    // batching as reads (a large multi-tag write is just as capable of
    // exceeding the negotiated PDU as a large multi-tag read).
    std::vector<TagConfig> pendingTags;
    std::unordered_map<std::string, PlcValue> valueByTagId;
    for (const auto& [tagId, value] : pending) {
        auto tagIt = tagsById_.find(tagId);
        if (tagIt == tagsById_.end()) continue; // unknown tag id, drop
        pendingTags.push_back(tagIt->second);
        valueByTagId[tagId] = value;
    }

    auto batches = buildByteSafeBatches(pendingTags, pduLength_);
    for (const auto& batch : batches) {
        // One buffer per item; sized up front so the pointers the items
        // hold into them stay valid until the call returns.
        std::vector<std::vector<uint8_t>> buffers(batch.size());
        std::vector<TS7DataItem> items;
        items.reserve(batch.size());
        for (size_t i = 0; i < batch.size(); ++i) {
            const TagConfig& tag = batch[i];
            items.push_back(makeWriteItem(tag, valueByTagId.at(tag.id), buffers[i]));
        }

        try {
            client_.writeMultiVars(items);
        } catch (const PlcError& e) {
            std::cerr << "PlcIoThread: write failed: " << e.what() << " (code " << e.code() << ")\n";
            handleConnectionLost();
            return;
        }
    }
}

void PlcIoThread::pollGroup(const PollGroup& group) {
    auto batches = buildByteSafeBatches(group.tags, pduLength_);
    for (const auto& batch : batches) {
        // One buffer per item; sized up front so the pointers the items
        // hold into them stay valid until the call returns.
        std::vector<std::vector<uint8_t>> buffers(batch.size());
        std::vector<TS7DataItem> items;
        items.reserve(batch.size());
        for (size_t i = 0; i < batch.size(); ++i) {
            items.push_back(makeReadItem(batch[i], buffers[i]));
        }

        try {
            client_.readMultiVars(items);
        } catch (const PlcError& e) {
            std::cerr << "PlcIoThread: read failed on DB" << batch.front().db
                      << " (tags:";
            for (const auto& tag : batch) std::cerr << " " << tag.id;
            std::cerr << "): " << e.what() << " (code " << e.code() << ")\n";
            handleConnectionLost();
            return;
        }

        ValueMap freshValues;
        auto now = std::chrono::steady_clock::now();
        auto wallNow = std::chrono::system_clock::now();
        for (size_t i = 0; i < batch.size(); ++i) {
            const TagConfig& tag = batch[i];
            PlcValue value = decodePlcValue(tag.type, buffers[i].data(), 0, tag.bit);
            freshValues[tag.id] = CachedValue{value, false, now};
            if (trendBuffer_) trendBuffer_->append(tag.id, wallNow, value);
        }
        cache_.update(freshValues);
    }
}

// The I/O loop: (re)connect if needed, send queued writes, then poll every
// group whose interval has elapsed. Sleeps kLoopSleepMs between iterations.
void PlcIoThread::run() {
    while (running_) {
        if (!client_.isConnected()) {
            if (!tryConnect()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs));
                continue;
            }
        }

        drainWriteQueue();

        auto now = std::chrono::steady_clock::now();
        for (auto& scheduled : scheduledGroups_) {
            if (now >= scheduled.nextDue) {
                scheduled.nextDue = now + std::chrono::milliseconds(scheduled.group.intervalMs);
                pollGroup(scheduled.group);
                if (state_.load() == ConnectionState::Reconnecting) break; // connection lost mid-poll
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs));
    }
}
