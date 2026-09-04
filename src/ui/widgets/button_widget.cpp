#include "ui/widgets/button_widget.hpp"
#include <imgui.h>

ButtonWidget::ButtonWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", tag.id)) {}

void ButtonWidget::render(const ValueMap& snapshot, PlcIoThread& io) {
    bool stale = snapshot.count(tagId_) == 0 || snapshot.at(tagId_).stale;
    ImGui::BeginDisabled(stale);
    if (ImGui::Button(label_.c_str())) {
        io.requestWrite(tagId_, true); // momentary pulse; assumes a bool tag
    }
    ImGui::EndDisabled();
}
