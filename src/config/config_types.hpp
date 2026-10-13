#pragma once
#include "plc/plc_value.hpp"
#include <map>
#include <string>
#include <vector>

struct PlcConnectionConfig {
    std::string name = "PLC"; // display name for the GUI's outermost window
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
    // Optional human-friendly name; empty means "use id" (see
    // tagDisplayLabel below).
    std::string label;
    // Optional; the physical device this tag belongs to (e.g. "Camera 1").
    // Written to every logging sink next to the label, so two devices that
    // share a label (e.g. two cameras both having a "Finished" signal) stay
    // in separate series instead of colliding into one.
    std::string device;
};

// The display name for a tag: its `label` if one is configured, otherwise
// its raw id. This single rule is used for widget captions, trend chart
// names, and the tag_id column written to every logging sink.
inline std::string tagDisplayLabel(const TagConfig& tag) {
    return tag.label.empty() ? tag.id : tag.label;
}

// YAML names: "slider", "input_number", "toggle", "button",
// "text_display", "led_indicator" (see ConfigLoader::parseWidgetType).
enum class WidgetType { Slider, InputNumber, Toggle, Button, TextDisplay, LedIndicator };

// The YAML name of a widget type, used in error messages.
inline const char* widgetTypeName(WidgetType type) {
    switch (type) {
        case WidgetType::Slider:       return "slider";
        case WidgetType::InputNumber:  return "input_number";
        case WidgetType::Toggle:       return "toggle";
        case WidgetType::Button:       return "button";
        case WidgetType::TextDisplay:  return "text_display";
        case WidgetType::LedIndicator: return "led_indicator";
    }
    return "unknown";
}

// Writable widgets send values to the PLC; the others only display them.
inline bool isWritableWidget(WidgetType type) {
    switch (type) {
        case WidgetType::Slider:
        case WidgetType::InputNumber:
        case WidgetType::Toggle:
        case WidgetType::Button:
            return true;
        case WidgetType::TextDisplay:
        case WidgetType::LedIndicator:
            return false;
    }
    return false;
}

struct WidgetConfig {
    WidgetType type = WidgetType::TextDisplay;
    std::string tag;    // TagConfig::id
    std::map<std::string, std::string> params; // label, min, max, format, ... (widget-specific)
};

// A labeled, collapsible sub-section of widgets nested inside a panel —
// e.g. a device's "Inputs" and "Outputs" both contained within one panel
// for that device, rather than each being its own top-level window.
struct WidgetGroupConfig {
    std::string title;
    std::vector<WidgetConfig> widgets;
};

struct PanelConfig {
    std::string title;
    std::string layout = "vertical";
    std::vector<WidgetConfig> widgets;      // ungrouped widgets, rendered flat
    std::vector<WidgetGroupConfig> groups;  // optional labeled sub-sections
};

// YAML names: "csv", "sqlite", "influxdb".
enum class LogSinkType { Csv, Sqlite, InfluxDb };

struct LogSinkConfig {
    std::string id;
    LogSinkType type = LogSinkType::Csv;
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
