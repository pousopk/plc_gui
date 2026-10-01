#include <doctest/doctest.h>
#include "logging/trend_history_query.hpp"
#include "logging/sqlite_log_sink.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>

namespace {
std::string tempDbPath() {
    return (std::filesystem::temp_directory_path() /
            ("plc_gui_test_" + std::to_string(std::rand()) + ".db")).string();
}

std::chrono::system_clock::time_point secondsSinceEpoch(long s) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{s}};
}
} // namespace

TEST_CASE("TrendHistoryQuery returns samples within range, parsed by type, ordered by time") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(secondsSinceEpoch(100), {{"speed", 3.5f, false}});
        sink.writeRow(secondsSinceEpoch(200), {{"speed", 4.5f, false}});
        sink.writeRow(secondsSinceEpoch(300), {{"speed", 5.5f, false}}); // outside queried range
        sink.writeRow(secondsSinceEpoch(150), {{"running", true, false}});
        sink.close();
    }

    TrendHistoryQuery query(path, "plc_readings");
    auto samples = query.query("speed", "", PlcType::Real, secondsSinceEpoch(0), secondsSinceEpoch(250));

    REQUIRE(samples.size() == 2);
    CHECK(samples[0].timestamp == secondsSinceEpoch(100));
    CHECK(std::get<float>(samples[0].value) == doctest::Approx(3.5f));
    CHECK(samples[1].timestamp == secondsSinceEpoch(200));
    CHECK(std::get<float>(samples[1].value) == doctest::Approx(4.5f));

    auto boolSamples = query.query("running", "", PlcType::Bool, secondsSinceEpoch(0), secondsSinceEpoch(250));
    REQUIRE(boolSamples.size() == 1);
    CHECK(std::get<bool>(boolSamples[0].value) == true);

    std::filesystem::remove(path);
}

TEST_CASE("TrendHistoryQuery returns empty for a tag with no rows in range") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(secondsSinceEpoch(100), {{"speed", 1.0f, false}});
        sink.close();
    }

    TrendHistoryQuery query(path, "plc_readings");
    auto samples = query.query("speed", "", PlcType::Real, secondsSinceEpoch(500), secondsSinceEpoch(600));
    CHECK(samples.empty());

    std::filesystem::remove(path);
}

TEST_CASE("TrendHistoryQuery returns empty (not a crash) for an unreadable database") {
    TrendHistoryQuery query("/nonexistent/path/does_not_exist.db", "plc_readings");
    auto samples = query.query("speed", "", PlcType::Real, secondsSinceEpoch(0), secondsSinceEpoch(1));
    CHECK(samples.empty());
}

TEST_CASE("TrendHistoryQuery isolates rows by device when two devices share a tag_id") {
    // Regression test for the bug where two devices sharing a device_type
    // blueprint (e.g. two cameras both having a "Finished" field) write
    // identical tag_id values, and a query with no device filter would
    // silently merge both devices' rows into one series.
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(secondsSinceEpoch(100), {{"Finished", true, false, "Camera 1"}});
        sink.writeRow(secondsSinceEpoch(100), {{"Finished", false, false, "Camera 2"}});
        sink.writeRow(secondsSinceEpoch(200), {{"Finished", true, false, "Camera 1"}});
        sink.writeRow(secondsSinceEpoch(200), {{"Finished", false, false, "Camera 2"}});
        sink.close();
    }

    TrendHistoryQuery query(path, "plc_readings");

    auto cam1Samples = query.query("Finished", "Camera 1", PlcType::Bool, secondsSinceEpoch(0), secondsSinceEpoch(300));
    REQUIRE(cam1Samples.size() == 2);
    CHECK(std::get<bool>(cam1Samples[0].value) == true);
    CHECK(std::get<bool>(cam1Samples[1].value) == true);

    auto cam2Samples = query.query("Finished", "Camera 2", PlcType::Bool, secondsSinceEpoch(0), secondsSinceEpoch(300));
    REQUIRE(cam2Samples.size() == 2);
    CHECK(std::get<bool>(cam2Samples[0].value) == false);
    CHECK(std::get<bool>(cam2Samples[1].value) == false);

    std::filesystem::remove(path);
}
