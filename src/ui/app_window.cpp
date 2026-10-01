#include "ui/app_window.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace {
void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Card layout tuning. Cards target this width; the grid fits as many
// columns as the window allows (at least one) and stretches them evenly
// to fill the remaining space.
constexpr float kCardMinWidth = 340.0f;
constexpr float kCardSpacing = 12.0f;

// Draws a small filled circle inline with the following text, matching
// LedIndicatorWidget's approach (the default ImGui font is ASCII-only, so
// a Unicode dot glyph would render as a missing-glyph box).
void drawStatusDot(ImVec4 color, float lineHeight) {
    float radius = lineHeight * 0.35f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center(cursor.x + radius, cursor.y + lineHeight * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(color));
    ImGui::Dummy(ImVec2(radius * 2.0f + 6.0f, lineHeight));
    ImGui::SameLine();
}
} // namespace

AppWindow::AppWindow(std::string plcName, std::vector<Panel> panels, ValueCache& cache, PlcIoThread& io,
                      std::vector<TagConfig> tags, TrendBuffer& trendBuffer, TrendHistoryQuery* historyQuery)
    : plcName_(std::move(plcName)), panels_(std::move(panels)), cache_(cache), io_(io),
      trendsScreen_(std::move(tags), trendBuffer, historyQuery) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) throw std::runtime_error("AppWindow: glfwInit failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window_ = glfwCreateWindow(1280, 720, "PLC GUI", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("AppWindow: glfwCreateWindow failed");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    applyTheme();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

AppWindow::~AppWindow() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

// A neutral gray/steel palette on a dark charcoal background: it stays out
// of the way of the widgets' own semantics, leaving the LED indicators'
// green (ok) / red (fault) as the only saturated colors on screen.
void AppWindow::applyTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;

    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 18.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.27f, 0.27f, 0.29f, 1.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.34f, 0.37f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.28f, 0.28f, 0.31f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.36f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.46f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.78f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.62f, 0.62f, 0.66f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.78f, 0.78f, 0.80f, 1.00f);

    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.46f, 0.46f, 0.50f, 1.00f);
}

const char* AppWindow::connectionStateLabel(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Connecting: return "Connecting...";
        case ConnectionState::Connected: return "Connected";
        case ConnectionState::Reconnecting: return "Reconnecting...";
    }
    return "Unknown";
}

ImVec4 AppWindow::connectionStateColor(ConnectionState state) {
    switch (state) {
        case ConnectionState::Connected: return ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
        case ConnectionState::Connecting:
        case ConnectionState::Reconnecting: return ImVec4(0.85f, 0.65f, 0.0f, 1.0f);
        case ConnectionState::Disconnected: return ImVec4(0.6f, 0.0f, 0.0f, 1.0f);
    }
    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
}

void AppWindow::renderStatusHeader() {
    ConnectionState state = io_.connectionState();
    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    drawStatusDot(connectionStateColor(state), lineHeight);
    ImGui::TextColored(connectionStateColor(state), "%s", connectionStateLabel(state));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

// Renders one device panel as a bordered "card": a title bar, then its
// ungrouped widgets, then each labeled group (e.g. "Inputs"/"Outputs") as
// its own sub-section — same widget->render() calls as before, just
// wrapped in card chrome instead of a flat CollapsingHeader stack.
void AppWindow::renderCard(Panel& panel, const ValueMap& snapshot, float cardWidth) {
    ImGui::BeginChild(panel.title.c_str(), ImVec2(cardWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.87f, 1.0f));
    ImGui::TextUnformatted(panel.title.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    for (auto& widget : panel.widgets) {
        widget->render(snapshot, io_);
    }

    for (size_t gi = 0; gi < panel.groups.size(); ++gi) {
        auto& group = panel.groups[gi];
        ImGui::PushID(static_cast<int>(gi));
        if (gi > 0) ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextDisabled("%s", group.title.c_str());
        ImGui::Spacing();
        for (auto& widget : group.widgets) {
            widget->render(snapshot, io_);
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// A responsive grid: as many columns as fit at kCardMinWidth each, evenly
// stretched to fill the available width, wrapping to the next row.
void AppWindow::renderPanelGrid(const ValueMap& snapshot) {
    float availWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>((availWidth + kCardSpacing) / (kCardMinWidth + kCardSpacing)));
    columns = std::min(columns, static_cast<int>(panels_.size()) > 0 ? static_cast<int>(panels_.size()) : 1);
    float cardWidth = (availWidth - kCardSpacing * static_cast<float>(columns - 1)) / static_cast<float>(columns);

    for (size_t pi = 0; pi < panels_.size(); ++pi) {
        ImGui::PushID(static_cast<int>(pi));
        renderCard(panels_[pi], snapshot, cardWidth);
        ImGui::PopID();

        bool lastInRow = (pi + 1) % static_cast<size_t>(columns) == 0;
        bool lastPanel = pi + 1 == panels_.size();
        if (!lastPanel && !lastInRow) {
            ImGui::SameLine(0.0f, kCardSpacing);
        }
    }
}

void AppWindow::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto snap = cache_.snapshot();

    // One window for the whole PLC connection: a status header at the
    // top, then every device panel laid out as a card in a responsive
    // grid below it — matching the single-PLC-connection architecture,
    // where "which PLC" is the outermost, non-repeating level of the UI.
    ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);
    ImGui::Begin(plcName_.c_str());
    renderStatusHeader();

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Devices")) {
            renderPanelGrid(*snap);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Trends")) {
            trendsScreen_.render();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::Render();
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void AppWindow::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        renderFrame();
    }
}
