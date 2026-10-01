#include "logging/csv_log_sink.hpp"
#include "logging/log_format.hpp"
#include <filesystem>

CsvLogSink::CsvLogSink(std::string path, int flushEveryNRows)
    : path_(std::move(path)), flushEveryNRows_(flushEveryNRows) {}

CsvLogSink::~CsvLogSink() { close(); }

void CsvLogSink::open() {
    bool isNew = !std::filesystem::exists(path_) || std::filesystem::file_size(path_) == 0;
    auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    file_.open(path_, std::ios::app);
    if (isNew) {
        file_ << "timestamp,tag_id,value,stale,device\n";
        file_.flush();
    }
}

void CsvLogSink::writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) {
    std::string tsStr = formatTimestamp(ts);
    for (const auto& v : values) {
        file_ << tsStr << ',' << v.tagId << ',' << formatPlcValue(v.value) << ','
              << (v.stale ? "true" : "false") << ',' << v.device << '\n';
    }
    if (++bufferedRows_ >= flushEveryNRows_) flush();
}

void CsvLogSink::close() {
    flush();
    if (file_.is_open()) file_.close();
}

void CsvLogSink::flush() {
    file_.flush();
    bufferedRows_ = 0;
}
