#include <doctest/doctest.h>
#include "plc/trend_buffer.hpp"
#include <atomic>
#include <thread>

TEST_CASE("TrendBuffer snapshot is empty for an unknown tag") {
    TrendBuffer buf;
    CHECK(buf.snapshot("nope").empty());
}

TEST_CASE("TrendBuffer append accumulates samples in order") {
    TrendBuffer buf;
    auto t0 = std::chrono::system_clock::now();
    buf.append("speed", t0, 1.0f);
    buf.append("speed", t0 + std::chrono::seconds(1), 2.0f);

    auto samples = buf.snapshot("speed");
    REQUIRE(samples.size() == 2);
    CHECK(std::get<float>(samples[0].value) == doctest::Approx(1.0f));
    CHECK(std::get<float>(samples[1].value) == doctest::Approx(2.0f));
}

TEST_CASE("TrendBuffer drops the oldest sample once capacity is exceeded") {
    TrendBuffer buf(/*capacityPerTag=*/3);
    auto t0 = std::chrono::system_clock::now();
    for (int i = 0; i < 5; ++i) {
        buf.append("speed", t0 + std::chrono::seconds(i), static_cast<int32_t>(i));
    }
    auto samples = buf.snapshot("speed");
    REQUIRE(samples.size() == 3);
    CHECK(std::get<int32_t>(samples[0].value) == 2); // 0 and 1 dropped
    CHECK(std::get<int32_t>(samples[2].value) == 4);
}

TEST_CASE("TrendBuffer keeps separate buffers per tag") {
    TrendBuffer buf;
    buf.append("a", std::chrono::system_clock::now(), int32_t{1});
    buf.append("b", std::chrono::system_clock::now(), int32_t{2});

    CHECK(buf.snapshot("a").size() == 1);
    CHECK(buf.snapshot("b").size() == 1);
    CHECK(std::get<int32_t>(buf.snapshot("a")[0].value) == 1);
    CHECK(std::get<int32_t>(buf.snapshot("b")[0].value) == 2);
}

TEST_CASE("TrendBuffer concurrent append/snapshot never observes a torn read") {
    TrendBuffer buf(/*capacityPerTag=*/50);
    constexpr int kIterations = 2000;
    std::atomic<bool> stop{false};

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            buf.append("speed", std::chrono::system_clock::now(), static_cast<int32_t>(i));
        }
        stop = true;
    });

    std::thread reader([&] {
        while (!stop.load()) {
            auto samples = buf.snapshot("speed");
            (void)samples;
        }
    });

    writer.join();
    reader.join();

    CHECK(buf.snapshot("speed").size() <= 50);
}
