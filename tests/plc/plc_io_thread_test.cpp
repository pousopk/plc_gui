#include <doctest/doctest.h>
#include "plc/plc_io_thread.hpp"
#include "plc/trend_buffer.hpp"
#include "support/test_plc_server.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace {

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

TagConfig makeTag(std::string id, int db, PlcType type, int offset, int intervalMs, int bit = 0) {
    TagConfig t;
    t.id = std::move(id);
    t.db = db;
    t.type = type;
    t.offset = offset;
    t.bit = bit;
    t.updateIntervalMs = intervalMs;
    return t;
}

PlcConnectionConfig fastReconnectConfig() {
    PlcConnectionConfig cfg;
    cfg.ip = "127.0.0.1";
    cfg.rack = 0;
    cfg.slot = 1;
    cfg.reconnectBackoffMs = {50, 50};
    return cfg;
}

bool startServerOrSkip(std::unique_ptr<TestPlcServer>& out, int dbNumber, int dbSize) {
    try {
        out = std::make_unique<TestPlcServer>(dbNumber, dbSize);
        return true;
    } catch (const TestPlcServerUnavailable& e) {
        MESSAGE("SKIPPING: ", std::string(e.what()));
        return false;
    }
}

} // namespace

TEST_CASE("PlcIoThread reads server values into the cache shortly after starting") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;
    encodePlcValue(PlcType::Real, 3.5f, server->db(), 0, 0);

    ValueCache cache;
    std::vector<TagConfig> tags = {makeTag("speed", 10, PlcType::Real, 0, 20)};
    PlcIoThread io(fastReconnectConfig(), tags, cache);
    io.start();

    bool ok = waitUntil([&] {
        auto snap = cache.snapshot();
        return snap->count("speed") == 1 && !snap->at("speed").stale;
    }, 1000);
    REQUIRE(ok);

    auto snap = cache.snapshot();
    CHECK(std::get<float>(snap->at("speed").value) == doctest::Approx(3.5f));
    CHECK(io.connectionState() == ConnectionState::Connected);

    io.stop();
}

TEST_CASE("PlcIoThread requestWrite is applied to the PLC") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;

    ValueCache cache;
    std::vector<TagConfig> tags = {makeTag("speed", 10, PlcType::Real, 0, 20)};
    PlcIoThread io(fastReconnectConfig(), tags, cache);
    io.start();

    waitUntil([&] { return io.connectionState() == ConnectionState::Connected; }, 1000);
    io.requestWrite("speed", 99.5f);

    bool ok = waitUntil([&] {
        return std::get<float>(decodePlcValue(PlcType::Real, server->db(), 0, 0)) == doctest::Approx(99.5f);
    }, 500);
    CHECK(ok);

    io.stop();
}

TEST_CASE("PlcIoThread writing a bool leaves the other bits packed into its byte untouched") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;
    // Byte 0 holds three bools: bit 0 and bit 2 already set by the PLC.
    server->db()[0] = 0b00000101;

    ValueCache cache;
    std::vector<TagConfig> tags = {
        makeTag("in", 10, PlcType::Bool, 0, 20, 0),
        makeTag("reset", 10, PlcType::Bool, 0, 20, 1),
        makeTag("trigger", 10, PlcType::Bool, 0, 20, 2),
    };
    PlcIoThread io(fastReconnectConfig(), tags, cache);
    io.start();
    REQUIRE(waitUntil([&] { return io.connectionState() == ConnectionState::Connected; }, 1000));

    io.requestWrite("reset", true);
    REQUIRE(waitUntil([&] { return (server->db()[0] & 0b010) != 0; }, 500));
    CHECK(server->db()[0] == 0b00000111);

    io.requestWrite("in", false);
    REQUIRE(waitUntil([&] { return (server->db()[0] & 0b001) == 0; }, 500));
    CHECK(server->db()[0] == 0b00000110);

    io.stop();
}

TEST_CASE("PlcIoThread marks values stale on disconnect and clears staleness on reconnect") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;
    encodePlcValue(PlcType::Real, 1.0f, server->db(), 0, 0);

    ValueCache cache;
    std::vector<TagConfig> tags = {makeTag("speed", 10, PlcType::Real, 0, 20)};
    PlcIoThread io(fastReconnectConfig(), tags, cache);
    io.start();

    REQUIRE(waitUntil([&] {
        auto snap = cache.snapshot();
        return snap->count("speed") == 1 && !snap->at("speed").stale;
    }, 1000));

    server.reset(); // simulate the PLC going away

    REQUIRE(waitUntil([&] {
        return io.connectionState() == ConnectionState::Reconnecting &&
               cache.snapshot()->at("speed").stale;
    }, 1000));

    if (!startServerOrSkip(server, 10, 64)) {
        io.stop();
        return;
    }
    encodePlcValue(PlcType::Real, 2.0f, server->db(), 0, 0);

    REQUIRE(waitUntil([&] {
        return io.connectionState() == ConnectionState::Connected &&
               !cache.snapshot()->at("speed").stale;
    }, 1000));

    CHECK(std::get<float>(cache.snapshot()->at("speed").value) == doctest::Approx(2.0f));

    io.stop();
}

TEST_CASE("PlcIoThread appends decoded values to the trend buffer when one is provided") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;
    encodePlcValue(PlcType::Real, 3.5f, server->db(), 0, 0);

    ValueCache cache;
    TrendBuffer trendBuffer;
    std::vector<TagConfig> tags = {makeTag("speed", 10, PlcType::Real, 0, 20)};
    PlcIoThread io(fastReconnectConfig(), tags, cache, &trendBuffer);
    io.start();

    bool ok = waitUntil([&] { return !trendBuffer.snapshot("speed").empty(); }, 1000);
    REQUIRE(ok);

    auto samples = trendBuffer.snapshot("speed");
    CHECK(std::get<float>(samples.back().value) == doctest::Approx(3.5f));

    io.stop();
}

TEST_CASE("PlcIoThread works with no trend buffer at all (default nullptr)") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;

    ValueCache cache;
    std::vector<TagConfig> tags = {makeTag("speed", 10, PlcType::Real, 0, 20)};
    PlcIoThread io(fastReconnectConfig(), tags, cache); // unchanged 3-arg call site
    io.start();

    REQUIRE(waitUntil([&] { return cache.snapshot()->count("speed") == 1; }, 1000));
    io.stop();
}
