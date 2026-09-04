#include "ui/app_window.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <stdexcept>

namespace {
void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}
} // namespace

AppWindow::AppWindow(std::string plcName, std::vector<Panel> panels, ValueCache& cache, PlcIoThread& io)
    : plcName_(std::move(plcName)), panels_(std::move(panels)), cache_(cache), io_(io) {
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
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

AppWindow::~AppWindow() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
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

void AppWindow::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto snap = cache_.snapshot();

    // One window for the whole PLC connection: status at the top, then
    // every device panel nested inside as a collapsible section, each
    // panel's own groups (e.g. "Inputs"/"Outputs") nested one level
    // deeper still. PushID/PopID per panel and per group guards against
    // two devices sharing a group title (e.g. two cameras both having an
    // "Inputs" section) — without it, ImGui would generate the same
    // widget id for both and conflate their open/closed state.
    ImGui::Begin(plcName_.c_str());
    ImGui::Text("Connection: %s", connectionStateLabel(io_.connectionState()));
    ImGui::Separator();

    for (size_t pi = 0; pi < panels_.size(); ++pi) {
        auto& panel = panels_[pi];
        ImGui::PushID(static_cast<int>(pi));
        if (ImGui::CollapsingHeader(panel.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            for (auto& widget : panel.widgets) {
                widget->render(*snap, io_);
            }
            for (size_t gi = 0; gi < panel.groups.size(); ++gi) {
                auto& group = panel.groups[gi];
                ImGui::PushID(static_cast<int>(gi));
                if (ImGui::CollapsingHeader(group.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();
                    for (auto& widget : group.widgets) {
                        widget->render(*snap, io_);
                    }
                    ImGui::Unindent();
                }
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::End();

    ImGui::Render();
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
