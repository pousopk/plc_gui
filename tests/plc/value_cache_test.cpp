#include <doctest/doctest.h>
#include "plc/value_cache.hpp"
#include <atomic>
#include <thread>

TEST_CASE("ValueCache starts with an empty snapshot") {
    ValueCache cache;
    auto snap = cache.snapshot();
    REQUIRE(snap != nullptr);
    CHECK(snap->empty());
}

TEST_CASE("ValueCache publish replaces the snapshot readers see") {
    ValueCache cache;
    auto next = std::make_shared<ValueMap>();
    (*next)["tag1"] = CachedValue{int32_t{42}, false, std::chrono::steady_clock::now()};
    cache.publish(next);

    auto snap = cache.snapshot();
    REQUIRE(snap->count("tag1") == 1);
    CHECK(std::get<int32_t>(snap->at("tag1").value) == 42);
    CHECK(snap->at("tag1").stale == false);
}

TEST_CASE("ValueCache concurrent publish/snapshot never yields a torn map") {
    ValueCache cache;
    constexpr int kIterations = 2000;
    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            auto next = std::make_shared<ValueMap>();
            for (const char* id : {"a", "b", "c"}) {
                (*next)[id] = CachedValue{int32_t{i}, false, std::chrono::steady_clock::now()};
            }
            cache.publish(next);
        }
        stop = true;
    });

    auto reader = [&] {
        while (!stop.load()) {
            auto snap = cache.snapshot();
            if (snap->size() == 3) {
                int a = std::get<int32_t>(snap->at("a").value);
                int b = std::get<int32_t>(snap->at("b").value);
                int c = std::get<int32_t>(snap->at("c").value);
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
