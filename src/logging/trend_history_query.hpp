#pragma once
#include "plc/plc_value.hpp"
#include "plc/trend_buffer.hpp" // TrendSample
#include <chrono>
#include <string>
#include <vector>

// Read-only historical query over a SqliteLogSink's database, opened via
// its own connection (separate from the sink's write connection) so a slow
// historical query never blocks logging writes.
//
// IMPORTANT: `tagId` here must be each tag's SQLite-stored key — the
// display name LogManager writes rows under (label if set, else id; see
// log_manager.cpp runRule) — NOT necessarily TagConfig::id. See
// ui/trends_screen.hpp for where that distinction is resolved.
//
// `device` must also be supplied (TagConfig::device, empty string if the
// tag has none) and is matched exactly against the row's device column.
// Without it, two devices sharing a device_type blueprint (e.g. two
// cameras both having a "Finished" field) would have identical tag_id
// values and their rows would be silently merged into one series.
class TrendHistoryQuery {
public:
    TrendHistoryQuery(std::string dbPath, std::string table);

    std::vector<TrendSample> query(const std::string& tagId, const std::string& device, PlcType type,
                                    std::chrono::system_clock::time_point from,
                                    std::chrono::system_clock::time_point to) const;

private:
    std::string dbPath_;
    std::string table_;
};
