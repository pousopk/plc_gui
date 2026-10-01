FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

set(PLC_GUI_TEST_SOURCES
    test_main.cpp
    plc/plc_value_test.cpp
    ${CMAKE_SOURCE_DIR}/src/plc/plc_value.cpp
    config/config_loader_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config_loader.cpp
    plc/plc_client_test.cpp
    support/test_plc_server.cpp
    ${CMAKE_SOURCE_DIR}/src/plc/plc_client.cpp
    plc/poll_group_test.cpp
    ${CMAKE_SOURCE_DIR}/src/plc/poll_group.cpp
    plc/value_cache_test.cpp
    plc/trend_buffer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/plc/trend_buffer.cpp
    plc/plc_io_thread_test.cpp
    ${CMAKE_SOURCE_DIR}/src/plc/plc_io_thread.cpp
    logging/csv_log_sink_test.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/csv_log_sink.cpp
    logging/sqlite_log_sink_test.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/sqlite_log_sink.cpp
    logging/influx_log_sink_test.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/influx_log_sink.cpp
    logging/log_manager_test.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/log_manager.cpp
    logging/trend_history_query_test.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/trend_history_query.cpp
    ui/panel_factory_test.cpp
    ui/implot_smoke_test.cpp
    ui/trends_screen_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/trends_screen.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/slider_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/input_number_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/toggle_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/button_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/text_display_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/led_indicator_widget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/panel_factory.cpp
)

add_executable(plc_gui_tests ${PLC_GUI_TEST_SOURCES})
target_link_libraries(plc_gui_tests PRIVATE
    doctest::doctest
    yaml-cpp::yaml-cpp
    imgui
    implot
    snap7_static
    SQLite::SQLite3
    CURL::libcurl
    Threads::Threads
)
target_include_directories(plc_gui_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME plc_gui_tests COMMAND plc_gui_tests)
