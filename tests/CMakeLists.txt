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
)

add_executable(plc_gui_tests ${PLC_GUI_TEST_SOURCES})
target_link_libraries(plc_gui_tests PRIVATE
    doctest::doctest
    yaml-cpp::yaml-cpp
    snap7_static
    SQLite::SQLite3
    Threads::Threads
)
target_include_directories(plc_gui_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME plc_gui_tests COMMAND plc_gui_tests)
