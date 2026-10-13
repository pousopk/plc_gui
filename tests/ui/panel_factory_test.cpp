#include <doctest/doctest.h>
#include "ui/panel_factory.hpp"
#include "ui/widgets/led_indicator_widget.hpp"
#include "ui/widgets/slider_widget.hpp"

namespace {
TagConfig makeTag(std::string id, PlcType type, std::string label = "") {
    TagConfig t;
    t.id = std::move(id);
    t.db = 1;
    t.type = type;
    t.offset = 0;
    t.updateIntervalMs = 100;
    t.label = std::move(label);
    return t;
}
} // namespace

TEST_CASE("PanelFactory builds widgets bound to the correct tag ids and params") {
    std::vector<TagConfig> tags = {makeTag("speed", PlcType::Real), makeTag("running", PlcType::Bool)};

    PanelConfig panel;
    panel.title = "Motor";
    panel.layout = "vertical";

    WidgetConfig slider;
    slider.type = WidgetType::Slider;
    slider.tag = "speed";
    slider.params = {{"min", "0"}, {"max", "3000"}, {"label", "Speed"}};

    WidgetConfig toggle;
    toggle.type = WidgetType::Toggle;
    toggle.tag = "running";
    toggle.params = {{"label", "Running"}};

    panel.widgets = {slider, toggle};

    auto panels = PanelFactory::build({panel}, tags);

    REQUIRE(panels.size() == 1);
    REQUIRE(panels[0].widgets.size() == 2);
    CHECK(panels[0].widgets[0]->tagId() == "speed");
    CHECK(panels[0].widgets[1]->tagId() == "running");

    auto* sliderWidget = dynamic_cast<SliderWidget*>(panels[0].widgets[0].get());
    REQUIRE(sliderWidget != nullptr);
    CHECK(sliderWidget->minValue() == doctest::Approx(0.0f));
    CHECK(sliderWidget->maxValue() == doctest::Approx(3000.0f));
    CHECK(sliderWidget->label() == "Speed");
}

TEST_CASE("A widget's caption defaults to the tag's label, but its own label param still wins") {
    std::vector<TagConfig> tags = {
        makeTag("cam1_finished", PlcType::Bool, "Finished"), // has a tag-level label
        makeTag("cam1_ok", PlcType::Bool, "Ok"),              // has a tag-level label, overridden below
    };

    PanelConfig panel;
    panel.title = "Outputs";

    WidgetConfig noOwnLabel;
    noOwnLabel.type = WidgetType::LedIndicator;
    noOwnLabel.tag = "cam1_finished"; // no "label" param -> falls back to tag.label

    WidgetConfig ownLabel;
    ownLabel.type = WidgetType::LedIndicator;
    ownLabel.tag = "cam1_ok";
    ownLabel.params = {{"label", "Custom Ok Caption"}}; // widget's own label wins

    panel.widgets = {noOwnLabel, ownLabel};

    auto panels = PanelFactory::build({panel}, tags);

    REQUIRE(panels[0].widgets.size() == 2);
    auto* w0 = dynamic_cast<LedIndicatorWidget*>(panels[0].widgets[0].get());
    auto* w1 = dynamic_cast<LedIndicatorWidget*>(panels[0].widgets[1].get());
    REQUIRE(w0 != nullptr);
    REQUIRE(w1 != nullptr);
    CHECK(w0->label() == "Finished");
    CHECK(w1->label() == "Custom Ok Caption");
}

TEST_CASE("PanelFactory builds grouped widgets nested inside a single panel") {
    std::vector<TagConfig> tags = {makeTag("cam1_in", PlcType::Bool), makeTag("cam1_finished", PlcType::Bool)};

    PanelConfig panel;
    panel.title = "Camera 1";

    WidgetGroupConfig inputs;
    inputs.title = "Inputs";
    WidgetConfig toggle;
    toggle.type = WidgetType::Toggle;
    toggle.tag = "cam1_in";
    inputs.widgets = {toggle};

    WidgetGroupConfig outputs;
    outputs.title = "Outputs";
    WidgetConfig led;
    led.type = WidgetType::LedIndicator;
    led.tag = "cam1_finished";
    outputs.widgets = {led};

    panel.groups = {inputs, outputs};

    auto panels = PanelFactory::build({panel}, tags);

    REQUIRE(panels.size() == 1);
    CHECK(panels[0].title == "Camera 1");
    CHECK(panels[0].widgets.empty()); // no ungrouped widgets in this panel
    REQUIRE(panels[0].groups.size() == 2);
    CHECK(panels[0].groups[0].title == "Inputs");
    REQUIRE(panels[0].groups[0].widgets.size() == 1);
    CHECK(panels[0].groups[0].widgets[0]->tagId() == "cam1_in");
    CHECK(panels[0].groups[1].title == "Outputs");
    REQUIRE(panels[0].groups[1].widgets.size() == 1);
    CHECK(panels[0].groups[1].widgets[0]->tagId() == "cam1_finished");
}

TEST_CASE("PanelFactory throws when a widget references a tag not in the tag list") {
    PanelConfig panel;
    panel.title = "P";
    WidgetConfig w;
    w.type = WidgetType::Toggle;
    w.tag = "missing";
    panel.widgets = {w};

    CHECK_THROWS_AS(PanelFactory::build({panel}, {}), std::runtime_error);
}

TEST_CASE("PanelFactory names the widget and tag when a numeric param fails to parse") {
    std::vector<TagConfig> tags = {makeTag("speed", PlcType::Real)};

    PanelConfig panel;
    panel.title = "P";
    WidgetConfig slider;
    slider.type = WidgetType::Slider;
    slider.tag = "speed";
    slider.params = {{"min", "not_a_number"}, {"max", "100"}};
    panel.widgets = {slider};

    CHECK_THROWS_WITH_AS(PanelFactory::build({panel}, tags),
                          doctest::Contains("widget 'slider' for tag 'speed' has an invalid parameter"),
                          std::runtime_error);
}
