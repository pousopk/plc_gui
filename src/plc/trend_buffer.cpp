#include "plc/trend_buffer.hpp"

TrendBuffer::TrendBuffer(size_t capacityPerTag) : capacityPerTag_(capacityPerTag) {}

void TrendBuffer::append(const std::string& tagId, std::chrono::system_clock::time_point timestamp, PlcValue value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& buf = buffers_[tagId];
    buf.push_back(TrendSample{timestamp, value});
    while (buf.size() > capacityPerTag_) buf.pop_front();
}

std::vector<TrendSample> TrendBuffer::snapshot(const std::string& tagId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(tagId);
    if (it == buffers_.end()) return {};
    return std::vector<TrendSample>(it->second.begin(), it->second.end());
}
