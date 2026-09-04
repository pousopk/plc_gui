#include "config/config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

const std::set<std::string> kKnownWidgetTypes = {
    "slider", "input_number", "toggle", "button", "text_display", "led_indicator"
};
const std::set<std::string> kKnownSinkTypes = {"csv", "sqlite", "influxdb"};
const std::set<std::string> kReservedWidgetKeys = {"type", "tag"};
const std::set<std::string> kWritableWidgetTypes = {"toggle", "button", "input_number", "slider"};
const std::set<std::string> kReadOnlyWidgetTypes = {"led_indicator", "text_display"};

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
    if (s == "uint") return PlcType::UInt;
    if (s == "usint") return PlcType::USInt;
    if (s == "real") return PlcType::Real;
    if (s == "time") return PlcType::Time;
    fail(context, "unknown type '" + s + "' (expected bool|int|uint|usint|real|time)");
}

PlcConnectionConfig parsePlc(const YAML::Node& root) {
    YAML::Node node = requireNode(root, "plc", "root");
    PlcConnectionConfig cfg;
    cfg.name = node["name"] ? node["name"].as<std::string>() : cfg.name;
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
        t.label = item["label"] ? item["label"].as<std::string>() : std::string();
        t.device = item["device"] ? item["device"].as<std::string>() : std::string();
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

// --- device_types: / devices: — reusable "device blueprint" expansion ---
//
// A device_types entry describes one repeatable physical device's fields
// once (offset/bit/type/label/widget/group). Each devices entry
// instantiates it with just an id prefix, DB number, and display name.
// Expansion below turns this into exactly the same TagConfig/PanelConfig/
// LogRuleConfig::tags shapes that direct tags:/panels:/rules[].tags
// already produce — so nothing downstream (PlcIoThread, PanelFactory,
// LogManager, widgets) needs any awareness this shorthand exists.

struct DeviceFieldBlueprint {
    std::string id;
    PlcType type;
    int offset;
    int bit;
    std::string label;
    bool writable;
    std::string widget;
    std::string group;
};

struct DeviceTypeBlueprint {
    std::vector<DeviceFieldBlueprint> fields;
    int updateIntervalMs;
};

struct DeviceInstance {
    std::string type;
    std::string id;
    int db;
    std::string label;
};

std::map<std::string, DeviceTypeBlueprint> parseDeviceTypes(const YAML::Node& root) {
    std::map<std::string, DeviceTypeBlueprint> types;
    YAML::Node node = root["device_types"];
    if (!node) return types;

    for (const auto& kv : node) {
        std::string typeName = kv.first.as<std::string>();
        std::string ctx = "device_types." + typeName;
        DeviceTypeBlueprint blueprint;
        blueprint.updateIntervalMs = requireNode(kv.second, "update_interval_ms", ctx).as<int>();

        YAML::Node fieldsNode = requireNode(kv.second, "fields", ctx);
        int fi = 0;
        for (const auto& f : fieldsNode) {
            std::string fctx = ctx + ".fields[" + std::to_string(fi++) + "]";
            DeviceFieldBlueprint field;
            field.id = requireNode(f, "id", fctx).as<std::string>();
            field.type = parsePlcType(requireNode(f, "type", fctx).as<std::string>(), fctx + ".type");
            field.offset = requireNode(f, "offset", fctx).as<int>();
            field.bit = f["bit"] ? f["bit"].as<int>() : 0;
            field.label = f["label"] ? f["label"].as<std::string>() : field.id;
            field.writable = f["writable"] ? f["writable"].as<bool>() : false;
            field.widget = requireNode(f, "widget", fctx).as<std::string>();
            if (!kKnownWidgetTypes.count(field.widget)) {
                fail(fctx + ".widget", "unknown widget type '" + field.widget + "'");
            }
            bool widgetIsWritable = kWritableWidgetTypes.count(field.widget) > 0;
            if (field.writable && !widgetIsWritable) {
                fail(fctx, "writable: true but widget '" + field.widget +
                               "' is read-only (use toggle/button/input_number/slider)");
            }
            if (!field.writable && widgetIsWritable) {
                fail(fctx, "widget '" + field.widget +
                               "' is writable but 'writable: true' is missing (set it, or use led_indicator/text_display)");
            }
            field.group = requireNode(f, "group", fctx).as<std::string>();
            blueprint.fields.push_back(std::move(field));
        }
        types[typeName] = std::move(blueprint);
    }
    return types;
}

std::vector<DeviceInstance> parseDevices(const YAML::Node& root) {
    std::vector<DeviceInstance> devices;
    YAML::Node node = root["devices"];
    if (!node) return devices;

    std::set<std::string> seenIds;
    int i = 0;
    for (const auto& item : node) {
        std::string ctx = "devices[" + std::to_string(i++) + "]";
        DeviceInstance d;
        d.type = requireNode(item, "type", ctx).as<std::string>();
        d.id = requireNode(item, "id", ctx).as<std::string>();
        d.db = requireNode(item, "db", ctx).as<int>();
        d.label = requireNode(item, "label", ctx).as<std::string>();
        if (!seenIds.insert(d.id).second) {
            fail(ctx + ".id", "duplicate device id '" + d.id + "' (each device needs a distinct id)");
        }
        devices.push_back(std::move(d));
    }
    return devices;
}

void expandDevices(const std::map<std::string, DeviceTypeBlueprint>& deviceTypes,
                    const std::vector<DeviceInstance>& devices,
                    AppConfig& cfg) {
    for (size_t di = 0; di < devices.size(); ++di) {
        const auto& device = devices[di];
        std::string ctx = "devices[" + std::to_string(di) + "]";
        auto typeIt = deviceTypes.find(device.type);
        if (typeIt == deviceTypes.end()) {
            fail(ctx + ".type", "unknown device type '" + device.type + "'");
        }
        const DeviceTypeBlueprint& blueprint = typeIt->second;

        PanelConfig panel;
        panel.title = device.label;
        std::map<std::string, size_t> groupIndexByTitle; // preserves first-seen group order

        for (const auto& field : blueprint.fields) {
            TagConfig tag;
            tag.id = device.id + "_" + field.id;
            tag.db = device.db;
            tag.type = field.type;
            tag.offset = field.offset;
            tag.bit = field.bit;
            tag.updateIntervalMs = blueprint.updateIntervalMs;
            tag.label = field.label;
            tag.device = device.label;
            cfg.tags.push_back(tag);

            WidgetConfig widget;
            widget.type = field.widget;
            widget.tag = tag.id;
            widget.params["label"] = field.label;

            auto groupIt = groupIndexByTitle.find(field.group);
            if (groupIt == groupIndexByTitle.end()) {
                WidgetGroupConfig group;
                group.title = field.group;
                panel.groups.push_back(std::move(group));
                groupIt = groupIndexByTitle.emplace(field.group, panel.groups.size() - 1).first;
            }
            panel.groups[groupIt->second].widgets.push_back(std::move(widget));
        }

        cfg.panels.push_back(std::move(panel));
    }
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
        if (item["groups"]) {
            int gi = 0;
            for (const auto& g : item["groups"]) {
                std::string groupCtx = ctx + ".groups[" + std::to_string(gi++) + "]";
                WidgetGroupConfig group;
                group.title = requireNode(g, "title", groupCtx).as<std::string>();
                if (g["widgets"]) {
                    int wi = 0;
                    for (const auto& w : g["widgets"]) {
                        group.widgets.push_back(parseWidget(w, groupCtx + ".widgets[" + std::to_string(wi++) + "]"));
                    }
                }
                p.groups.push_back(std::move(group));
            }
        }
        panels.push_back(std::move(p));
    }
    return panels;
}

