#pragma once
#include "config/config_types.hpp"
#include "ui/widgets/iwidget.hpp"
#include <memory>
#include <string>
#include <vector>

struct Panel {
    std::string title;
    std::vector<std::unique_ptr<IWidget>> widgets;
};

class PanelFactory {
public:
    static std::vector<Panel> build(const std::vector<PanelConfig>& panelConfigs,
                                     const std::vector<TagConfig>& tags);
};
