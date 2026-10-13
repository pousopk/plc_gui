#include "ui/panel_factory.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/input_number_widget.hpp"
#include "ui/widgets/led_indicator_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_display_widget.hpp"
#include "ui/widgets/toggle_widget.hpp"
#include <stdexcept>
#include <unordered_map>

namespace {

std::unique_ptr<Widget> createWidget(const WidgetConfig& widgetConfig, const TagConfig& tag) {
    switch (widgetConfig.type) {
        case WidgetType::Slider:       return std::make_unique<SliderWidget>(widgetConfig, tag);
        case WidgetType::InputNumber:  return std::make_unique<InputNumberWidget>(widgetConfig, tag);
        case WidgetType::Toggle:       return std::make_unique<ToggleWidget>(widgetConfig, tag);
        case WidgetType::Button:       return std::make_unique<ButtonWidget>(widgetConfig, tag);
        case WidgetType::TextDisplay:  return std::make_unique<TextDisplayWidget>(widgetConfig, tag);
        case WidgetType::LedIndicator: return std::make_unique<LedIndicatorWidget>(widgetConfig, tag);
    }
    throw std::runtime_error("PanelFactory: unknown widget type");
}

std::unique_ptr<Widget> buildWidget(const WidgetConfig& widgetConfig,
                                     const std::unordered_map<std::string, TagConfig>& tagsById) {
    auto tagIt = tagsById.find(widgetConfig.tag);
    if (tagIt == tagsById.end()) {
        throw std::runtime_error("PanelFactory: widget references unknown tag '" + widgetConfig.tag + "'");
    }
    try {
        return createWidget(widgetConfig, tagIt->second);
    } catch (const std::exception& e) {
        // Widget constructors parse numeric params (e.g. a slider's min/max)
        // via std::stof/std::stoi, whose exceptions carry no context beyond
        // "stof" -- naming the widget/tag here is the only way to tell which
        // config line is actually broken.
        throw std::runtime_error(std::string("PanelFactory: widget '") + widgetTypeName(widgetConfig.type) +
                                 "' for tag '" + widgetConfig.tag + "' has an invalid parameter: " + e.what());
    }
}

} // namespace

std::vector<Panel> PanelFactory::build(const std::vector<PanelConfig>& panelConfigs,
                                        const std::vector<TagConfig>& tags) {
    std::unordered_map<std::string, TagConfig> tagsById;
    for (const auto& tag : tags) tagsById[tag.id] = tag;

    std::vector<Panel> panels;
    for (const auto& panelConfig : panelConfigs) {
        Panel panel;
        panel.title = panelConfig.title;
        for (const auto& widgetConfig : panelConfig.widgets) {
            panel.widgets.push_back(buildWidget(widgetConfig, tagsById));
        }
        for (const auto& groupConfig : panelConfig.groups) {
            WidgetGroup group;
            group.title = groupConfig.title;
            for (const auto& widgetConfig : groupConfig.widgets) {
                group.widgets.push_back(buildWidget(widgetConfig, tagsById));
            }
            panel.groups.push_back(std::move(group));
        }
        panels.push_back(std::move(panel));
    }
    return panels;
}
