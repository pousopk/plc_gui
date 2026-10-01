#include <doctest/doctest.h>
#include "logging/sqlite_log_sink.hpp"
#include <sqlite3.h>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace {
std::string tempDbPath() {
    return (std::filesystem::temp_directory_path() /
            ("plc_gui_test_" + std::to_string(std::rand()) + ".db")).string();
}

struct Row { std::string ts, tagId, value; int stale; };

std::vector<Row> readAllRows(const std::string& path, const std::string& table) {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT timestamp, tag_id, value, stale FROM " + table + " ORDER BY rowid";
    REQUIRE(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK);

    std::vector<Row> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows.push_back({
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
            sqlite3_column_int(stmt, 3)
        });
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return rows;
}

std::vector<std::string> readDeviceColumn(const std::string& path, const std::string& table) {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT device FROM " + table + " ORDER BY rowid";
    REQUIRE(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK);

    std::vector<std::string> devices;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        devices.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return devices;
}
} // namespace

TEST_CASE("SqliteLogSink creates its table and persists rows visible after close") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{42}, false}, {"tag2", true, true}});
        sink.close();
    }

    auto rows = readAllRows(path, "plc_readings");
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].tagId == "tag1");
    CHECK(rows[0].value == "42");
    CHECK(rows[0].stale == 0);
    CHECK(rows[1].tagId == "tag2");
    CHECK(rows[1].stale == 1);

    std::filesystem::remove(path);
}

TEST_CASE("SqliteLogSink appends across separate sink instances on the same file") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{1}, false}});
        sink.close();
    }
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{2}, false}});
        sink.close();
    }

    auto rows = readAllRows(path, "plc_readings");
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].value == "1");
    CHECK(rows[1].value == "2");

    std::filesystem::remove(path);
}

TEST_CASE("SqliteLogSink stores each row's device") {
    auto path = tempDbPath();
    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{},
                       {{"finished", true, false, "Camera 1"}, {"finished", false, false, "Camera 2"}});
        sink.close();
    }

    auto devices = readDeviceColumn(path, "plc_readings");
    REQUIRE(devices.size() == 2);
    CHECK(devices[0] == "Camera 1");
    CHECK(devices[1] == "Camera 2");

    std::filesystem::remove(path);
}

TEST_CASE("SqliteLogSink migrates an existing table created before the device column existed") {
    auto path = tempDbPath();
    {
        // Simulate a pre-migration database: the old 4-column schema, no device column.
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
        char* err = nullptr;
        REQUIRE(sqlite3_exec(db,
            "CREATE TABLE plc_readings (timestamp TEXT NOT NULL, tag_id TEXT NOT NULL, "
            "value TEXT NOT NULL, stale INTEGER NOT NULL)",
            nullptr, nullptr, &err) == SQLITE_OK);
        sqlite3_close(db);
    }

    {
        SqliteLogSink sink(path, "plc_readings");
        sink.open(); // must migrate in (add the device column) without throwing
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{1}, false, "Camera 1"}});
        sink.close();
    }
    {
        // Reopening an already-migrated table must not fail either (idempotent).
        SqliteLogSink sink(path, "plc_readings");
        sink.open();
        sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", int32_t{2}, false, "Camera 1"}});
        sink.close();
    }

    auto devices = readDeviceColumn(path, "plc_readings");
    REQUIRE(devices.size() == 2);
    CHECK(devices[0] == "Camera 1");
    CHECK(devices[1] == "Camera 1");

    std::filesystem::remove(path);
}

TEST_CASE("SqliteLogSink flushes below the row threshold once closed") {
    auto path = tempDbPath();
    SqliteLogSink sink(path, "plc_readings", /*flushEveryNRows=*/50);
    sink.open();
    sink.writeRow(std::chrono::system_clock::time_point{}, {{"tag1", 3.5f, false}});
    sink.close();

    auto rows = readAllRows(path, "plc_readings");
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].value == "3.5");

    std::filesystem::remove(path);
}
