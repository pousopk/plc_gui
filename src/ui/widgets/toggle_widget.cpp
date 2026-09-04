#include "ui/widgets/toggle_widget.hpp"
#include <imgui.h>

ToggleWidget::ToggleWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", tag.id)) {}

void ToggleWidget::render(const ValueMap& snapshot, PlcIoThread& io) {
    auto it = snapshot.find(tagId_);
    bool stale = it == snapshot.end() || it->second.stale;
    bool value = (it != snapshot.end() && std::holds_alternative<bool>(it->second.value))
                     ? std::get<bool>(it->second.value) : false;

    ImGui::BeginDisabled(stale);
    if (ImGui::Checkbox(label_.c_str(), &value)) {
        io.requestWrite(tagId_, value);
    }
    ImGui::EndDisabled();
}
