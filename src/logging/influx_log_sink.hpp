#pragma once
#include "logging/log_sink.hpp"
#include <curl/curl.h>
#include <string>

// Exposed for testing: builds the InfluxDB v2 line-protocol body for one
// writeRow() call — one line per value, newline-separated.
std::string buildInfluxLineProtocol(const std::string& measurement,
                                     std::chrono::system_clock::time_point ts,
                                     const std::vector<LogValue>& values);

// Writes rows to InfluxDB v2's HTTP write API. A failed write (bad token,
// unreachable server, non-2xx response) throws — LogManager already
// catches and logs sink exceptions without crashing, same as CSV/SQLite.
class InfluxLogSink : public ILogSink {
public:
    InfluxLogSink(std::string url, std::string token, std::string org,
                  std::string bucket, std::string measurement);
    ~InfluxLogSink() override;

    void open() override;
    void writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) override;
    void close() override;

private:
    std::string url_;
    std::string token_;
    std::string org_;
    std::string bucket_;
    std::string measurement_;
    CURL* curl_ = nullptr;
};
