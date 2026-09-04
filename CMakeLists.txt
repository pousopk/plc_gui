cmake_minimum_required(VERSION 3.21)
project(plc_gui CXX)

# Several vendored/fetched third-party projects (e.g. doctest 2.4.x) declare
# a cmake_minimum_required below 3.5, which CMake 4.x refuses outright since
# compatibility with pre-3.5 was removed. This is CMake's own documented
# workaround for configuring such dependencies anyway.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- yaml-cpp ---
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        yaml-cpp-0.9.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# --- Dear ImGui (no CMake support upstream: built as a plain static lib) ---
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.92.6
)
FetchContent_MakeAvailable(imgui)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- Snap7 (vendored source, hand-written build — see cmake/snap7.cmake) ---
include(cmake/snap7.cmake)

# --- SQLite3 (system package) ---
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# --- libcurl (system package) — InfluxDB v2 sink writes over HTTP ---
find_package(CURL REQUIRED)

set(PLC_GUI_SOURCES
    src/main.cpp
    src/plc/plc_value.cpp
    src/config/config_loader.cpp
    src/plc/plc_client.cpp
    src/plc/poll_group.cpp
    src/plc/plc_io_thread.cpp
    src/logging/csv_log_sink.cpp
    src/logging/sqlite_log_sink.cpp
    src/logging/influx_log_sink.cpp
    src/logging/log_manager.cpp
    src/ui/widgets/slider_widget.cpp
    src/ui/widgets/input_number_widget.cpp
    src/ui/widgets/toggle_widget.cpp
    src/ui/widgets/button_widget.cpp
    src/ui/widgets/text_display_widget.cpp
    src/ui/widgets/led_indicator_widget.cpp
    src/ui/panel_factory.cpp
    src/ui/app_window.cpp
    src/app/application.cpp
)

add_executable(plc_gui ${PLC_GUI_SOURCES})
target_link_libraries(plc_gui PRIVATE
    yaml-cpp::yaml-cpp
    imgui
    snap7_static
    SQLite::SQLite3
    CURL::libcurl
    Threads::Threads
)
target_include_directories(plc_gui PRIVATE src)

add_executable(fake_plc_server tools/fake_plc_server.cpp)
target_link_libraries(fake_plc_server PRIVATE snap7_static Threads::Threads)

enable_testing()
add_subdirectory(tests)
