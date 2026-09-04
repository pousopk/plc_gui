#include "ui/widgets/led_indicator_widget.hpp"
#include <imgui.h>

LedIndicatorWidget::LedIndicatorWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", defaultLabel(tag))) {}

void LedIndicatorWidget::render(const ValueMap& snapshot, PlcIoThread&) {
    auto it = snapshot.find(tagId_);
    bool on = it != snapshot.end() && std::holds_alternative<bool>(it->second.value) && std::get<bool>(it->second.value);
    bool stale = it == snapshot.end() || it->second.stale;
    ImVec4 color = stale ? ImVec4(0.5f, 0.5f, 0.5f, 1.0f) : (on ? ImVec4(0.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.6f, 0.0f, 0.0f, 1.0f));

    // Drawn as a filled circle via the draw list rather than a Unicode glyph
    // (e.g. U+25CF) — ImGui's default font is ASCII-only and renders any
    // out-of-range codepoint as a "missing glyph" placeholder.
    float lineHeight = ImGui::GetTextLineHeight();
    float radius = lineHeight * 0.35f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center(cursor.x + radius, cursor.y + lineHeight * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(color));
    ImGui::Dummy(ImVec2(radius * 2.0f + 4.0f, lineHeight));
    ImGui::SameLine();
    ImGui::Text("%s", label_.c_str());
}
