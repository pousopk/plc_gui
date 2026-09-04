#include "ui/widgets/text_display_widget.hpp"
#include <imgui.h>
#include <cstdio>

namespace {
// format_ strings are always float-style (e.g. "%.0f L") regardless of the
// tag's underlying type, matching the config schema's example — so every
// value is widened to double before formatting.
double asDouble(const PlcValue& v) {
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0 : 0.0;
    if (std::holds_alternative<int32_t>(v)) return static_cast<double>(std::get<int32_t>(v));
    return static_cast<double>(std::get<float>(v));
}
} // namespace

TextDisplayWidget::TextDisplayWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", tag.id)), format_(getParam(cfg, "format", "%.2f")) {}

void TextDisplayWidget::render(const ValueMap& snapshot, PlcIoThread&) {
    auto it = snapshot.find(tagId_);
    char buf[64];
    if (it == snapshot.end()) {
        std::snprintf(buf, sizeof(buf), "N/A");
    } else {
        std::snprintf(buf, sizeof(buf), format_.c_str(), asDouble(it->second.value));
    }
    if (it != snapshot.end() && it->second.stale) ImGui::TextDisabled("%s: %s (stale)", label_.c_str(), buf);
    else ImGui::Text("%s: %s", label_.c_str(), buf);
}
