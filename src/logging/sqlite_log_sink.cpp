#include "logging/sqlite_log_sink.hpp"
#include "logging/log_format.hpp"
#include <filesystem>
#include <stdexcept>

SqliteLogSink::SqliteLogSink(std::string path, std::string table, int flushEveryNRows)
    : path_(std::move(path)), table_(std::move(table)), flushEveryNRows_(flushEveryNRows) {}

SqliteLogSink::~SqliteLogSink() { close(); }

void SqliteLogSink::execOrThrow(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown sqlite error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SqliteLogSink: " + msg);
    }
}

void SqliteLogSink::open() {
    auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("SqliteLogSink: failed to open " + path_ + ": " + msg);
    }

    execOrThrow("CREATE TABLE IF NOT EXISTS " + table_ +
                " (timestamp TEXT NOT NULL, tag_id TEXT NOT NULL, value TEXT NOT NULL, stale INTEGER NOT NULL)");

    std::string insertSql = "INSERT INTO " + table_ + " (timestamp, tag_id, value, stale) VALUES (?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_, insertSql.c_str(), -1, &insertStmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("SqliteLogSink: failed to prepare insert: ") + sqlite3_errmsg(db_));
    }

    execOrThrow("BEGIN TRANSACTION");
}

void SqliteLogSink::writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) {
    std::string tsStr = formatTimestamp(ts);
    for (const auto& v : values) {
        sqlite3_reset(insertStmt_);
        sqlite3_bind_text(insertStmt_, 1, tsStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt_, 2, v.tagId.c_str(), -1, SQLITE_TRANSIENT);
        std::string valStr = formatPlcValue(v.value);
        sqlite3_bind_text(insertStmt_, 3, valStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insertStmt_, 4, v.stale ? 1 : 0);
        if (sqlite3_step(insertStmt_) != SQLITE_DONE) {
            throw std::runtime_error(std::string("SqliteLogSink: insert failed: ") + sqlite3_errmsg(db_));
        }
    }
    if (++pendingRows_ >= flushEveryNRows_) flush();
}

void SqliteLogSink::flush() {
    execOrThrow("COMMIT");
    execOrThrow("BEGIN TRANSACTION");
    pendingRows_ = 0;
}

void SqliteLogSink::close() {
    if (!db_) return;
    if (insertStmt_) { sqlite3_finalize(insertStmt_); insertStmt_ = nullptr; }
    execOrThrow("COMMIT");
    sqlite3_close(db_);
    db_ = nullptr;
}
