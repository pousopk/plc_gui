#include "ui/widgets/led_indicator_widget.hpp"
#include "ui/draw_helpers.hpp"
#include <imgui.h>

namespace {
const ImVec4 kStaleColor(0.5f, 0.5f, 0.5f, 1.0f);
const ImVec4 kOnColor(0.0f, 0.8f, 0.0f, 1.0f);
const ImVec4 kOffColor(0.6f, 0.0f, 0.0f, 1.0f);
} // namespace

LedIndicatorWidget::LedIndicatorWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : Widget(tag.id), label_(getParam(cfg, "label", tagDisplayLabel(tag))) {}

void LedIndicatorWidget::render(const ValueMap& snapshot, PlcIoThread&) {
    auto it = snapshot.find(tagId_);
    bool hasValue = it != snapshot.end();
    bool stale = !hasValue || it->second.stale;
    bool on = hasValue && std::holds_alternative<bool>(it->second.value) && std::get<bool>(it->second.value);

    ImVec4 color = kOffColor;
    if (stale) {
        color = kStaleColor;
    } else if (on) {
        color = kOnColor;
    }

    drawFilledDot(color, ImGui::GetTextLineHeight(), 4.0f);
    ImGui::Text("%s", label_.c_str());
}
