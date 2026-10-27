#pragma once
#include "config/config_types.hpp"
#include "plc/value_cache.hpp"
#include <lmlib-profinet/PlcIoThread.hpp>
#include <vector>

// The poll/write loop lives in lmlib-profinet; these aliases keep it usable
// unqualified across the app.
using profinet::ConnectionState;
using profinet::PlcIoThread;

// The app's config structs carry GUI/logging-only fields (name, label,
// device, ...) that lmlib-profinet doesn't know about; these strip them down
// to what PlcIoThread needs.
inline profinet::PlcConnectionConfig toProfinetConnection(const PlcConnectionConfig& plc) {
    profinet::PlcConnectionConfig out;
    out.ip = plc.ip;
    out.rack = plc.rack;
    out.slot = plc.slot;
    out.reconnectBackoffMs = plc.reconnectBackoffMs;
    return out;
}

inline std::vector<profinet::TagConfig> toProfinetTags(const std::vector<TagConfig>& tags) {
    std::vector<profinet::TagConfig> out;
    out.reserve(tags.size());
    for (const auto& tag : tags) {
        profinet::TagConfig t;
        t.id = tag.id;
        t.db = tag.db;
        t.type = tag.type;
        t.offset = tag.offset;
        t.bit = tag.bit;
        t.updateIntervalMs = tag.updateIntervalMs;
        out.push_back(std::move(t));
    }
    return out;
}