LoggingConfig parseLogging(const YAML::Node& root,
                            const std::map<std::string, DeviceTypeBlueprint>& deviceTypes,
                            const std::vector<DeviceInstance>& devices) {
    LoggingConfig logging;
    YAML::Node node = requireNode(root, "logging", "root");

    std::unordered_map<std::string, const DeviceInstance*> deviceById;
    for (const auto& d : devices) deviceById[d.id] = &d;

    if (node["sinks"]) {
        for (const auto& item : node["sinks"]) {
            std::string ctx = "logging.sinks[" + std::to_string(logging.sinks.size()) + "]";
            LogSinkConfig s;
            s.id = requireNode(item, "id", ctx).as<std::string>();
            s.type = requireNode(item, "type", ctx).as<std::string>();
            if (!kKnownSinkTypes.count(s.type)) fail(ctx + ".type", "unknown sink type '" + s.type + "'");
            if (s.type == "csv" || s.type == "sqlite") {
                s.path = requireNode(item, "path", ctx).as<std::string>();
            }
            s.table = item["table"] ? item["table"].as<std::string>() : std::string();
            if (s.type == "influxdb") {
                s.url = requireNode(item, "url", ctx).as<std::string>();
                s.token = requireNode(item, "token", ctx).as<std::string>();
                s.org = requireNode(item, "org", ctx).as<std::string>();
                s.bucket = requireNode(item, "bucket", ctx).as<std::string>();
                s.measurement = item["measurement"] ? item["measurement"].as<std::string>() : std::string("plc_readings");
            }
            logging.sinks.push_back(std::move(s));
        }
    }
    if (node["rules"]) {
        for (const auto& item : node["rules"]) {
            std::string ctx = "logging.rules[" + std::to_string(logging.rules.size()) + "]";
            LogRuleConfig r;
            r.tags = item["tags"] ? item["tags"].as<std::vector<std::string>>() : std::vector<std::string>();
            if (item["devices"]) {
                for (const auto& devIdNode : item["devices"]) {
                    std::string deviceId = devIdNode.as<std::string>();
                    auto devIt = deviceById.find(deviceId);
                    if (devIt == deviceById.end()) {
                        fail(ctx + ".devices", "references unknown device '" + deviceId + "'");
                    }
                    auto typeIt = deviceTypes.find(devIt->second->type);
                    if (typeIt == deviceTypes.end()) {
                        fail(ctx + ".devices", "device '" + deviceId + "' has unknown type '" + devIt->second->type + "'");
                    }
                    for (const auto& field : typeIt->second.fields) {
                        r.tags.push_back(deviceId + "_" + field.id);
                    }
                }
            }
            if (r.tags.empty()) fail(ctx, "must specify at least one of 'tags' or 'devices'");
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
        for (size_t gi = 0; gi < cfg.panels[pi].groups.size(); ++gi) {
            const auto& group = cfg.panels[pi].groups[gi];
            for (size_t wi = 0; wi < group.widgets.size(); ++wi) {
                const auto& w = group.widgets[wi];
                if (!tagIds.count(w.tag)) {
                    fail("panels[" + std::to_string(pi) + "].groups[" + std::to_string(gi) +
                             "].widgets[" + std::to_string(wi) + "].tag",
                         "references unknown tag '" + w.tag + "'");
                }
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

    auto deviceTypes = parseDeviceTypes(root);
    auto devices = parseDevices(root);
    expandDevices(deviceTypes, devices, cfg); // appends to cfg.tags / cfg.panels

    cfg.logging = parseLogging(root, deviceTypes, devices);
    validateReferences(cfg);
    return cfg;
}
