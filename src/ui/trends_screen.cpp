#include "ui/trends_screen.hpp"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <iterator>
#include <unordered_map>

namespace {
// One label per TrendHistoryRange value, in enum order.
constexpr const char* kRangeLabels[] = {"Last hour", "Last 24h", "Last 7 days"};
constexpr int kRangeCount = static_cast<int>(std::size(kRangeLabels));

// Fractional padding added to the auto-fit extents: (x, y).
constexpr ImVec2 kFitPadding(0.02f, 0.1f);

// ImPlot only auto-fits axes to data once, on the very first frame a given
// plot ID is ever rendered (implot.cpp: `!plot.Initialized` gate). That first
// frame typically has zero tags checked, so the one-time fit locks onto an
// empty-data default range forever, leaving any later-plotted data (real Unix
// timestamps) far outside it and invisible. ImPlotAxisFlags_AutoFit makes both
// axes refit to the actual data extents every frame instead of just once.
void setupTrendAxes() {
    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
}

double toPlotX(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration<double>(tp.time_since_epoch()).count();
}

} // namespace

std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>
trendHistoryRangeToTimestamps(TrendHistoryRange range, std::chrono::system_clock::time_point now) {
    std::chrono::hours lookback{1};
    switch (range) {
        case TrendHistoryRange::LastHour:    lookback = std::chrono::hours(1); break;
        case TrendHistoryRange::Last24Hours: lookback = std::chrono::hours(24); break;
        case TrendHistoryRange::Last7Days:   lookback = std::chrono::hours(24 * 7); break;
    }
    return {now - lookback, now};
}

std::vector<TrendTagInfo> buildTrendTagList(const std::vector<TagConfig>& tags) {
    std::vector<TrendTagInfo> list;
    list.reserve(tags.size());
    for (const auto& tag : tags) {
        std::string label = tagDisplayLabel(tag);
        std::string displayName = tag.device.empty() ? label : (tag.device + " - " + label);
        list.push_back(TrendTagInfo{tag.id, label, displayName, tag.device, tag.type});
    }
    return list;
}

std::vector<TrendDeviceGroup> groupTrendTagsByDevice(const std::vector<TrendTagInfo>& tagList) {
    std::vector<TrendDeviceGroup> groups;
    std::unordered_map<std::string, size_t> indexByDevice;
    for (const auto& tag : tagList) {
        auto it = indexByDevice.find(tag.device);
        if (it == indexByDevice.end()) {
            indexByDevice[tag.device] = groups.size();
            groups.push_back(TrendDeviceGroup{tag.device, {tag}});
        } else {
            groups[it->second].tags.push_back(tag);
        }
    }
    return groups;
}

std::unordered_map<std::string, std::vector<TrendSample>> refreshTrendHistoryCache(
    TrendHistoryQuery& historyQuery,
    const std::vector<TrendTagInfo>& tagList,
    const std::unordered_map<std::string, bool>& checked,
    TrendHistoryRange range,
    std::chrono::system_clock::time_point now) {
    std::unordered_map<std::string, std::vector<TrendSample>> result;
    auto [from, to] = trendHistoryRangeToTimestamps(range, now);
    for (const auto& tag : tagList) {
        auto it = checked.find(tag.id);
        if (it == checked.end() || !it->second) continue;
        result[tag.id] = historyQuery.query(tag.label, tag.device, tag.type, from, to);
    }
    return result;
}

TrendsScreen::TrendsScreen(std::vector<TagConfig> tags, TrendBuffer& trendBuffer, TrendHistoryQuery* historyQuery)
    : tagList_(buildTrendTagList(tags)), trendBuffer_(trendBuffer), historyQuery_(historyQuery) {
    for (const auto& tag : tagList_) checked_[tag.id] = false;
}

void TrendsScreen::refreshHistoryCacheIfNeeded() {
    if (!historyCacheDirty_ || !historyQuery_) return;
    historyCache_ = refreshTrendHistoryCache(*historyQuery_, tagList_, checked_, selectedRange_,
                                              std::chrono::system_clock::now());
    historyCacheDirty_ = false;
}

std::vector<TrendSample> TrendsScreen::samplesFor(const TrendTagInfo& tag) const {
    if (showLive_) return trendBuffer_.snapshot(tag.id);

    auto it = historyCache_.find(tag.id);
    if (it == historyCache_.end()) return {};
    return it->second;
}

