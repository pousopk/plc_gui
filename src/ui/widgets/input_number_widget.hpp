#pragma once
#include "ui/widgets/iwidget.hpp"

class InputNumberWidget : public Widget {
public:
    InputNumberWidget(const WidgetConfig& cfg, const TagConfig& tag);
    void render(const ValueMap& snapshot, PlcIoThread& io) override;

    const std::string& label() const { return label_; }
    PlcType valueType() const { return type_; }

private:
    std::string label_;
    PlcType type_;
    // The value shown in the control (localFloat_ for Real tags, localInt_
    // otherwise). Refreshed from the PLC every frame, EXCEPT while the user
    // is interacting with the control (activeLastFrame_), so a live update
    // can't overwrite a value they are still editing. The edited value is
    // written to the PLC only once the user finishes
    // (ImGui::IsItemDeactivatedAfterEdit).
    float localFloat_ = 0.0f;
    int localInt_ = 0;
    bool activeLastFrame_ = false;
};
