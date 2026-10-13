#pragma once
#include "logging/trend_history_query.hpp"
#include "plc/plc_io_thread.hpp"
#include "plc/trend_buffer.hpp"
#include "plc/value_cache.hpp"
#include "ui/panel_factory.hpp"
#include "ui/trends_screen.hpp"
#include <imgui.h>
#include <string>
#include <vector>

struct GLFWwindow;

// Owns the GLFW window and ImGui/OpenGL3 backend. run() blocks, rendering
// one frame per loop iteration, until the window is closed.
//
// Renders one PLC-level window (titled plcName, connection status at the
// top) with two tabs:
//   - Devices: every device panel as a card in a responsive grid, each
//     card showing its groups (e.g. "Inputs"/"Outputs") as sub-sections.
//   - Trends:  the TrendsScreen (live and historical charts).
class AppWindow {
public:
    AppWindow(std::string plcName, std::vector<Panel> panels, ValueCache& cache, PlcIoThread& io,
              std::vector<TagConfig> tags, TrendBuffer& trendBuffer, TrendHistoryQuery* historyQuery);
    ~AppWindow();
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    void run();

private:
    void renderFrame();
    void renderStatusHeader();
    void renderPanelGrid(const ValueMap& snapshot);
    void renderCard(Panel& panel, const ValueMap& snapshot, float cardWidth);
    static const char* connectionStateLabel(ConnectionState state);
    static ImVec4 connectionStateColor(ConnectionState state);
    static void applyTheme();

    std::string plcName_;
    std::vector<Panel> panels_;
    ValueCache& cache_;
    PlcIoThread& io_;
    TrendsScreen trendsScreen_;
    GLFWwindow* window_ = nullptr;
};
