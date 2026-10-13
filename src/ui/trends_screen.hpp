#pragma once
#include "config/config_types.hpp"
#include "logging/trend_history_query.hpp"
#include "plc/trend_buffer.hpp"
#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class TrendHistoryRange { LastHour, Last24Hours, Last7Days };

// [from, to] window for a historical range selection, anchored at `now`.
std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
trendHistoryRangeToTimestamps(TrendHistoryRange range, std::chrono::system_clock::time_point now);

struct TrendTagInfo {
    std::string id;          // TagConfig::id -- the key TrendBuffer (live) uses
    std::string label;       // SQLite-stored key (label-or-id) -- see TrendHistoryQuery. NOT
                              // for display: tags from different devices sharing a device_type
                              // blueprint (e.g. two cameras) commonly share this same label.
    std::string displayName; // UI-only text for the checklist/legend: label prefixed with
                              // TagConfig::device when set (e.g. "Camera 1 - Finished"), so
                              // same-label tags from different devices stay distinguishable.
                              // Never used as a query key.
    std::string device;      // TagConfig::device, passed to TrendHistoryQuery::query() so rows
                              // from a same-labeled tag on a different device aren't merged in.
    PlcType type;
};

// One entry per configured tag, in config order. `label` is
// tagDisplayLabel(tag) -- the same name LogManager stores rows under.
std::vector<TrendTagInfo> buildTrendTagList(const std::vector<TagConfig>& tags);

struct TrendDeviceGroup {
    std::string device; // empty string = tags with no configured device ("Other" in the UI)
    std::vector<TrendTagInfo> tags;
};

// Buckets tagList into one group per distinct TagConfig::device, in the
// order each device is first encountered (so groups come out in config
// order, e.g. cam1's tags before cam2's). Tags with no device share one
// group keyed by the empty string.
std::vector<TrendDeviceGroup> groupTrendTagsByDevice(const std::vector<TrendTagInfo>& tagList);

// Runs one historical query per checked tag (querying by `label`, the
// SQLite-stored key) for the given range, returning results keyed by `id`
// (so callers can look them up the same way they look up TrendBuffer).
std::unordered_map<std::string, std::vector<TrendSample>> refreshTrendHistoryCache(
    TrendHistoryQuery& historyQuery,
    const std::vector<TrendTagInfo>& tagList,
    const std::unordered_map<std::string, bool>& checked,
    TrendHistoryRange range,
    std::chrono::system_clock::time_point now);

// Renders the "Trends" tab: a tag checklist, a Live/historical range
// selector, an Overlay/Stacked layout toggle, and an ImPlot chart of every
// checked tag's series -- either overlaid on one plot, or stacked one plot
// per tag with linked time axes (numeric tags as a line, PlcType::Bool tags
// as a step plot). Historical range buttons are disabled when historyQuery
// is nullptr (no sqlite sink configured) -- Live mode (reading trendBuffer)
// always works.
class TrendsScreen {
public:
    TrendsScreen(std::vector<TagConfig> tags, TrendBuffer& trendBuffer, TrendHistoryQuery* historyQuery);

    void render();

private:
    void renderTagChecklist();
    void renderModeSelector();
    void renderChart();

    void refreshHistoryCacheIfNeeded();
    // t's samples for the current mode: the live buffer, or the cached
    // historical query result.
    std::vector<TrendSample> samplesFor(const TrendTagInfo& tag) const;
    // Plots t's samples into the current ImPlot plot.
    void plotTag(const TrendTagInfo& tag);

    std::vector<TrendTagInfo> tagList_;
    TrendBuffer& trendBuffer_;
    TrendHistoryQuery* historyQuery_;

    std::unordered_map<std::string, bool> checked_; // keyed by TrendTagInfo::id
    bool showLive_ = true;
    bool stacked_ = false; // true = one linked-X subplot per checked tag; false = all tags overlaid
    TrendHistoryRange selectedRange_ = TrendHistoryRange::LastHour;
    bool historyCacheDirty_ = true;
    std::unordered_map<std::string, std::vector<TrendSample>> historyCache_; // keyed by id
};
