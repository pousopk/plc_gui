#include <doctest/doctest.h>
#include "ui/panel_factory.hpp"
#include "ui/widgets/slider_widget.hpp"

namespace {
TagConfig makeTag(std::string id, PlcType type) {
    TagConfig t;
    t.id = std::move(id);
    t.db = 1;
    t.type = type;
    t.offset = 0;
    t.updateIntervalMs = 100;
    return t;
}
} // namespace

TEST_CASE("PanelFactory builds widgets bound to the correct tag ids and params") {
    std::vector<TagConfig> tags = {makeTag("speed", PlcType::Real), makeTag("running", PlcType::Bool)};

    PanelConfig panel;
    panel.title = "Motor";
    panel.layout = "vertical";

    WidgetConfig slider;
    slider.type = "slider";
    slider.tag = "speed";
    slider.params = {{"min", "0"}, {"max", "3000"}, {"label", "Speed"}};

    WidgetConfig toggle;
    toggle.type = "toggle";
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

TEST_CASE("PanelFactory throws when a widget references a tag not in the tag list") {
    PanelConfig panel;
    panel.title = "P";
    WidgetConfig w;
    w.type = "toggle";
    w.tag = "missing";
    panel.widgets = {w};

    CHECK_THROWS_AS(PanelFactory::build({panel}, {}), std::runtime_error);
}
