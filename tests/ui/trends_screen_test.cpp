#include <doctest/doctest.h>
#include "ui/trends_screen.hpp"
#include "logging/sqlite_log_sink.hpp"
#include <cstdlib>
#include <filesystem>

namespace {
TagConfig makeTag(std::string id, PlcType type, std::string label = "", std::string device = "") {
    TagConfig t;
    t.id = std::move(id);
    t.db = 1;
    t.type = type;
    t.offset = 0;
    t.updateIntervalMs = 100;
    t.label = std::move(label);
    t.device = std::move(device);
    return t;
}

std::string tempDbPath() {
    return (std::filesystem::temp_directory_path() /
            ("plc_gui_test_" + std::to_string(std::rand()) + ".db")).string();
}
} // namespace

TEST_CASE("buildTrendTagList carries id, default label, and type per tag") {
    std::vector<TagConfig> tags = {
        makeTag("motor1_speed", PlcType::Real, "Speed (RPM)"),
        makeTag("running", PlcType::Bool),
    };
    auto list = buildTrendTagList(tags);

    REQUIRE(list.size() == 2);
    CHECK(list[0].id == "motor1_speed");
    CHECK(list[0].label == "Speed (RPM)");
    CHECK(list[0].type == PlcType::Real);
    CHECK(list[1].id == "running");
    CHECK(list[1].label == "running"); // no label set, falls back to id
    CHECK(list[1].type == PlcType::Bool);
}

TEST_CASE("buildTrendTagList prefixes displayName with device when tags across devices share a label") {
    std::vector<TagConfig> tags = {
        makeTag("cam1_finished", PlcType::Bool, "Finished", "Camera 1"),
        makeTag("cam2_finished", PlcType::Bool, "Finished", "Camera 2"),
        makeTag("no_device_tag", PlcType::Real, "Speed"), // no device -- unprefixed
    };
    auto list = buildTrendTagList(tags);

    REQUIRE(list.size() == 3);
    // label stays the raw SQLite-stored key (unprefixed) -- see
    // TrendHistoryQuery/refreshTrendHistoryCache, which query by this field.
    CHECK(list[0].label == "Finished");
    CHECK(list[1].label == "Finished");
    // displayName is UI-only and disambiguates checklist/legend entries.
    CHECK(list[0].displayName == "Camera 1 - Finished");
    CHECK(list[1].displayName == "Camera 2 - Finished");
    CHECK(list[2].displayName == "Speed"); // no device set -- unprefixed
}

TEST_CASE("groupTrendTagsByDevice buckets tags by device, preserving first-seen order") {
    std::vector<TrendTagInfo> tagList = {
        {"cam1_finished", "Finished", "Camera 1 - Finished", "Camera 1", PlcType::Bool},
        {"cam1_ready", "Ready", "Camera 1 - Ready", "Camera 1", PlcType::Bool},
        {"cam2_finished", "Finished", "Camera 2 - Finished", "Camera 2", PlcType::Bool},
        {"no_device_tag", "Speed", "Speed", "", PlcType::Real},
    };
    auto groups = groupTrendTagsByDevice(tagList);

    REQUIRE(groups.size() == 3);
    CHECK(groups[0].device == "Camera 1");
    REQUIRE(groups[0].tags.size() == 2);
    CHECK(groups[0].tags[0].id == "cam1_finished");
    CHECK(groups[0].tags[1].id == "cam1_ready");
    CHECK(groups[1].device == "Camera 2");
    REQUIRE(groups[1].tags.size() == 1);
    CHECK(groups[1].tags[0].id == "cam2_finished");
    CHECK(groups[2].device == ""); // ungrouped bucket, rendered as "Other"
    REQUIRE(groups[2].tags.size() == 1);
    CHECK(groups[2].tags[0].id == "no_device_tag");
}

TEST_CASE("trendHistoryRangeToTimestamps returns [now - lookback, now] for each preset") {
    auto now = std::chrono::system_clock::now();

    auto [fromHour, toHour] = trendHistoryRangeToTimestamps(TrendHistoryRange::LastHour, now);
    CHECK(toHour == now);
    CHECK(now - fromHour == std::chrono::hours(1));

    auto [from24h, to24h] = trendHistoryRangeToTimestamps(TrendHistoryRange::Last24Hours, now);
    CHECK(now - from24h == std::chrono::hours(24));

    auto [from7d, to7d] = trendHistoryRangeToTimestamps(TrendHistoryRange::Last7Days, now);
    CHECK(now - from7d == std::chrono::hours(24 * 7));
}

TEST_CASE("refreshTrendHistoryCache queries SQLite by each tag's display name, not its raw id") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        // LogManager writes rows keyed by label-or-id (log_manager.cpp
        // runRule) -- "Speed (RPM)" here, not the tag's raw id.
        sink.writeRow(std::chrono::system_clock::now(), {{"Speed (RPM)", 3.5f, false}});
        sink.close();
    }

    TrendHistoryQuery historyQuery(path, "plc_readings");
    std::vector<TrendTagInfo> tagList = {{"motor1_speed", "Speed (RPM)", "Speed (RPM)", "", PlcType::Real}};
    std::unordered_map<std::string, bool> checked = {{"motor1_speed", true}};

    auto cache = refreshTrendHistoryCache(historyQuery, tagList, checked,
                                           TrendHistoryRange::LastHour, std::chrono::system_clock::now());

    REQUIRE(cache.count("motor1_speed") == 1); // result keyed back by raw id
    REQUIRE(cache.at("motor1_speed").size() == 1);
    CHECK(std::get<float>(cache.at("motor1_speed")[0].value) == doctest::Approx(3.5f));

    std::filesystem::remove(path);
}

TEST_CASE("refreshTrendHistoryCache skips unchecked tags") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::now(), {{"speed", 1.0f, false}});
        sink.close();
    }

    TrendHistoryQuery historyQuery(path, "plc_readings");
    std::vector<TrendTagInfo> tagList = {{"speed", "speed", "speed", "", PlcType::Real}};
    std::unordered_map<std::string, bool> checked = {{"speed", false}};

    auto cache = refreshTrendHistoryCache(historyQuery, tagList, checked,
                                           TrendHistoryRange::LastHour, std::chrono::system_clock::now());
    CHECK(cache.empty());

    std::filesystem::remove(path);
}

TEST_CASE("refreshTrendHistoryCache queries by device, isolating same-labeled tags from different devices") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::now(), {{"Finished", true, false, "Camera 1"}});
        sink.writeRow(std::chrono::system_clock::now(), {{"Finished", false, false, "Camera 2"}});
        sink.close();
    }

    TrendHistoryQuery historyQuery(path, "plc_readings");
    std::vector<TrendTagInfo> tagList = {
        {"cam1_finished", "Finished", "Camera 1 - Finished", "Camera 1", PlcType::Bool},
        {"cam2_finished", "Finished", "Camera 2 - Finished", "Camera 2", PlcType::Bool},
    };
    std::unordered_map<std::string, bool> checked = {{"cam1_finished", true}, {"cam2_finished", true}};

    auto cache = refreshTrendHistoryCache(historyQuery, tagList, checked,
                                           TrendHistoryRange::LastHour, std::chrono::system_clock::now());

    REQUIRE(cache.at("cam1_finished").size() == 1);
    CHECK(std::get<bool>(cache.at("cam1_finished")[0].value) == true);
    REQUIRE(cache.at("cam2_finished").size() == 1);
    CHECK(std::get<bool>(cache.at("cam2_finished")[0].value) == false);

    std::filesystem::remove(path);
}
