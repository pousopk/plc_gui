#include "logging/influx_log_sink.hpp"
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {

std::string escapeTag(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == ' ' || c == ',' || c == '=') out += '\\';
        out += c;
    }
    return out;
}

size_t captureWriteCallback(char* data, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(data, size * nmemb);
    return size * nmemb;
}

} // namespace

std::string buildInfluxLineProtocol(const std::string& measurement,
                                     std::chrono::system_clock::time_point ts,
                                     const std::vector<LogValue>& values) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
    std::ostringstream out;
    for (const auto& v : values) {
        out << measurement << ",tag_id=" << escapeTag(v.tagId) << " value=";
        std::visit([&out](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, bool>) {
                out << (val ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int32_t>) {
                out << val << "i";
            } else {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%g", val);
                out << buf;
            }
        }, v.value);
        out << ",stale=" << (v.stale ? "true" : "false");
        out << " " << ms << "\n";
    }
    return out.str();
}

InfluxLogSink::InfluxLogSink(std::string url, std::string token, std::string org,
                              std::string bucket, std::string measurement)
    : url_(std::move(url)), token_(std::move(token)), org_(std::move(org)),
      bucket_(std::move(bucket)), measurement_(std::move(measurement)) {}

InfluxLogSink::~InfluxLogSink() { close(); }

void InfluxLogSink::open() {
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("InfluxLogSink: curl_easy_init failed");
}

void InfluxLogSink::writeRow(std::chrono::system_clock::time_point ts, const std::vector<LogValue>& values) {
    std::string body = buildInfluxLineProtocol(measurement_, ts, values);
    std::string writeUrl = url_ + "/api/v2/write?org=" + org_ + "&bucket=" + bucket_ + "&precision=ms";
    std::string authHeader = "Authorization: Token " + token_;

    std::string responseBody;

    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, writeUrl.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, 3000L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, captureWriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseBody);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    CURLcode rc = curl_easy_perform(curl_);
    long httpCode = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("InfluxLogSink: curl error: ") + curl_easy_strerror(rc));
    }
    if (httpCode < 200 || httpCode >= 300) {
        throw std::runtime_error("InfluxLogSink: HTTP " + std::to_string(httpCode) +
                                  " from InfluxDB write endpoint: " + responseBody);
    }
}

void InfluxLogSink::close() {
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
}
