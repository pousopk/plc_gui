#pragma once
#include "config/config_types.hpp"
#include <stdexcept>
#include <string>

class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& msg) : std::runtime_error(msg) {}
};

class ConfigLoader {
public:
    static AppConfig load(const std::string& path);
};
