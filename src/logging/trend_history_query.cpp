#include "logging/trend_history_query.hpp"
#include "logging/log_format.hpp"
#include <sqlite3.h>
#include <iostream>

TrendHistoryQuery::TrendHistoryQuery(std::string dbPath, std::string table)
    : dbPath_(std::move(dbPath)), table_(std::move(table)) {}

std::vector<TrendSample> TrendHistoryQuery::query(const std::string& tagLabel, const std::string& device, PlcType type,
                                                    std::chrono::system_clock::time_point from,
                                                    std::chrono::system_clock::time_point to) const {
    std::vector<TrendSample> result;

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath_.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::cerr << "TrendHistoryQuery: failed to open " << dbPath_ << ": "
                  << (db ? sqlite3_errmsg(db) : "unknown error") << "\n";
        if (db) sqlite3_close(db);
        return result;
    }

    std::string sql = "SELECT timestamp, value FROM " + table_ +
                       " WHERE tag_id = ? AND device = ? AND timestamp BETWEEN ? AND ? ORDER BY timestamp";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "TrendHistoryQuery: failed to prepare query: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return result;
    }

    std::string fromStr = formatTimestamp(from);
    std::string toStr = formatTimestamp(to);
    sqlite3_bind_text(stmt, 1, tagLabel.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, device.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fromStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, toStr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string tsText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string valueText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        try {
            result.push_back(TrendSample{parseTimestamp(tsText), parsePlcValue(type, valueText)});
        } catch (const std::exception&) {
            continue; // malformed value text; skip this row rather than aborting the query
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}
