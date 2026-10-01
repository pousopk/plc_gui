#include "ui/panel_factory.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/input_number_widget.hpp"
#include "ui/widgets/led_indicator_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_display_widget.hpp"
#include "ui/widgets/toggle_widget.hpp"
#include <functional>
#include <map>
#include <stdexcept>
#include <unordered_map>

namespace {

using WidgetFactoryFn = std::function<std::unique_ptr<IWidget>(const WidgetConfig&, const TagConfig&)>;

const std::map<std::string, WidgetFactoryFn>& widgetRegistry() {
    static const std::map<std::string, WidgetFactoryFn> registry = {
        {"slider", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<SliderWidget>(c, t); }},
        {"input_number", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<InputNumberWidget>(c, t); }},
        {"toggle", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<ToggleWidget>(c, t); }},
        {"button", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<ButtonWidget>(c, t); }},
        {"text_display", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<TextDisplayWidget>(c, t); }},
        {"led_indicator", [](const WidgetConfig& c, const TagConfig& t) { return std::make_unique<LedIndicatorWidget>(c, t); }},
    };
    return registry;
}

std::unique_ptr<IWidget> buildWidget(const WidgetConfig& wc, const std::unordered_map<std::string, TagConfig>& tagsById) {
    auto tagIt = tagsById.find(wc.tag);
    if (tagIt == tagsById.end()) {
        throw std::runtime_error("PanelFactory: widget references unknown tag '" + wc.tag + "'");
    }
    auto factoryIt = widgetRegistry().find(wc.type);
    if (factoryIt == widgetRegistry().end()) {
        throw std::runtime_error("PanelFactory: unknown widget type '" + wc.type + "'");
    }
    try {
        return factoryIt->second(wc, tagIt->second);
    } catch (const std::exception& e) {
        // Widget constructors parse numeric params (e.g. a slider's min/max)
        // via std::stof/std::stoi, whose exceptions carry no context beyond
        // "stof" -- naming the widget/tag here is the only way to tell which
        // config line is actually broken.
        throw std::runtime_error("PanelFactory: widget '" + wc.type + "' for tag '" + wc.tag +
                                  "' has an invalid parameter: " + e.what());
    }
}

} // namespace

std::vector<Panel> PanelFactory::build(const std::vector<PanelConfig>& panelConfigs,
                                        const std::vector<TagConfig>& tags) {
    std::unordered_map<std::string, TagConfig> tagsById;
    for (const auto& t : tags) tagsById[t.id] = t;

    std::vector<Panel> panels;
    for (const auto& pc : panelConfigs) {
        Panel panel;
        panel.title = pc.title;
        for (const auto& wc : pc.widgets) {
            panel.widgets.push_back(buildWidget(wc, tagsById));
        }
        for (const auto& gc : pc.groups) {
            WidgetGroup group;
            group.title = gc.title;
            for (const auto& wc : gc.widgets) {
                group.widgets.push_back(buildWidget(wc, tagsById));
            }
            panel.groups.push_back(std::move(group));
        }
        panels.push_back(std::move(panel));
    }
    return panels;
}
