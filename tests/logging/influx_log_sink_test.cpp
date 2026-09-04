#include <doctest/doctest.h>
#include "logging/influx_log_sink.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <chrono>

TEST_CASE("buildInfluxLineProtocol formats bool, int, and float fields correctly") {
    auto ts = std::chrono::system_clock::time_point(std::chrono::milliseconds(1700000000123));
    std::vector<LogValue> values = {
        {"tag1", int32_t{42}, false},
        {"tag2", true, true},
        {"tag3", 3.5f, false},
    };

    std::string line = buildInfluxLineProtocol("plc_readings", ts, values);

    CHECK(line.find("plc_readings,tag_id=tag1 value_int=42i,stale=false 1700000000123\n") != std::string::npos);
    CHECK(line.find("plc_readings,tag_id=tag2 value_bool=true,stale=true 1700000000123\n") != std::string::npos);
    CHECK(line.find("plc_readings,tag_id=tag3 value_float=3.5,stale=false 1700000000123\n") != std::string::npos);
}

TEST_CASE("buildInfluxLineProtocol never uses the same field name for different PlcValue types") {
    // InfluxDB enforces one type per field name per measurement (not per
    // series) — a bool tag and an int tag sharing a field name in the same
    // measurement causes one to be silently dropped on write. Each type
    // must get a distinct field key so mixed-type measurements never
    // collide, no matter how many tags of each type are present.
    auto ts = std::chrono::system_clock::time_point(std::chrono::milliseconds(1000));
    std::vector<LogValue> values = {
        {"a", true, false},
        {"b", false, false},
        {"c", int32_t{7}, false},
        {"d", 1.5f, false},
    };

    std::string line = buildInfluxLineProtocol("m", ts, values);

    CHECK(line.find("value_bool=") != std::string::npos);
    CHECK(line.find("value_int=") != std::string::npos);
    CHECK(line.find("value_float=") != std::string::npos);
    CHECK(line.find(" value=") == std::string::npos); // the old, colliding field name must be gone
}

TEST_CASE("buildInfluxLineProtocol emits device as a real tag when set, and omits it when empty") {
    // Two devices sharing a signal label (e.g. two cameras both having a
    // "Finished" tag_id) must not collide into the same series — the
    // device tag is what keeps them apart.
    auto ts = std::chrono::system_clock::time_point(std::chrono::milliseconds(1000));
    std::vector<LogValue> withDevice = {{"Finished", true, false, "Camera 1"}};
    std::vector<LogValue> withoutDevice = {{"Finished", true, false}};

    std::string lineWith = buildInfluxLineProtocol("m", ts, withDevice);
    std::string lineWithout = buildInfluxLineProtocol("m", ts, withoutDevice);

    CHECK(lineWith.find("m,tag_id=Finished,device=Camera\\ 1 value_bool=true,stale=false 1000\n") != std::string::npos);
    CHECK(lineWithout.find("m,tag_id=Finished value_bool=true,stale=false 1000\n") != std::string::npos);
    CHECK(lineWithout.find("device=") == std::string::npos);
}

TEST_CASE("buildInfluxLineProtocol escapes spaces, commas, and equals signs in tag_id") {
    auto ts = std::chrono::system_clock::time_point(std::chrono::milliseconds(1000));
    std::vector<LogValue> values = {{"a b,c=d", int32_t{1}, false}};

    std::string line = buildInfluxLineProtocol("m", ts, values);

    CHECK(line.find("tag_id=a\\ b\\,c\\=d ") != std::string::npos);
}

TEST_CASE("buildInfluxLineProtocol produces one line per value, newline-separated") {
    auto ts = std::chrono::system_clock::time_point(std::chrono::milliseconds(1000));
    std::vector<LogValue> values = {
        {"a", int32_t{1}, false},
        {"b", int32_t{2}, false},
    };

    std::string line = buildInfluxLineProtocol("m", ts, values);
    size_t newlineCount = std::count(line.begin(), line.end(), '\n');
    CHECK(newlineCount == 2);
}

namespace {
bool canReachInflux(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::string healthUrl = url + "/health";
    curl_easy_setopt(curl, CURLOPT_URL, healthUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char*, size_t size, size_t nmemb, void*) { return size * nmemb; });
    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK;
}
} // namespace

TEST_CASE("InfluxLogSink reaches a local InfluxDB and gets a proper auth rejection for a bad token") {
    const std::string url = "http://localhost:8086";
    if (!canReachInflux(url)) {
        MESSAGE("SKIPPING: no InfluxDB reachable at ", url);
        return;
    }

    // A bogus token proves the request shape (URL, headers, line-protocol
    // body) is well-formed and reaches the server's auth check, without
    // needing real credentials this test suite shouldn't have anyway.
    InfluxLogSink sink(url, "bogus-token-for-shape-verification", "bogus-org", "bogus-bucket", "plc_readings");
    sink.open();
    CHECK_THROWS_AS(
        sink.writeRow(std::chrono::system_clock::now(), {{"tag1", int32_t{1}, false}}),
        std::runtime_error);
    sink.close();
}
