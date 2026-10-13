#include "config/config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <map>
#include <set>
#include <unordered_map>

// Every parse/validate function below takes a `path` argument: the location
// of the YAML node being read (e.g. "panels[0].widgets[2]"). It is only used
// to make error messages point at the exact config line that is wrong.

namespace {

// Widget keys that WidgetConfig stores in dedicated fields; every other key
// on a widget entry goes into WidgetConfig::params.
const std::set<std::string> kReservedWidgetKeys = {"type", "tag"};

[[noreturn]] void fail(const std::string& path, const std::string& reason) {
    throw ConfigError(path + ": " + reason);
}

// Path of the element at `index` in the list at `listPath`, e.g.
// atIndex("tags", 3) == "tags[3]".
std::string atIndex(const std::string& listPath, size_t index) {
    return listPath + "[" + std::to_string(index) + "]";
}

YAML::Node requireNode(const YAML::Node& parent, const std::string& key, const std::string& parentPath) {
    YAML::Node node = parent[key];
    if (!node) fail(parentPath + "." + key, "missing required field");
    return node;
}

// Returns parent[key] converted to T, or `fallback` when the key is absent.
template <typename T>
T readOptional(const YAML::Node& parent, const std::string& key, const T& fallback) {
    YAML::Node node = parent[key];
    if (!node) return fallback;
    return node.as<T>();
}

PlcType parsePlcType(const std::string& name, const std::string& path) {
    if (name == "bool") return PlcType::Bool;
    if (name == "int") return PlcType::Int;
    if (name == "uint") return PlcType::UInt;
    if (name == "usint") return PlcType::USInt;
    if (name == "real") return PlcType::Real;
    if (name == "time") return PlcType::Time;
    fail(path, "unknown type '" + name + "' (expected bool|int|uint|usint|real|time)");
}

WidgetType parseWidgetType(const std::string& name, const std::string& path) {
    if (name == "slider") return WidgetType::Slider;
    if (name == "input_number") return WidgetType::InputNumber;
    if (name == "toggle") return WidgetType::Toggle;
    if (name == "button") return WidgetType::Button;
    if (name == "text_display") return WidgetType::TextDisplay;
    if (name == "led_indicator") return WidgetType::LedIndicator;
    fail(path, "unknown widget type '" + name + "'");
}

LogSinkType parseLogSinkType(const std::string& name, const std::string& path) {
    if (name == "csv") return LogSinkType::Csv;
    if (name == "sqlite") return LogSinkType::Sqlite;
    if (name == "influxdb") return LogSinkType::InfluxDb;
    fail(path, "unknown sink type '" + name + "'");
}

PlcConnectionConfig parsePlc(const YAML::Node& root) {
    YAML::Node node = requireNode(root, "plc", "root");
    PlcConnectionConfig plc;
    plc.name = readOptional(node, "name", plc.name);
    plc.ip = requireNode(node, "ip", "plc").as<std::string>();
    plc.rack = readOptional(node, "rack", plc.rack);
    plc.slot = readOptional(node, "slot", plc.slot);
    plc.connectTimeoutMs = readOptional(node, "connect_timeout_ms", plc.connectTimeoutMs);
    plc.reconnectBackoffMs = readOptional(node, "reconnect_backoff_ms", plc.reconnectBackoffMs);
    return plc;
}

std::vector<TagConfig> parseTags(const YAML::Node& root) {
    std::vector<TagConfig> tags;
    YAML::Node tagsNode = root["tags"];
    if (!tagsNode) return tags;

    for (const auto& tagNode : tagsNode) {
        std::string path = atIndex("tags", tags.size());
        TagConfig tag;
        tag.id = requireNode(tagNode, "id", path).as<std::string>();
        tag.db = requireNode(tagNode, "db", path).as<int>();
        tag.type = parsePlcType(requireNode(tagNode, "type", path).as<std::string>(), path + ".type");
        tag.offset = requireNode(tagNode, "offset", path).as<int>();
        tag.bit = readOptional(tagNode, "bit", 0);
        tag.updateIntervalMs = requireNode(tagNode, "update_interval_ms", path).as<int>();
        tag.label = readOptional(tagNode, "label", std::string());
        tag.device = readOptional(tagNode, "device", std::string());
        tags.push_back(std::move(tag));
    }
    return tags;
}

WidgetConfig parseWidget(const YAML::Node& widgetNode, const std::string& path) {
    WidgetConfig widget;
    widget.type = parseWidgetType(requireNode(widgetNode, "type", path).as<std::string>(), path + ".type");
    widget.tag = requireNode(widgetNode, "tag", path).as<std::string>();
    for (const auto& keyValue : widgetNode) {
        std::string key = keyValue.first.as<std::string>();
        if (kReservedWidgetKeys.count(key)) continue;
        widget.params[key] = keyValue.second.as<std::string>();
    }
    return widget;
}

std::vector<WidgetConfig> parseWidgetList(const YAML::Node& listNode, const std::string& listPath) {
    std::vector<WidgetConfig> widgets;
    if (!listNode) return widgets;
    for (const auto& widgetNode : listNode) {
        widgets.push_back(parseWidget(widgetNode, atIndex(listPath, widgets.size())));
    }
    return widgets;
}

std::vector<PanelConfig> parsePanels(const YAML::Node& root) {
    std::vector<PanelConfig> panels;
    YAML::Node panelsNode = root["panels"];
    if (!panelsNode) return panels;

    for (const auto& panelNode : panelsNode) {
        std::string path = atIndex("panels", panels.size());
        PanelConfig panel;
        panel.title = requireNode(panelNode, "title", path).as<std::string>();
        panel.layout = readOptional(panelNode, "layout", std::string("vertical"));
        if (panel.layout != "vertical") {
            fail(path + ".layout", "unsupported layout '" + panel.layout + "' (only 'vertical' is implemented)");
        }
        panel.widgets = parseWidgetList(panelNode["widgets"], path + ".widgets");

        if (panelNode["groups"]) {
            for (const auto& groupNode : panelNode["groups"]) {
                std::string groupPath = atIndex(path + ".groups", panel.groups.size());
                WidgetGroupConfig group;
                group.title = requireNode(groupNode, "title", groupPath).as<std::string>();
                group.widgets = parseWidgetList(groupNode["widgets"], groupPath + ".widgets");
                panel.groups.push_back(std::move(group));
            }
        }
        panels.push_back(std::move(panel));
    }
    return panels;
}

// --- device_types: / devices: -- reusable "device blueprint" expansion ---
//
// A device_types entry describes one repeatable physical device's fields
// once (offset/bit/type/label/widget/group). Each devices entry
// instantiates it with just an id prefix, DB number, and display name.
// expandDevices() turns this into exactly the same TagConfig/PanelConfig
// shapes that direct tags:/panels: produce, and parseLogging() expands a
// rule's `devices:` into tag ids -- so nothing downstream (PlcIoThread,
// PanelFactory, LogManager, widgets) needs to know this shorthand exists.

struct DeviceFieldBlueprint {
    std::string id;
    PlcType type;
    int offset;
    int bit;
    std::string label;
    bool writable;
    WidgetType widget;
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
    // Added to every field's blueprint offset; lets several instances of the
    // same device_type share one DB at different byte offsets.
    int offset = 0;
    std::string label;
};

// The tags and panels generated from the devices: section.
struct ExpandedDevices {
    std::vector<TagConfig> tags;
    std::vector<PanelConfig> panels;
};

// The id of the tag generated for one field of one device instance.
std::string deviceTagId(const std::string& deviceId, const std::string& fieldId) {
    return deviceId + "_" + fieldId;
}

DeviceFieldBlueprint parseDeviceField(const YAML::Node& fieldNode, const std::string& path) {
    DeviceFieldBlueprint field;
    field.id = requireNode(fieldNode, "id", path).as<std::string>();
    field.type = parsePlcType(requireNode(fieldNode, "type", path).as<std::string>(), path + ".type");
    field.offset = requireNode(fieldNode, "offset", path).as<int>();
    field.bit = readOptional(fieldNode, "bit", 0);
    field.label = readOptional(fieldNode, "label", field.id);
    field.writable = readOptional(fieldNode, "writable", false);
    field.widget = parseWidgetType(requireNode(fieldNode, "widget", path).as<std::string>(), path + ".widget");

    // `writable` must agree with the widget, so the YAML states explicitly
    // which fields the GUI is allowed to change.
    std::string widgetName = widgetTypeName(field.widget);
    bool widgetIsWritable = isWritableWidget(field.widget);
    if (field.writable && !widgetIsWritable) {
        fail(path, "writable: true but widget '" + widgetName +
                       "' is read-only (use toggle/button/input_number/slider)");
    }
    if (!field.writable && widgetIsWritable) {
        fail(path, "widget '" + widgetName +
                       "' is writable but 'writable: true' is missing "
                       "(set it, or use led_indicator/text_display)");
    }

    field.group = requireNode(fieldNode, "group", path).as<std::string>();
    return field;
}

std::map<std::string, DeviceTypeBlueprint> parseDeviceTypes(const YAML::Node& root) {
    std::map<std::string, DeviceTypeBlueprint> deviceTypes;
    YAML::Node deviceTypesNode = root["device_types"];
    if (!deviceTypesNode) return deviceTypes;

    for (const auto& entry : deviceTypesNode) {
        std::string typeName = entry.first.as<std::string>();
        const YAML::Node& typeNode = entry.second;
        std::string path = "device_types." + typeName;

        DeviceTypeBlueprint blueprint;
        blueprint.updateIntervalMs = requireNode(typeNode, "update_interval_ms", path).as<int>();
        YAML::Node fieldsNode = requireNode(typeNode, "fields", path);
        for (const auto& fieldNode : fieldsNode) {
            std::string fieldPath = atIndex(path + ".fields", blueprint.fields.size());
            blueprint.fields.push_back(parseDeviceField(fieldNode, fieldPath));
        }
        deviceTypes[typeName] = std::move(blueprint);
    }
    return deviceTypes;
}

std::vector<DeviceInstance> parseDevices(const YAML::Node& root) {
    std::vector<DeviceInstance> devices;
    YAML::Node devicesNode = root["devices"];
    if (!devicesNode) return devices;

    std::set<std::string> seenIds;
    for (const auto& deviceNode : devicesNode) {
        std::string path = atIndex("devices", devices.size());
        DeviceInstance device;
        device.type = requireNode(deviceNode, "type", path).as<std::string>();
        device.id = requireNode(deviceNode, "id", path).as<std::string>();
        device.db = requireNode(deviceNode, "db", path).as<int>();
        device.offset = readOptional(deviceNode, "offset", 0);
        device.label = requireNode(deviceNode, "label", path).as<std::string>();
        if (!seenIds.insert(device.id).second) {
            fail(path + ".id", "duplicate device id '" + device.id + "' (each device needs a distinct id)");
        }
        devices.push_back(std::move(device));
    }
    return devices;
}

// Generates one tag per blueprint field and one panel per device (with the
// fields' widgets grouped by their `group`, in first-seen order).
ExpandedDevices expandDevices(const std::map<std::string, DeviceTypeBlueprint>& deviceTypes,
                              const std::vector<DeviceInstance>& devices) {
    ExpandedDevices expanded;
    for (size_t deviceIndex = 0; deviceIndex < devices.size(); ++deviceIndex) {
        const DeviceInstance& device = devices[deviceIndex];
        auto typeIt = deviceTypes.find(device.type);
        if (typeIt == deviceTypes.end()) {
            fail(atIndex("devices", deviceIndex) + ".type", "unknown device type '" + device.type + "'");
        }
        const DeviceTypeBlueprint& blueprint = typeIt->second;

        PanelConfig panel;
        panel.title = device.label;
        std::map<std::string, size_t> groupIndexByTitle;

        for (const auto& field : blueprint.fields) {
            TagConfig tag;
            tag.id = deviceTagId(device.id, field.id);
            tag.db = device.db;
            tag.type = field.type;
            tag.offset = field.offset + device.offset;
            tag.bit = field.bit;
            tag.updateIntervalMs = blueprint.updateIntervalMs;
            tag.label = field.label;
            tag.device = device.label;
            expanded.tags.push_back(tag);

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

        expanded.panels.push_back(std::move(panel));
    }
    return expanded;
}

LogSinkConfig parseLogSink(const YAML::Node& sinkNode, const std::string& path) {
    LogSinkConfig sink;
    sink.id = requireNode(sinkNode, "id", path).as<std::string>();
    sink.type = parseLogSinkType(requireNode(sinkNode, "type", path).as<std::string>(), path + ".type");
    sink.table = readOptional(sinkNode, "table", std::string());
    switch (sink.type) {
        case LogSinkType::Csv:
        case LogSinkType::Sqlite:
            sink.path = requireNode(sinkNode, "path", path).as<std::string>();
            break;
        case LogSinkType::InfluxDb:
            sink.url = requireNode(sinkNode, "url", path).as<std::string>();
            sink.token = requireNode(sinkNode, "token", path).as<std::string>();
            sink.org = requireNode(sinkNode, "org", path).as<std::string>();
            sink.bucket = requireNode(sinkNode, "bucket", path).as<std::string>();
            sink.measurement = readOptional(sinkNode, "measurement", std::string("plc_readings"));
            break;
    }
    return sink;
}

LogRuleConfig parseLogRule(const YAML::Node& ruleNode, const std::string& path,
                           const std::map<std::string, DeviceTypeBlueprint>& deviceTypes,
                           const std::unordered_map<std::string, const DeviceInstance*>& deviceById) {
    LogRuleConfig rule;
    rule.tags = readOptional(ruleNode, "tags", std::vector<std::string>());

    // `devices: [cam1]` is shorthand for every tag generated for cam1.
    if (ruleNode["devices"]) {
        for (const auto& deviceIdNode : ruleNode["devices"]) {
            std::string deviceId = deviceIdNode.as<std::string>();
            auto deviceIt = deviceById.find(deviceId);
            if (deviceIt == deviceById.end()) {
                fail(path + ".devices", "references unknown device '" + deviceId + "'");
            }
            const DeviceInstance& device = *deviceIt->second;
            auto typeIt = deviceTypes.find(device.type);
            if (typeIt == deviceTypes.end()) {
                fail(path + ".devices", "device '" + deviceId + "' has unknown type '" + device.type + "'");
            }
            for (const auto& field : typeIt->second.fields) {
                rule.tags.push_back(deviceTagId(deviceId, field.id));
            }
        }
    }

    if (rule.tags.empty()) fail(path, "must specify at least one of 'tags' or 'devices'");
    rule.intervalMs = requireNode(ruleNode, "interval_ms", path).as<int>();
    rule.sinks = requireNode(ruleNode, "sinks", path).as<std::vector<std::string>>();
    return rule;
}

LoggingConfig parseLogging(const YAML::Node& root,
                           const std::map<std::string, DeviceTypeBlueprint>& deviceTypes,
                           const std::vector<DeviceInstance>& devices) {
    LoggingConfig logging;
    YAML::Node loggingNode = requireNode(root, "logging", "root");

    std::unordered_map<std::string, const DeviceInstance*> deviceById;
    for (const auto& device : devices) deviceById[device.id] = &device;

    if (loggingNode["sinks"]) {
        for (const auto& sinkNode : loggingNode["sinks"]) {
            std::string path = atIndex("logging.sinks", logging.sinks.size());
            logging.sinks.push_back(parseLogSink(sinkNode, path));
        }
    }
    if (loggingNode["rules"]) {
        for (const auto& ruleNode : loggingNode["rules"]) {
            std::string path = atIndex("logging.rules", logging.rules.size());
            logging.rules.push_back(parseLogRule(ruleNode, path, deviceTypes, deviceById));
        }
    }
    return logging;
}

// Two tags in the same DB whose byte ranges [offset, offset + size) overlap
// almost always mean a copy-paste offset mistake -- except two Bool tags at
// the same offset with different bit numbers, which is ordinary bit-packing
// (e.g. "in"/"reset"/"trigger_software" all living in byte 0 of a struct).
void validateTagLayout(const std::vector<TagConfig>& tags) {
    std::set<std::string> seenIds;
    for (size_t tagIndex = 0; tagIndex < tags.size(); ++tagIndex) {
        if (!seenIds.insert(tags[tagIndex].id).second) {
            fail(atIndex("tags", tagIndex) + ".id", "duplicate tag id '" + tags[tagIndex].id + "'");
        }
    }

    std::map<int, std::vector<const TagConfig*>> tagsByDb;
    for (const auto& tag : tags) tagsByDb[tag.db].push_back(&tag);

    for (const auto& [db, dbTags] : tagsByDb) {
        for (size_t a = 0; a < dbTags.size(); ++a) {
            for (size_t b = a + 1; b < dbTags.size(); ++b) {
                const TagConfig& first = *dbTags[a];
                const TagConfig& second = *dbTags[b];
                int firstEnd = first.offset + plcTypeByteSize(first.type);
                int secondEnd = second.offset + plcTypeByteSize(second.type);
                bool overlaps = first.offset < secondEnd && second.offset < firstEnd;
                if (!overlaps) continue;

                bool packedBitsInSameByte = first.type == PlcType::Bool && second.type == PlcType::Bool &&
                                            first.offset == second.offset && first.bit != second.bit;
                if (packedBitsInSameByte) continue;

                fail("tags", "tag '" + first.id + "' (db " + std::to_string(db) + ", offset " +
                                 std::to_string(first.offset) + ") overlaps tag '" + second.id + "' (db " +
                                 std::to_string(db) + ", offset " + std::to_string(second.offset) + ")");
            }
        }
    }
}

void validateWidgetTags(const std::vector<WidgetConfig>& widgets, const std::string& listPath,
                        const std::set<std::string>& tagIds) {
    for (size_t widgetIndex = 0; widgetIndex < widgets.size(); ++widgetIndex) {
        const WidgetConfig& widget = widgets[widgetIndex];
        if (!tagIds.count(widget.tag)) {
            fail(atIndex(listPath, widgetIndex) + ".tag", "references unknown tag '" + widget.tag + "'");
        }
    }
}

// Checks that every widget and logging rule points at a tag that exists,
// and every logging rule points at a sink that exists.
void validateReferences(const AppConfig& config) {
    std::set<std::string> tagIds;
    for (const auto& tag : config.tags) tagIds.insert(tag.id);

    for (size_t panelIndex = 0; panelIndex < config.panels.size(); ++panelIndex) {
        const PanelConfig& panel = config.panels[panelIndex];
        std::string panelPath = atIndex("panels", panelIndex);
        validateWidgetTags(panel.widgets, panelPath + ".widgets", tagIds);
        for (size_t groupIndex = 0; groupIndex < panel.groups.size(); ++groupIndex) {
            std::string groupPath = atIndex(panelPath + ".groups", groupIndex);
            validateWidgetTags(panel.groups[groupIndex].widgets, groupPath + ".widgets", tagIds);
        }
    }

    std::set<std::string> sinkIds;
    for (const auto& sink : config.logging.sinks) sinkIds.insert(sink.id);

    for (size_t ruleIndex = 0; ruleIndex < config.logging.rules.size(); ++ruleIndex) {
        const LogRuleConfig& rule = config.logging.rules[ruleIndex];
        std::string rulePath = atIndex("logging.rules", ruleIndex);
        for (const auto& tagId : rule.tags) {
            if (!tagIds.count(tagId)) fail(rulePath + ".tags", "references unknown tag '" + tagId + "'");
        }
        for (const auto& sinkId : rule.sinks) {
            if (!sinkIds.count(sinkId)) fail(rulePath + ".sinks", "references unknown sink '" + sinkId + "'");
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

    AppConfig config;
    config.plc = parsePlc(root);
    config.tags = parseTags(root);
    config.panels = parsePanels(root);

    auto deviceTypes = parseDeviceTypes(root);
    auto devices = parseDevices(root);
    ExpandedDevices expanded = expandDevices(deviceTypes, devices);
    config.tags.insert(config.tags.end(), expanded.tags.begin(), expanded.tags.end());
    config.panels.insert(config.panels.end(), expanded.panels.begin(), expanded.panels.end());

    config.logging = parseLogging(root, deviceTypes, devices);
    validateTagLayout(config.tags);
    validateReferences(config);
    return config;
}
