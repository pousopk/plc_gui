#pragma once
#include "config/config_types.hpp"

class Application {
public:
    Application(AppConfig config, bool headless);
    int run(); // returns the process exit code

private:
    AppConfig config_;
    bool headless_;
};
