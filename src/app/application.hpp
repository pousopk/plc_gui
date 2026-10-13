#pragma once
#include "config/config_types.hpp"
#include "plc/plc_io_thread.hpp"
#include "plc/trend_buffer.hpp"
#include "plc/value_cache.hpp"

// Wires the core together: starts the PLC I/O thread and the logging thread,
// then either opens the GUI or (headless) waits for SIGINT/SIGTERM.
class Application {
public:
    Application(AppConfig config, bool headless);
    int run(); // returns the process exit code

private:
    // Headless mode: blocks until SIGINT or SIGTERM is received.
    void waitForStopSignal();
    // GUI mode: blocks until the window is closed.
    void runGui(ValueCache& cache, PlcIoThread& io, TrendBuffer& trendBuffer);

    AppConfig config_;
    bool headless_;
};
