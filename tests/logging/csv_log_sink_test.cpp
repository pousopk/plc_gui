#include <doctest/doctest.h>
#include "logging/csv_log_sink.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
std::string tempCsvPath() {
    return (std::filesystem::temp_directory_path() /
            ("plc_gui_test_" + std::to_string(std::rand()) + ".csv")).string();
}
std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}
} // namespace

TEST_CASE("CsvLogSink writes a header once and appends rows in long format") {
    auto path = tempCsvPath();
    {
        CsvLogSink sink(path);
        sink.open();
        auto ts = std::chrono::system_clock::time_point{}; // epoch, deterministic
        sink.writeRow(ts, {{"tag1", int32_t{42}, false}, {"tag2", true, true}});
        sink.close();
    }

    std::string content = readFile(path);
    CHECK(content.find("timestamp,tag_id,value,stale,device\n") == 0);
    CHECK(content.find("tag1,42,false,") != std::string::npos);
    CHECK(content.find("tag2,true,true,") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("CsvLogSink writes each row's device") {
    auto path = tempCsvPath();
    CsvLogSink sink(path);
    sink.open();
    sink.writeRow(std::chrono::system_clock::time_point{},
                   {{"finished", true, false, "Camera 1"}, {"finished", false, false, "Camera 2"}});
    sink.close();

    std::string content = readFile(path);
    CHECK(content.find("finished,true,false,Camera 1") != std::string::npos);
    CHECK(content.find("finished,false,false,Camera 2") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("CsvLogSink does not duplicate the header when reopened") {
    auto path = tempCsvPath();
    {
        CsvLogSink sink(path);
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{1}, false}});
        sink.close();
    }
    {
        CsvLogSink sink(path);
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{2}, false}});
        sink.close();
    }

    std::string content = readFile(path);
    size_t firstHeader = content.find("timestamp,tag_id,value,stale");
    size_t secondHeader = content.find("timestamp,tag_id,value,stale", firstHeader + 1);
    CHECK(firstHeader != std::string::npos);
    CHECK(secondHeader == std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("CsvLogSink flushes on close even below the row threshold") {
    auto path = tempCsvPath();
    CsvLogSink sink(path, /*flushEveryNRows=*/50);
    sink.open();
    sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", 3.5f, false}});
    sink.close();

    std::string content = readFile(path);
    CHECK(content.find("tag1,3.5,false") != std::string::npos);

    std::filesystem::remove(path);
}
