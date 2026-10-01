#include <doctest/doctest.h>
#include <imgui.h>
#include <implot.h>

TEST_CASE("ImPlot context can be created and destroyed") {
    ImGui::CreateContext();
    ImPlotContext* ctx = ImPlot::CreateContext();
    CHECK(ctx != nullptr);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}
