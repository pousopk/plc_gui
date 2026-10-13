#pragma once
#include "ui/widgets/iwidget.hpp"

class LedIndicatorWidget : public Widget {
public:
    LedIndicatorWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;
    const std::string& label() const { return label_; }
private:
    std::string label_;
};
