FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

set(PLC_GUI_TEST_SOURCES
    test_main.cpp
)

add_executable(plc_gui_tests ${PLC_GUI_TEST_SOURCES})
target_link_libraries(plc_gui_tests PRIVATE
    doctest::doctest
    yaml-cpp::yaml-cpp
    snap7_static
    SQLite::SQLite3
    Threads::Threads
)
target_include_directories(plc_gui_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME plc_gui_tests COMMAND plc_gui_tests)
