#include "ui/trends_screen.hpp"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace {
constexpr const char* kRangeLabels[] = {"Last hour", "Last 24h", "Last 7 days"};

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

double toPlotY(const PlcValue& v) {
    return std::visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) return val ? 1.0 : 0.0;
        else return static_cast<double>(val);
    }, v);
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
    for (const auto& t : tags) {
        std::string label = t.label.empty() ? t.id : t.label;
        std::string displayName = t.device.empty() ? label : (t.device + " - " + label);
        list.push_back(TrendTagInfo{t.id, label, displayName, t.device, t.type});
    }
    return list;
}

std::vector<TrendDeviceGroup> groupTrendTagsByDevice(const std::vector<TrendTagInfo>& tagList) {
    std::vector<TrendDeviceGroup> groups;
    std::unordered_map<std::string, size_t> indexByDevice;
    for (const auto& t : tagList) {
        auto it = indexByDevice.find(t.device);
        if (it == indexByDevice.end()) {
            indexByDevice[t.device] = groups.size();
            groups.push_back(TrendDeviceGroup{t.device, {t}});
        } else {
            groups[it->second].tags.push_back(t);
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
    for (const auto& t : tagList) {
        auto it = checked.find(t.id);
        if (it == checked.end() || !it->second) continue;
        result[t.id] = historyQuery.query(t.label, t.device, t.type, from, to);
    }
    return result;
}

TrendsScreen::TrendsScreen(std::vector<TagConfig> tags, TrendBuffer& trendBuffer, TrendHistoryQuery* historyQuery)
    : tagList_(buildTrendTagList(tags)), trendBuffer_(trendBuffer), historyQuery_(historyQuery) {
    for (const auto& t : tagList_) checked_[t.id] = false;
}

void TrendsScreen::refreshHistoryCacheIfNeeded() {
    if (!historyCacheDirty_ || !historyQuery_) return;
    historyCache_ = refreshTrendHistoryCache(*historyQuery_, tagList_, checked_, selectedRange_,
                                              std::chrono::system_clock::now());
    historyCacheDirty_ = false;
}

void TrendsScreen::plotTag(const TrendTagInfo& t) {
    const std::vector<TrendSample>& samples = showLive_
        ? trendBuffer_.snapshot(t.id)
        : (historyCache_.count(t.id) ? historyCache_.at(t.id) : std::vector<TrendSample>{});
    if (samples.empty()) return;

    std::vector<double> xs, ys;
    xs.reserve(samples.size());
    ys.reserve(samples.size());
    for (const auto& s : samples) {
        xs.push_back(toPlotX(s.timestamp));
        ys.push_back(toPlotY(s.value));
    }

    if (t.type == PlcType::Bool) {
        ImPlot::PlotStairs(t.displayName.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
    } else {
        ImPlot::PlotLine(t.displayName.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
    }
}

void TrendsScreen::render() {
    ImGui::BeginChild("TrendTagList", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders);
    for (const auto& group : groupTrendTagsByDevice(tagList_)) {
        ImGui::PushID(group.device.c_str());

        bool allChecked = !group.tags.empty() &&
            std::all_of(group.tags.begin(), group.tags.end(),
                        [&](const TrendTagInfo& t) { return checked_[t.id]; });
        if (ImGui::Checkbox("##selectAll", &allChecked)) {
            for (const auto& t : group.tags) checked_[t.id] = allChecked;
            historyCacheDirty_ = true;
        }
        ImGui::SameLine();
        bool open = ImGui::CollapsingHeader(group.device.empty() ? "Other" : group.device.c_str());

        if (open) {
            ImGui::Indent();
            for (const auto& t : group.tags) {
                ImGui::PushID(t.id.c_str());
                bool wasChecked = checked_[t.id];
                ImGui::Checkbox(t.label.c_str(), &checked_[t.id]);
                if (checked_[t.id] != wasChecked) historyCacheDirty_ = true;
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("TrendChartArea", ImVec2(0.0f, 0.0f), 0);

    if (ImGui::RadioButton("Live", showLive_)) showLive_ = true;
    ImGui::SameLine();
    for (int i = 0; i < 3; ++i) {
        auto range = static_cast<TrendHistoryRange>(i);
        ImGui::BeginDisabled(historyQuery_ == nullptr);
        if (ImGui::RadioButton(kRangeLabels[i], !showLive_ && selectedRange_ == range)) {
            showLive_ = false;
            selectedRange_ = range;
            historyCacheDirty_ = true;
        }
        ImGui::EndDisabled();
        if (i + 1 < 3) ImGui::SameLine();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::RadioButton("Overlay", !stacked_)) stacked_ = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Stacked", stacked_)) stacked_ = true;

    if (!showLive_) refreshHistoryCacheIfNeeded();

    // Extra room around the auto-fit extents so series that sit at the
    // edge of their range (bool 0/1 steps, flat-lined values) aren't drawn
    // right on the plot border.
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, kFitPadding);
    if (stacked_) {
        std::vector<const TrendTagInfo*> checkedTags;
        for (const auto& t : tagList_) {
            if (checked_[t.id]) checkedTags.push_back(&t);
        }
        if (!checkedTags.empty() &&
            ImPlot::BeginSubplots("##TrendStack", static_cast<int>(checkedTags.size()), 1, ImVec2(-1, -1),
                                  ImPlotSubplotFlags_LinkAllX)) {
            for (const TrendTagInfo* t : checkedTags) {
                // Title names the tag, so the single-entry legend is redundant;
                // "##id" keeps the plot ID unique even if display names collide.
                std::string title = t->displayName + "##" + t->id;
                if (ImPlot::BeginPlot(title.c_str(), ImVec2(), ImPlotFlags_NoLegend)) {
                    setupTrendAxes();
                    plotTag(*t);
                    ImPlot::EndPlot();
                }
            }
            ImPlot::EndSubplots();
        }
    } else if (ImPlot::BeginPlot("##TrendChart", ImVec2(-1, -1))) {
        setupTrendAxes();
        for (const auto& t : tagList_) {
            if (checked_[t.id]) plotTag(t);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();

    ImGui::EndChild();
}
