#pragma once
#include "logging/log_sink.hpp"
#include <sqlite3.h>
#include <string>

class SqliteLogSink : public ILogSink {
public:
    SqliteLogSink(std::string path, std::string table, int flushEveryNRows = 50);
    ~SqliteLogSink() override;

    void open() override;
    void writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) override;
    void close() override;

private:
    void flush();
    void execOrThrow(const std::string& sql);

    std::string path_;
    std::string table_;
    int flushEveryNRows_;
    sqlite3* db_ = nullptr;
    sqlite3_stmt* insertStmt_ = nullptr;
    int pendingRows_ = 0;
};
