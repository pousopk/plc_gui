#include "ui/widgets/slider_widget.hpp"
#include <imgui.h>

SliderWidget::SliderWidget(const WidgetConfig& cfg, const TagConfig& tag)
    : WidgetBase(tag.id),
      label_(getParam(cfg, "label", defaultLabel(tag))),
      minVal_(std::stof(getParam(cfg, "min", "0"))),
      maxVal_(std::stof(getParam(cfg, "max", "100"))) {}

void SliderWidget::render(const ValueMap& snapshot, PlcIoThread& io) {
    auto it = snapshot.find(tagId_);
    bool stale = it == snapshot.end() || it->second.stale;

    if (!activeLastFrame_ && it != snapshot.end() && std::holds_alternative<float>(it->second.value)) {
        localValue_ = std::get<float>(it->second.value);
    }

    ImGui::BeginDisabled(stale);
    ImGui::SliderFloat(label_.c_str(), &localValue_, minVal_, maxVal_);
    activeLastFrame_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        io.requestWrite(tagId_, localValue_);
    }
    ImGui::EndDisabled();
}
