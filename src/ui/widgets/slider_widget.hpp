#pragma once
#include "ui/widgets/iwidget.hpp"

class SliderWidget : public WidgetBase {
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
    float localValue_ = 0.0f;
    bool activeLastFrame_ = false;
};
