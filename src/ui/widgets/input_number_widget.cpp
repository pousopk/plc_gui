#include "ui/widgets/input_number_widget.hpp"
#include <imgui.h>

InputNumberWidget::InputNumberWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id), label_(getParam(cfg, "label", defaultLabel(tag))), type_(tag.type) {}

void InputNumberWidget::render(const ValueMap& snapshot, PlcIoThread& io) {
    auto it = snapshot.find(tagId_);
    bool stale = it == snapshot.end() || it->second.stale;

    ImGui::BeginDisabled(stale);
    if (type_ == PlcType::Real) {
        if (!activeLastFrame_ && it != snapshot.end() && std::holds_alternative<float>(it->second.value)) {
            localFloat_ = std::get<float>(it->second.value);
        }
        ImGui::InputFloat(label_.c_str(), &localFloat_);
        activeLastFrame_ = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) io.requestWrite(tagId_, localFloat_);
    } else {
        if (!activeLastFrame_ && it != snapshot.end() && std::holds_alternative<int32_t>(it->second.value)) {
            localInt_ = std::get<int32_t>(it->second.value);
        }
        ImGui::InputInt(label_.c_str(), &localInt_);
        activeLastFrame_ = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) io.requestWrite(tagId_, static_cast<int32_t>(localInt_));
    }
    ImGui::EndDisabled();
}
