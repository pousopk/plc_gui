#pragma once
#include "logging/log_sink.hpp"
#include <fstream>
#include <string>

class CsvLogSink : public ILogSink {
public:
    explicit CsvLogSink(std::string path, int flushEveryNRows = 50);
    ~CsvLogSink() override;

    void open() override;
    void writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) override;
    void close() override;

private:
    void flush();

    std::string path_;
    int flushEveryNRows_;
    std::ofstream file_;
    int bufferedRows_ = 0;
};
