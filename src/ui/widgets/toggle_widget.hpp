#pragma once
#include "ui/widgets/iwidget.hpp"

class ToggleWidget : public Widget {
public:
    ToggleWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;
    const std::string& label() const { return label_; }
private:
    std::string label_;
};
