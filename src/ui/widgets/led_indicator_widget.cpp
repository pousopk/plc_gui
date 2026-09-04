#include "ui/widgets/led_indicator_widget.hpp"
#include <imgui.h>

LedIndicatorWidget::LedIndicatorWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", tag.id)) {}

void LedIndicatorWidget::render(const ValueMap& snapshot, PlcIoThread&) {
    auto it = snapshot.find(tagId_);
    bool on = it != snapshot.end() && std::holds_alternative<bool>(it->second.value) && std::get<bool>(it->second.value);
    bool stale = it == snapshot.end() || it->second.stale;
    ImVec4 color = stale ? ImVec4(0.5f, 0.5f, 0.5f, 1.0f) : (on ? ImVec4(0.0f, 0.8f, 0.0f, 1.0f) : ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
    ImGui::TextColored(color, "\xE2\x97\x8F"); // U+25CF BLACK CIRCLE
    ImGui::SameLine();
    ImGui::Text("%s", label_.c_str());
}
