#include "app/application.hpp"
#include "config/config_loader.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string configPath = "config/plc.yaml";
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--headless") {
            headless = true;
        }
    }

    AppConfig config;
    try {
        config = ConfigLoader::load(configPath);
    } catch (const ConfigError& e) {
        std::cerr << "plc_gui: config error: " << e.what() << "\n";
        return 1;
    }

    Application app(std::move(config), headless);
    return app.run();
}
