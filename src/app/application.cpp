#include "app/application.hpp"
#include "logging/csv_log_sink.hpp"
#include "logging/influx_log_sink.hpp"
#include "logging/log_manager.hpp"
#include "logging/sqlite_log_sink.hpp"
#include "logging/trend_history_query.hpp"
#include "ui/app_window.hpp"
#include "ui/panel_factory.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

namespace {

// Set by the SIGINT/SIGTERM handler. A signal handler may only touch
// lock-free atomics, so it does nothing else; the main thread polls it.
std::atomic<bool> g_stopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free, "signal handler needs a lock-free flag");

constexpr int kStopPollIntervalMs = 100;

void handleStopSignal(int) {
    g_stopRequested = true;
}

std::unordered_map<std::string, std::shared_ptr<ILogSink>> buildSinks(const LoggingConfig& logging) {
    std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinks;
    for (const auto& sink : logging.sinks) {
        switch (sink.type) {
            case LogSinkType::Csv:
                sinks[sink.id] = std::make_shared<CsvLogSink>(sink.path);
                break;
            case LogSinkType::Sqlite:
                sinks[sink.id] = std::make_shared<SqliteLogSink>(sink.path, sink.table);
                break;
            case LogSinkType::InfluxDb:
                sinks[sink.id] = std::make_shared<InfluxLogSink>(sink.url, sink.token, sink.org,
                                                                 sink.bucket, sink.measurement);
                break;
        }
    }
    return sinks;
}

// The Trends screen's historical view reads from the first SQLite sink.
// Returns nullptr when no SQLite sink is configured (historical view disabled).
std::unique_ptr<TrendHistoryQuery> createHistoryQuery(const LoggingConfig& logging) {
    for (const auto& sink : logging.sinks) {
        if (sink.type == LogSinkType::Sqlite) {
            return std::make_unique<TrendHistoryQuery>(sink.path, sink.table);
        }
    }
    return nullptr;
}

} // namespace

Application::Application(AppConfig config, bool headless)
    : config_(std::move(config)), headless_(headless) {}

int Application::run() {
    ValueCache cache;
    TrendBuffer trendBuffer;
    PlcIoThread io(toProfinetConnection(config_.plc), toProfinetTags(config_.tags), cache,
                   [&trendBuffer](const std::string& tagId, std::chrono::system_clock::time_point timestamp,
                                  const PlcValue& value) { trendBuffer.append(tagId, timestamp, value); });
    LogManager logManager(config_.logging.rules, buildSinks(config_.logging), cache, config_.tags);

    io.start();
    logManager.start();

    if (headless_) {
        waitForStopSignal();
    } else {
        runGui(cache, io, trendBuffer);
    }

    logManager.stop();
    io.stop();
    return 0;
}

void Application::waitForStopSignal() {
    std::signal(SIGINT, handleStopSignal);
    std::signal(SIGTERM, handleStopSignal);

    std::cout << "plc_gui: running headless. Ctrl+C to stop.\n";
    while (!g_stopRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kStopPollIntervalMs));
    }
}

void Application::runGui(ValueCache& cache, PlcIoThread& io, TrendBuffer& trendBuffer) {
    auto panels = PanelFactory::build(config_.panels, config_.tags);
    std::unique_ptr<TrendHistoryQuery> historyQuery = createHistoryQuery(config_.logging);

    AppWindow window(config_.plc.name, std::move(panels), cache, io,
                     config_.tags, trendBuffer, historyQuery.get());
    window.run(); // blocks until the window is closed
}
