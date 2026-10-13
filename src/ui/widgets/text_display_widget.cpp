#include "ui/widgets/text_display_widget.hpp"
#include <imgui.h>
#include <cstdio>

TextDisplayWidget::TextDisplayWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : Widget(tag.id),
      label_(getParam(cfg, "label", tagDisplayLabel(tag))),
      format_(getParam(cfg, "format", "%.2f")) {}

void TextDisplayWidget::render(const ValueMap& snapshot, PlcIoThread&) {
    auto it = snapshot.find(tagId_);
    char buf[64];
    if (it == snapshot.end()) {
        std::snprintf(buf, sizeof(buf), "N/A");
    } else {
        // format_ is always float-style (e.g. "%.0f L") regardless of the
        // tag's type, so every value is widened to double first.
        std::snprintf(buf, sizeof(buf), format_.c_str(), plcValueAsDouble(it->second.value));
    }
    if (it != snapshot.end() && it->second.stale) ImGui::TextDisabled("%s: %s (stale)", label_.c_str(), buf);
    else ImGui::Text("%s: %s", label_.c_str(), buf);
}
