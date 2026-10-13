#include "ui/widgets/button_widget.hpp"
#include <imgui.h>

ButtonWidget::ButtonWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : Widget(tag.id), label_(getParam(cfg, "label", tagDisplayLabel(tag))) {}

void ButtonWidget::render(const ValueMap& snapshot, PlcIoThread& io) {
    bool stale = snapshot.count(tagId_) == 0 || snapshot.at(tagId_).stale;
    ImGui::BeginDisabled(stale);
    if (ImGui::Button(label_.c_str())) {
        // Writes `true` only; nothing writes `false` back afterwards, so the
        // PLC program is expected to reset the bit itself. Assumes a bool tag.
        io.requestWrite(tagId_, true);
    }
    ImGui::EndDisabled();
}
