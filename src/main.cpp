#include "app/application.hpp"
#include "config/config_loader.hpp"
#include "ui/panel_factory.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    std::string configPath = "config/plc.yaml";
    bool headless = false;
    bool validateOnly = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--headless") {
            headless = true;
        } else if (arg == "--validate-config") {
            validateOnly = true;
        }
    }

    AppConfig config;
    try {
        config = ConfigLoader::load(configPath);
    } catch (const ConfigError& e) {
        std::cerr << "plc_gui: config error: " << e.what() << "\n";
        return 1;
    }

    if (validateOnly) {
        // Also force widget construction (parses params like a slider's
        // min/max via std::stof) without ever connecting to the PLC,
        // opening a window, or touching a log sink -- PanelFactory::build()
        // is otherwise only called in GUI mode, so a malformed widget param
        // would go uncaught in a headless-only deployment until someone
        // happened to open the GUI.
        try {
            PanelFactory::build(config.panels, config.tags);
        } catch (const std::exception& e) {
            std::cerr << "plc_gui: config error: " << e.what() << "\n";
            return 1;
        }
        std::cout << "plc_gui: config OK\n";
        return 0;
    }

    Application app(std::move(config), headless);
    return app.run();
}
