#include "config/config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <set>
#include <sstream>

namespace {

const std::set<std::string> kKnownWidgetTypes = {
    "slider", "input_number", "toggle", "button", "text_display", "led_indicator"
};
const std::set<std::string> kKnownSinkTypes = {"csv", "sqlite"};
const std::set<std::string> kReservedWidgetKeys = {"type", "tag"};

[[noreturn]] void fail(const std::string& context, const std::string& reason) {
    std::ostringstream oss;
    oss << context << ": " << reason;
    throw ConfigError(oss.str());
}

YAML::Node requireNode(const YAML::Node& parent, const std::string& key, const std::string& context) {
    YAML::Node node = parent[key];
    if (!node) fail(context + "." + key, "missing required field");
    return node;
}

PlcType parsePlcType(const std::string& s, const std::string& context) {
    if (s == "bool") return PlcType::Bool;
    if (s == "int") return PlcType::Int;
    if (s == "real") return PlcType::Real;
    fail(context, "unknown type '" + s + "' (expected bool|int|real)");
}

PlcConnectionConfig parsePlc(const YAML::Node& root) {
    YAML::Node node = requireNode(root, "plc", "root");
    PlcConnectionConfig cfg;
    cfg.ip = requireNode(node, "ip", "plc").as<std::string>();
    cfg.rack = node["rack"] ? node["rack"].as<int>() : cfg.rack;
    cfg.slot = node["slot"] ? node["slot"].as<int>() : cfg.slot;
    cfg.connectTimeoutMs = node["connect_timeout_ms"] ? node["connect_timeout_ms"].as<int>() : cfg.connectTimeoutMs;
    if (node["reconnect_backoff_ms"]) {
        cfg.reconnectBackoffMs = node["reconnect_backoff_ms"].as<std::vector<int>>();
    }
    return cfg;
}

std::vector<TagConfig> parseTags(const YAML::Node& root) {
    std::vector<TagConfig> tags;
    YAML::Node node = root["tags"];
    if (!node) return tags;
    for (const auto& item : node) {
        std::string ctx = "tags[" + std::to_string(tags.size()) + "]";
        TagConfig t;
        t.id = requireNode(item, "id", ctx).as<std::string>();
        t.db = requireNode(item, "db", ctx).as<int>();
        t.type = parsePlcType(requireNode(item, "type", ctx).as<std::string>(), ctx + ".type");
        t.offset = requireNode(item, "offset", ctx).as<int>();
        t.bit = item["bit"] ? item["bit"].as<int>() : 0;
        t.updateIntervalMs = requireNode(item, "update_interval_ms", ctx).as<int>();
        tags.push_back(std::move(t));
    }
    return tags;
}

WidgetConfig parseWidget(const YAML::Node& item, const std::string& ctx) {
    WidgetConfig w;
    w.type = requireNode(item, "type", ctx).as<std::string>();
    if (!kKnownWidgetTypes.count(w.type)) fail(ctx + ".type", "unknown widget type '" + w.type + "'");
    w.tag = requireNode(item, "tag", ctx).as<std::string>();
    for (const auto& kv : item) {
        std::string key = kv.first.as<std::string>();
        if (kReservedWidgetKeys.count(key)) continue;
        w.params[key] = kv.second.as<std::string>();
    }
    return w;
}

std::vector<PanelConfig> parsePanels(const YAML::Node& root) {
    std::vector<PanelConfig> panels;
    YAML::Node node = root["panels"];
    if (!node) return panels;
    for (const auto& item : node) {
        std::string ctx = "panels[" + std::to_string(panels.size()) + "]";
        PanelConfig p;
        p.title = requireNode(item, "title", ctx).as<std::string>();
        p.layout = item["layout"] ? item["layout"].as<std::string>() : std::string("vertical");
        if (p.layout != "vertical") fail(ctx + ".layout", "unsupported layout '" + p.layout + "' (only 'vertical' is implemented)");
        if (item["widgets"]) {
            int i = 0;
            for (const auto& w : item["widgets"]) {
                p.widgets.push_back(parseWidget(w, ctx + ".widgets[" + std::to_string(i++) + "]"));
            }
        }
        panels.push_back(std::move(p));
    }
    return panels;
}

LoggingConfig parseLogging(const YAML::Node& root) {
    LoggingConfig logging;
    YAML::Node node = requireNode(root, "logging", "root");

    if (node["sinks"]) {
        for (const auto& item : node["sinks"]) {
            std::string ctx = "logging.sinks[" + std::to_string(logging.sinks.size()) + "]";
            LogSinkConfig s;
            s.id = requireNode(item, "id", ctx).as<std::string>();
            s.type = requireNode(item, "type", ctx).as<std::string>();
            if (!kKnownSinkTypes.count(s.type)) fail(ctx + ".type", "unknown sink type '" + s.type + "'");
            s.path = requireNode(item, "path", ctx).as<std::string>();
            s.table = item["table"] ? item["table"].as<std::string>() : std::string();
            logging.sinks.push_back(std::move(s));
        }
    }
    if (node["rules"]) {
        for (const auto& item : node["rules"]) {
            std::string ctx = "logging.rules[" + std::to_string(logging.rules.size()) + "]";
            LogRuleConfig r;
            r.tags = requireNode(item, "tags", ctx).as<std::vector<std::string>>();
            r.intervalMs = requireNode(item, "interval_ms", ctx).as<int>();
            r.sinks = requireNode(item, "sinks", ctx).as<std::vector<std::string>>();
            logging.rules.push_back(std::move(r));
        }
    }
    return logging;
}

void validateReferences(const AppConfig& cfg) {
    std::set<std::string> tagIds;
    for (const auto& t : cfg.tags) tagIds.insert(t.id);

    for (size_t pi = 0; pi < cfg.panels.size(); ++pi) {
        for (size_t wi = 0; wi < cfg.panels[pi].widgets.size(); ++wi) {
            const auto& w = cfg.panels[pi].widgets[wi];
            if (!tagIds.count(w.tag)) {
                fail("panels[" + std::to_string(pi) + "].widgets[" + std::to_string(wi) + "].tag",
                     "references unknown tag '" + w.tag + "'");
            }
        }
    }

    std::set<std::string> sinkIds;
    for (const auto& s : cfg.logging.sinks) sinkIds.insert(s.id);

    for (size_t ri = 0; ri < cfg.logging.rules.size(); ++ri) {
        const auto& r = cfg.logging.rules[ri];
        std::string ctx = "logging.rules[" + std::to_string(ri) + "]";
        for (const auto& tagId : r.tags) {
            if (!tagIds.count(tagId)) fail(ctx + ".tags", "references unknown tag '" + tagId + "'");
        }
        for (const auto& sinkId : r.sinks) {
            if (!sinkIds.count(sinkId)) fail(ctx + ".sinks", "references unknown sink '" + sinkId + "'");
        }
    }
}

} // namespace

AppConfig ConfigLoader::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("failed to parse YAML: ") + e.what());
    }

    AppConfig cfg;
    cfg.plc = parsePlc(root);
    cfg.tags = parseTags(root);
    cfg.panels = parsePanels(root);
    cfg.logging = parseLogging(root);
    validateReferences(cfg);
    return cfg;
}
