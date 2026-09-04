#pragma once
#include "plc/plc_value.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

struct PlcConnectionConfig {
    std::string ip;
    int rack = 0;
    int slot = 1;
    int connectTimeoutMs = 2000;
    std::vector<int> reconnectBackoffMs = {500, 1000, 2000, 5000};
};

struct TagConfig {
    std::string id;
    int db = 0;
    PlcType type = PlcType::Int;
    int offset = 0;
    int bit = 0; // only meaningful for PlcType::Bool
    int updateIntervalMs = 1000;
};

struct WidgetConfig {
    std::string type;   // "slider" | "input_number" | "toggle" | "button" | "text_display" | "led_indicator"
    std::string tag;    // TagConfig::id
    std::map<std::string, std::string> params; // label, min, max, format, ... (widget-specific)
};

struct PanelConfig {
    std::string title;
    std::string layout = "vertical";
    std::vector<WidgetConfig> widgets;
};

struct LogSinkConfig {
    std::string id;
    std::string type; // "csv" | "sqlite" | "influxdb"
    std::string path;  // csv, sqlite
    std::string table; // sqlite only
    std::string url;         // influxdb only, e.g. "http://localhost:8086"
    std::string token;       // influxdb only
    std::string org;         // influxdb only
    std::string bucket;      // influxdb only
    std::string measurement; // influxdb only
};

struct LogRuleConfig {
    std::vector<std::string> tags; // TagConfig::id list
    int intervalMs = 1000;
    std::vector<std::string> sinks; // LogSinkConfig::id list
};

struct LoggingConfig {
    std::vector<LogSinkConfig> sinks;
    std::vector<LogRuleConfig> rules;
};

struct AppConfig {
    PlcConnectionConfig plc;
    std::vector<TagConfig> tags;
    std::vector<PanelConfig> panels;
    LoggingConfig logging;
};
