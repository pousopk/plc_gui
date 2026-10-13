#pragma once
#include <chrono>
#include <functional>
#include <thread>

// Polls `predicate` every 10 ms until it returns true or `timeoutMs` passes.
// Returns the predicate's final result. Used by tests that wait on a
// background thread (PlcIoThread, LogManager) instead of sleeping blindly.
inline bool waitUntil(const std::function<bool()>& predicate, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}
