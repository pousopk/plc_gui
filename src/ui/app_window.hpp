#pragma once
#include "plc/plc_io_thread.hpp"
#include "plc/value_cache.hpp"
#include "ui/panel_factory.hpp"
#include <vector>

struct GLFWwindow;

// Owns the GLFW window and ImGui/OpenGL3 backend. run() blocks, rendering
// one frame per loop iteration, until the window is closed.
class AppWindow {
public:
    AppWindow(std::vector<Panel> panels, ValueCache& cache, PlcIoThread& io);
    ~AppWindow();
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    void run();

private:
    void renderFrame();
    static const char* connectionStateLabel(ConnectionState state);

    std::vector<Panel> panels_;
    ValueCache& cache_;
    PlcIoThread& io_;
    GLFWwindow* window_ = nullptr;
};
