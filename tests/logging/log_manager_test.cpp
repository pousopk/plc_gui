#include <doctest/doctest.h>
#include "logging/log_manager.hpp"
#include "support/wait_until.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace {

class FakeLogSink : public ILogSink {
public:
    void open() override { openCalled = true; }
    void writeRow(std::chrono::system_clock::time_point, const std::vector<LogValue>& values) override {
        std::lock_guard<std::mutex> lock(mutex);
        rows.push_back(values);
    }
    void close() override { closeCalled = true; }

    bool openCalled = false;
    bool closeCalled = false;
    std::mutex mutex;
    std::vector<std::vector<LogValue>> rows;

    size_t rowCount() { std::lock_guard<std::mutex> lock(mutex); return rows.size(); }
};

class ThrowingLogSink : public ILogSink {
public:
    void open() override {}
    void writeRow(std::chrono::system_clock::time_point, const std::vector<LogValue>&) override {
        throw std::runtime_error("simulated sink failure");
    }
    void close() override {}
};

ValueMap makeValues() {
    ValueMap values;
    values["tag1"] = CachedValue{int32_t{42}, false, std::chrono::steady_clock::now()};
    values["tag2"] = CachedValue{true, true, std::chrono::steady_clock::now()};
    return values;
}

} // namespace

TEST_CASE("LogManager opens sinks on start and closes them on stop") {
    ValueCache cache;
    cache.update(makeValues());
    auto sink = std::make_shared<FakeLogSink>();

    LogRuleConfig rule;
    rule.tags = {"tag1"};
    rule.intervalMs = 1000;
    rule.sinks = {"s1"};

    LogManager mgr({rule}, {{"s1", sink}}, cache);
    mgr.start();
    CHECK(sink->openCalled);
    mgr.stop();
    CHECK(sink->closeCalled);
}

TEST_CASE("LogManager dispatches current cache values to configured sinks on schedule") {
    ValueCache cache;
    cache.update(makeValues());
    auto sink = std::make_shared<FakeLogSink>();

    LogRuleConfig rule;
    rule.tags = {"tag1", "tag2"};
    rule.intervalMs = 20;
    rule.sinks = {"s1"};

    LogManager mgr({rule}, {{"s1", sink}}, cache);
    mgr.start();

    REQUIRE(waitUntil([&] { return sink->rowCount() >= 1; }, 500));
    mgr.stop();

    auto row = sink->rows.front();
    REQUIRE(row.size() == 2);
    CHECK(row[0].tagLabel == "tag1");
    CHECK(std::get<int32_t>(row[0].value) == 42);
    CHECK(row[0].stale == false);
    CHECK(row[1].tagLabel == "tag2");
    CHECK(row[1].stale == true);
}

TEST_CASE("LogManager dispatches under the tag's label when one is configured, falling back to id otherwise") {
    ValueCache cache;
    cache.update(makeValues());
    auto sink = std::make_shared<FakeLogSink>();

    LogRuleConfig rule;
    rule.tags = {"tag1", "tag2"};
    rule.intervalMs = 20;
    rule.sinks = {"s1"};

    TagConfig labeledTag;
    labeledTag.id = "tag1";
    labeledTag.label = "Finished";
    // tag2 is deliberately absent from the tag list.

    LogManager mgr({rule}, {{"s1", sink}}, cache, {labeledTag});
    mgr.start();

    REQUIRE(waitUntil([&] { return sink->rowCount() >= 1; }, 500));
    mgr.stop();

    auto row = sink->rows.front();
    REQUIRE(row.size() == 2);
    CHECK(row[0].tagLabel == "Finished"); // tag1 has a label -> used instead of the raw id
    CHECK(row[1].tagLabel == "tag2");     // tag2 is unknown -> falls back to its raw id
}

TEST_CASE("LogManager sets LogValue::device when configured, leaving it empty otherwise") {
    ValueCache cache;
    cache.update(makeValues());
    auto sink = std::make_shared<FakeLogSink>();

    LogRuleConfig rule;
    rule.tags = {"tag1", "tag2"};
    rule.intervalMs = 20;
    rule.sinks = {"s1"};

    TagConfig deviceTag;
    deviceTag.id = "tag1";
    deviceTag.device = "Camera 1";
    TagConfig plainTag;
    plainTag.id = "tag2";

    LogManager mgr({rule}, {{"s1", sink}}, cache, {deviceTag, plainTag});
    mgr.start();

    REQUIRE(waitUntil([&] { return sink->rowCount() >= 1; }, 500));
    mgr.stop();

    auto row = sink->rows.front();
    REQUIRE(row.size() == 2);
    CHECK(row[0].device == "Camera 1");
    CHECK(row[1].device == "");
}

TEST_CASE("LogManager keeps running and keeps logging to other sinks after one sink throws") {
    ValueCache cache;
    cache.update(makeValues());
    auto badSink = std::make_shared<ThrowingLogSink>();
    auto goodSink = std::make_shared<FakeLogSink>();

    LogRuleConfig rule;
    rule.tags = {"tag1"};
    rule.intervalMs = 20;
    rule.sinks = {"bad", "good"};

    LogManager mgr({rule}, {{"bad", badSink}, {"good", goodSink}}, cache);
    mgr.start();

    REQUIRE(waitUntil([&] { return goodSink->rowCount() >= 3; }, 1000));
    mgr.stop();
}
