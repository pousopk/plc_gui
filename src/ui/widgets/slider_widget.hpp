#pragma once
#include "ui/widgets/iwidget.hpp"

class SliderWidget : public Widget {
public:
    SliderWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;

    float minValue() const { return minVal_; }
    float maxValue() const { return maxVal_; }
    const std::string& label() const { return label_; }

private:
    std::string label_;
    float minVal_;
    float maxVal_;
    // The value shown in the control. Refreshed from the PLC every frame,
    // EXCEPT while the user is interacting with the control
    // (activeLastFrame_), so a live update can't overwrite a value they are
    // still editing. The edited value is written to the PLC only once the
    // user finishes (ImGui::IsItemDeactivatedAfterEdit).
    float localValue_ = 0.0f;
    bool activeLastFrame_ = false;
};
