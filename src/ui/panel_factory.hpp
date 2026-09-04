#pragma once
#include "config/config_types.hpp"
#include "ui/widgets/iwidget.hpp"
#include <memory>
#include <string>
#include <vector>

struct WidgetGroup {
    std::string title;
    std::vector<std::unique_ptr<IWidget>> widgets;
};

struct Panel {
    std::string title;
    std::vector<std::unique_ptr<IWidget>> widgets; // ungrouped
    std::vector<WidgetGroup> groups;                // labeled sub-sections
};

class PanelFactory {
public:
    static std::vector<Panel> build(const std::vector<PanelConfig>& panelConfigs,
                                     const std::vector<TagConfig>& tags);
};
