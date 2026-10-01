#include "app/application.hpp"
#include "logging/csv_log_sink.hpp"
#include "logging/influx_log_sink.hpp"
#include "logging/log_manager.hpp"
#include "logging/sqlite_log_sink.hpp"
#include "logging/trend_history_query.hpp"
#include "plc/plc_io_thread.hpp"
#include "plc/trend_buffer.hpp"
#include "plc/value_cache.hpp"
#include "ui/app_window.hpp"
#include "ui/panel_factory.hpp"
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

std::atomic<bool>* g_stopFlag = nullptr;
std::condition_variable* g_stopCv = nullptr;

void handleSignal(int) {
    if (g_stopFlag) {
        g_stopFlag->store(true);
        g_stopCv->notify_all();
    }
}

std::unordered_map<std::string, std::shared_ptr<ILogSink>> buildSinks(const LoggingConfig& logging) {
    std::unordered_map<std::string, std::shared_ptr<ILogSink>> sinks;
    for (const auto& s : logging.sinks) {
        if (s.type == "csv") {
            sinks[s.id] = std::make_shared<CsvLogSink>(s.path);
        } else if (s.type == "sqlite") {
            sinks[s.id] = std::make_shared<SqliteLogSink>(s.path, s.table);
        } else if (s.type == "influxdb") {
            sinks[s.id] = std::make_shared<InfluxLogSink>(s.url, s.token, s.org, s.bucket, s.measurement);
        }
    }
    return sinks;
}

std::unordered_map<std::string, std::string> buildTagLabels(const std::vector<TagConfig>& tags) {
    std::unordered_map<std::string, std::string> labels;
    for (const auto& t : tags) {
        if (!t.label.empty()) labels[t.id] = t.label;
    }
    return labels;
}

std::unordered_map<std::string, std::string> buildTagDevices(const std::vector<TagConfig>& tags) {
    std::unordered_map<std::string, std::string> devices;
    for (const auto& t : tags) {
        if (!t.device.empty()) devices[t.id] = t.device;
    }
    return devices;
}

} // namespace

Application::Application(AppConfig config, bool headless)
    : config_(std::move(config)), headless_(headless) {}

int Application::run() {
    ValueCache cache;
    TrendBuffer trendBuffer;
    PlcIoThread io(config_.plc, config_.tags, cache, &trendBuffer);
    LogManager logManager(config_.logging.rules, buildSinks(config_.logging), cache,
                           buildTagLabels(config_.tags), buildTagDevices(config_.tags));

    io.start();
    logManager.start();

    if (headless_) {
        std::atomic<bool> stopFlag{false};
        std::condition_variable cv;
        std::mutex mutex;
        g_stopFlag = &stopFlag;
        g_stopCv = &cv;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        std::cout << "plc_gui: running headless. Ctrl+C to stop.\n";
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return stopFlag.load(); });
    } else {
        auto panels = PanelFactory::build(config_.panels, config_.tags);

        std::unique_ptr<TrendHistoryQuery> historyQuery;
        for (const auto& sink : config_.logging.sinks) {
            if (sink.type == "sqlite") {
                historyQuery = std::make_unique<TrendHistoryQuery>(sink.path, sink.table);
                break;
            }
        }

        AppWindow window(config_.plc.name, std::move(panels), cache, io,
                          config_.tags, trendBuffer, historyQuery.get());
        window.run();
    }

    logManager.stop();
    io.stop();
    return 0;
}
