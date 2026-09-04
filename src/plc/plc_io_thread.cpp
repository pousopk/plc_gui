#include "plc/plc_io_thread.hpp"
#include <algorithm>
#include <iostream>

PlcIoThread::PlcIoThread(PlcConnectionConfig plcConfig, std::vector<TagConfig> tags, ValueCache& cache)
    : plcConfig_(std::move(plcConfig)), tags_(std::move(tags)), cache_(cache) {
    for (const auto& t : tags_) tagsById_[t.id] = t;
    pollGroups_ = buildPollGroups(tags_);
    nextDue_.assign(pollGroups_.size(), std::chrono::steady_clock::now());
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
        return true;
    } catch (const PlcError& e) {
        state_ = ConnectionState::Reconnecting;
        std::cerr << "PlcIoThread: connect to " << plcConfig_.ip << " rack=" << plcConfig_.rack
                  << " slot=" << plcConfig_.slot << " failed: " << e.what() << " (code " << e.code() << ")\n";
        markAllStale();
        int delayMs = plcConfig_.reconnectBackoffMs.empty()
            ? 1000
            : plcConfig_.reconnectBackoffMs[std::min(backoffIndex_, plcConfig_.reconnectBackoffMs.size() - 1)];
        if (backoffIndex_ + 1 < plcConfig_.reconnectBackoffMs.size()) ++backoffIndex_;
        nextConnectAttempt_ = now + std::chrono::milliseconds(delayMs);
        return false;
    }
}

void PlcIoThread::markAllStale() {
    auto current = cache_.snapshot();
    auto next = std::make_shared<ValueMap>(*current);
    for (auto& [id, cv] : *next) cv.stale = true;
    cache_.publish(next);
}

void PlcIoThread::drainWriteQueue() {
    std::vector<std::pair<std::string, PlcValue>> pending;
    {
        std::lock_guard<std::mutex> lock(writeQueueMutex_);
        pending.swap(writeQueue_);
    }
    if (pending.empty()) return;

    for (size_t i = 0; i < pending.size(); i += kMaxItemsPerBatch) {
        size_t end = std::min(i + static_cast<size_t>(kMaxItemsPerBatch), pending.size());
        std::vector<TS7DataItem> items;
        std::vector<std::vector<uint8_t>> buffers;
        items.reserve(end - i);
        buffers.reserve(end - i);

        for (size_t j = i; j < end; ++j) {
            auto it = tagsById_.find(pending[j].first);
            if (it == tagsById_.end()) continue; // unknown tag id, drop
            const TagConfig& tag = it->second;
            int size = plcTypeByteSize(tag.type);
            buffers.emplace_back(static_cast<size_t>(size), 0);
            encodePlcValue(tag.type, pending[j].second, buffers.back().data(), 0, tag.bit);

            TS7DataItem item{};
            item.Area = S7AreaDB;
            item.WordLen = S7WLByte;
            item.DBNumber = tag.db;
            item.Start = tag.offset;
            item.Amount = size;
            item.pdata = buffers.back().data();
            items.push_back(item);
        }
        if (items.empty()) continue;
        try {
            client_.writeMultiVars(items);
        } catch (const PlcError& e) {
            state_ = ConnectionState::Reconnecting;
            std::cerr << "PlcIoThread: write failed: " << e.what() << " (code " << e.code() << ")\n";
            markAllStale();
            client_.disconnect();
            return;
        }
    }
}

void PlcIoThread::pollGroup(const PollGroup& group) {
    for (const auto& batch : group.batches) {
        std::vector<TS7DataItem> items;
        std::vector<std::vector<uint8_t>> buffers;
        items.reserve(batch.size());
        buffers.reserve(batch.size());

        for (const auto& tag : batch) {
            int size = plcTypeByteSize(tag.type);
            buffers.emplace_back(static_cast<size_t>(size), 0);
            TS7DataItem item{};
            item.Area = S7AreaDB;
            item.WordLen = S7WLByte;
            item.DBNumber = tag.db;
            item.Start = tag.offset;
            item.Amount = size;
            item.pdata = buffers.back().data();
            items.push_back(item);
        }

        try {
            client_.readMultiVars(items);
        } catch (const PlcError& e) {
            state_ = ConnectionState::Reconnecting;
            std::cerr << "PlcIoThread: read failed on DB" << batch.front().db
                      << " (tags:";
            for (const auto& tag : batch) std::cerr << " " << tag.id;
            std::cerr << "): " << e.what() << " (code " << e.code() << ")\n";
            markAllStale();
            client_.disconnect();
            return;
        }

        auto current = cache_.snapshot();
        auto next = std::make_shared<ValueMap>(*current);
        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < batch.size(); ++i) {
            const TagConfig& tag = batch[i];
            PlcValue value = decodePlcValue(tag.type, buffers[i].data(), 0, tag.bit);
            (*next)[tag.id] = CachedValue{value, false, now};
        }
        cache_.publish(next);
    }
}

void PlcIoThread::run() {
    while (running_) {
        if (!client_.isConnected()) {
            if (!tryConnect()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        }

        drainWriteQueue();

        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < pollGroups_.size(); ++i) {
            if (now >= nextDue_[i]) {
                nextDue_[i] = now + std::chrono::milliseconds(pollGroups_[i].intervalMs);
                pollGroup(pollGroups_[i]);
                if (state_.load() == ConnectionState::Reconnecting) break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
