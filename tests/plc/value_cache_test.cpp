#include <doctest/doctest.h>
#include "plc/value_cache.hpp"
#include <atomic>
#include <thread>

TEST_CASE("ValueCache starts with an empty snapshot") {
    ValueCache cache;
    CHECK(cache.snapshot().empty());
}

TEST_CASE("ValueCache update sets values readers see, leaving other tags untouched") {
    ValueCache cache;
    cache.update({{"tag1", CachedValue{int32_t{42}, false, std::chrono::steady_clock::now()}}});
    cache.update({{"tag2", CachedValue{true, false, std::chrono::steady_clock::now()}}});

    ValueMap snapshot = cache.snapshot();
    REQUIRE(snapshot.count("tag1") == 1);
    REQUIRE(snapshot.count("tag2") == 1);
    CHECK(std::get<int32_t>(snapshot.at("tag1").value) == 42);
    CHECK(snapshot.at("tag1").stale == false);
}

TEST_CASE("ValueCache markAllStale flags every value as stale") {
    ValueCache cache;
    cache.update({{"tag1", CachedValue{int32_t{1}, false, std::chrono::steady_clock::now()}},
                  {"tag2", CachedValue{int32_t{2}, false, std::chrono::steady_clock::now()}}});
    cache.markAllStale();

    ValueMap snapshot = cache.snapshot();
    CHECK(snapshot.at("tag1").stale);
    CHECK(snapshot.at("tag2").stale);
}

TEST_CASE("ValueCache concurrent update/snapshot never yields a torn map") {
    ValueCache cache;
    constexpr int kIterations = 2000;
    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            ValueMap values;
            for (const char* id : {"a", "b", "c"}) {
                values[id] = CachedValue{int32_t{i}, false, std::chrono::steady_clock::now()};
            }
            cache.update(values);
        }
        stop = true;
    });

    auto reader = [&] {
        while (!stop.load()) {
            ValueMap snapshot = cache.snapshot();
            if (snapshot.size() == 3) {
                int a = std::get<int32_t>(snapshot.at("a").value);
                int b = std::get<int32_t>(snapshot.at("b").value);
                int c = std::get<int32_t>(snapshot.at("c").value);
                if (a != b || b != c) failed = true;
            }
        }
    };
    std::thread r1(reader), r2(reader);
    writer.join();
    r1.join();
    r2.join();

    CHECK_FALSE(failed.load());
}
