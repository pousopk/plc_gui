#pragma once
#include "ui/widgets/iwidget.hpp"

class InputNumberWidget : public WidgetBase {
public:
    InputNumberWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;

    const std::string& label() const { return label_; }
    PlcType valueType() const { return type_; }

private:
    std::string label_;
    PlcType type_;
    float localFloat_ = 0.0f;
    int localInt_ = 0;
    bool activeLastFrame_ = false;
};
