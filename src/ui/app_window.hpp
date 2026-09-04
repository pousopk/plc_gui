#pragma once
#include "plc/plc_io_thread.hpp"
#include "plc/value_cache.hpp"
#include "ui/panel_factory.hpp"
#include <string>
#include <vector>

struct GLFWwindow;

// Owns the GLFW window and ImGui/OpenGL3 backend. run() blocks, rendering
// one frame per loop iteration, until the window is closed.
//
// Renders one PLC-level window (titled plcName, showing connection status
// at the top) containing every device panel as a nested collapsible
// section, each panel's own groups (e.g. "Inputs"/"Outputs") nested one
// level deeper still — matching the single-PLC-connection architecture,
// where "which PLC" is the outermost, non-repeating level of the UI.
class AppWindow {
public:
    AppWindow(std::string plcName, std::vector<Panel> panels, ValueCache& cache, PlcIoThread& io);
    ~AppWindow();
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    void run();

private:
    void renderFrame();
    static const char* connectionStateLabel(ConnectionState state);

    std::string plcName_;
    std::vector<Panel> panels_;
    ValueCache& cache_;
    PlcIoThread& io_;
    GLFWwindow* window_ = nullptr;
};
