#pragma once
#include "ui/widgets/iwidget.hpp"

class TextDisplayWidget : public Widget {
public:
    TextDisplayWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;
    const std::string& label() const { return label_; }
    const std::string& format() const { return format_; }
private:
    std::string label_;
    std::string format_;
};
