// A config-driven in-process Snap7 server standing in for real PLC
// hardware. Loads the same YAML config plc_gui would use, registers one
// server area per distinct DB the config's tags reference (sized to cover
// every tag's byte range), and continuously writes synthetic-but-changing
// values into them -- so plc_gui (pointed at 127.0.0.1) can be exercised
// end-to-end (connect, read, write, log, trend) without real hardware.
//
// Tags bound to a writable widget (slider/input_number/toggle/button) are
// never animated: they keep whatever plc_gui last wrote, the way a real
// PLC holds a setpoint, instead of being overwritten on the next tick.
//
// Doesn't validate that your DB layout matches a real PLC program -- only
// that your config's plumbing (connection, polling, logging, trends)
// actually works.
#include "config/config_loader.hpp"
#include "plc/plc_value.hpp"
#include <snap7.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};
void handleSignal(int) { g_stop = true; }

// Ids of every tag some writable widget points at, across both a panel's
// flat widgets and its groups (device_types expansion produces groups).
std::set<std::string> collectWritableTagIds(const std::vector<PanelConfig>& panels) {
    static const std::set<std::string> kWritableWidgetTypes = {"slider", "input_number", "toggle", "button"};
    std::set<std::string> ids;
    auto scan = [&](const std::vector<WidgetConfig>& widgets) {
        for (const auto& w : widgets) {
            if (kWritableWidgetTypes.count(w.type)) ids.insert(w.tag);
        }
    };
    for (const auto& panel : panels) {
        scan(panel.widgets);
        for (const auto& group : panel.groups) scan(group.widgets);
    }
    return ids;
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::string configPath = "config/plc.yaml";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) configPath = argv[++i];
    }

    AppConfig config;
    try {
        config = ConfigLoader::load(configPath);
    } catch (const ConfigError& e) {
        std::cerr << "fake_plc_server: config error: " << e.what() << "\n";
        return 1;
    }

    if (config.tags.empty()) {
        std::cerr << "fake_plc_server: " << configPath << " has no tags -- nothing to serve\n";
        return 1;
    }

    // One buffer per distinct DB, sized to cover every tag's byte range.
    std::map<int, std::vector<uint8_t>> dbBuffers;
    for (const auto& tag : config.tags) {
        int end = tag.offset + plcTypeByteSize(tag.type);
        auto& buf = dbBuffers[tag.db];
        if (static_cast<int>(buf.size()) < end) buf.resize(static_cast<size_t>(end), 0);
    }

    S7Object server = Srv_Create();
    for (auto& [db, buf] : dbBuffers) {
        Srv_RegisterArea(server, srvAreaDB, static_cast<word>(db), buf.data(), static_cast<int>(buf.size()));
    }

    // Always loopback-only, regardless of the config's own plc.ip -- that
    // field is for the real PLC; point plc_gui at 127.0.0.1 yourself (e.g.
    // a temp copy of your config) to talk to this fake server instead.
    int rc = Srv_StartTo(server, "127.0.0.1");
    if (rc != 0) {
        std::fprintf(stderr,
            "fake_plc_server: Srv_StartTo failed (code %d). Port 102 needs elevated "
            "privileges on Linux: 'sudo setcap cap_net_bind_service=+ep <path-to-fake_plc_server>' "
            "or 'sudo sysctl net.ipv4.ip_unprivileged_port_start=0'.\n", rc);
        Srv_Destroy(&server);
        return 1;
    }

    const std::set<std::string> writableTagIds = collectWritableTagIds(config.panels);

    std::printf("fake_plc_server: serving %s on 127.0.0.1:102 (%zu tags across %zu DB%s, %zu held for writes). "
                "Ctrl+C to stop.\n",
                configPath.c_str(), config.tags.size(), dbBuffers.size(), dbBuffers.size() == 1 ? "" : "s",
                writableTagIds.size());

    auto start = std::chrono::steady_clock::now();
    while (!g_stop) {
        double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (const auto& tag : config.tags) {
            if (writableTagIds.count(tag.id)) continue;
            auto& buf = dbBuffers[tag.db];
            PlcValue value;
            switch (tag.type) {
                case PlcType::Bool:
                    // Slow toggle so a "constantly on/off" signal and an
                    // actual transition are both easy to spot in the GUI.
                    value = (static_cast<int>(elapsedSeconds / 3.0) % 2) == 0;
                    break;
                case PlcType::Real:
                    value = static_cast<float>(50.0 + 50.0 * std::sin(elapsedSeconds));
                    break;
                default: // Int, UInt, USInt, Time -- a wrapping counter is enough to show movement
                    value = static_cast<int32_t>(static_cast<long long>(elapsedSeconds * 10) % 1000);
                    break;
            }
            encodePlcValue(tag.type, value, buf.data(), tag.offset, tag.bit);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    Srv_Stop(server);
    Srv_Destroy(&server);
    std::printf("fake_plc_server: stopped.\n");
    return 0;
}