void TrendsScreen::plotTag(const TrendTagInfo& tag) {
    std::vector<TrendSample> samples = samplesFor(tag);
    if (samples.empty()) return;

    std::vector<double> xs, ys;
    xs.reserve(samples.size());
    ys.reserve(samples.size());
    for (const auto& sample : samples) {
        xs.push_back(toPlotX(sample.timestamp));
        ys.push_back(plcValueAsDouble(sample.value));
    }

    if (tag.type == PlcType::Bool) {
        ImPlot::PlotStairs(tag.displayName.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
    } else {
        ImPlot::PlotLine(tag.displayName.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
    }
}

void TrendsScreen::render() {
    renderTagChecklist();
    ImGui::SameLine();

    ImGui::BeginChild("TrendChartArea", ImVec2(0.0f, 0.0f), 0);
    renderModeSelector();
    if (!showLive_) refreshHistoryCacheIfNeeded();
    renderChart();
    ImGui::EndChild();
}

// Left column: one collapsible section per device, each with a
// "select all" checkbox and one checkbox per tag.
void TrendsScreen::renderTagChecklist() {
    ImGui::BeginChild("TrendTagList", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders);
    for (const auto& group : groupTrendTagsByDevice(tagList_)) {
        ImGui::PushID(group.device.c_str());

        bool allChecked = !group.tags.empty() &&
            std::all_of(group.tags.begin(), group.tags.end(),
                        [&](const TrendTagInfo& tag) { return checked_[tag.id]; });
        if (ImGui::Checkbox("##selectAll", &allChecked)) {
            for (const auto& tag : group.tags) checked_[tag.id] = allChecked;
            historyCacheDirty_ = true;
        }
        ImGui::SameLine();
        bool open = ImGui::CollapsingHeader(group.device.empty() ? "Other" : group.device.c_str());

        if (open) {
            ImGui::Indent();
            for (const auto& tag : group.tags) {
                ImGui::PushID(tag.id.c_str());
                bool wasChecked = checked_[tag.id];
                ImGui::Checkbox(tag.label.c_str(), &checked_[tag.id]);
                if (checked_[tag.id] != wasChecked) historyCacheDirty_ = true;
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// Top bar: Live / historical range radio buttons, then Overlay / Stacked.
void TrendsScreen::renderModeSelector() {
    if (ImGui::RadioButton("Live", showLive_)) showLive_ = true;
    ImGui::SameLine();
    for (int i = 0; i < kRangeCount; ++i) {
        auto range = static_cast<TrendHistoryRange>(i);
        ImGui::BeginDisabled(historyQuery_ == nullptr);
        if (ImGui::RadioButton(kRangeLabels[i], !showLive_ && selectedRange_ == range)) {
            showLive_ = false;
            selectedRange_ = range;
            historyCacheDirty_ = true;
        }
        ImGui::EndDisabled();
        if (i + 1 < kRangeCount) ImGui::SameLine();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::RadioButton("Overlay", !stacked_)) stacked_ = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Stacked", stacked_)) stacked_ = true;
}

void TrendsScreen::renderChart() {
    // Extra room around the auto-fit extents so series that sit at the
    // edge of their range (bool 0/1 steps, flat-lined values) aren't drawn
    // right on the plot border.
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, kFitPadding);
    if (stacked_) {
        std::vector<const TrendTagInfo*> checkedTags;
        for (const auto& tag : tagList_) {
            if (checked_[tag.id]) checkedTags.push_back(&tag);
        }
        if (!checkedTags.empty() &&
            ImPlot::BeginSubplots("##TrendStack", static_cast<int>(checkedTags.size()), 1, ImVec2(-1, -1),
                                  ImPlotSubplotFlags_LinkAllX)) {
            for (const TrendTagInfo* tag : checkedTags) {
                // Title names the tag, so the single-entry legend is redundant;
                // "##id" keeps the plot ID unique even if display names collide.
                std::string title = tag->displayName + "##" + tag->id;
                if (ImPlot::BeginPlot(title.c_str(), ImVec2(), ImPlotFlags_NoLegend)) {
                    setupTrendAxes();
                    plotTag(*tag);
                    ImPlot::EndPlot();
                }
            }
            ImPlot::EndSubplots();
        }
    } else if (ImPlot::BeginPlot("##TrendChart", ImVec2(-1, -1))) {
        setupTrendAxes();
        for (const auto& tag : tagList_) {
            if (checked_[tag.id]) plotTag(tag);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}
