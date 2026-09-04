#include <doctest/doctest.h>
#include "config/config_loader.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

std::string writeTempYaml(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() /
                ("plc_gui_test_" + std::to_string(std::rand()) + ".yaml");
    std::ofstream out(path);
    out << content;
    out.close();
    return path.string();
}

// Uses a custom raw-string delimiter (YAML) rather than the default R"( )" —
// content like `"Speed (RPM)"` contains a literal `)"`, which would
// otherwise terminate the raw string early.
const char* kValidConfig = R"YAML(
plc:
  ip: "192.168.0.10"
  rack: 0
  slot: 1
  connect_timeout_ms: 2000
  reconnect_backoff_ms: [500, 1000]

tags:
  - id: motor1_speed
    db: 10
    type: real
    offset: 0
    update_interval_ms: 100
  - id: motor1_running
    db: 10
    type: bool
    offset: 4
    bit: 0
    update_interval_ms: 100

panels:
  - title: "Motor Control"
    layout: vertical
    widgets:
      - type: slider
        tag: motor1_speed
        label: "Speed (RPM)"
        min: "0"
        max: "3000"
      - type: toggle
        tag: motor1_running
        label: "Running"

logging:
  sinks:
    - id: csv_main
      type: csv
      path: "logs/plc_log.csv"
  rules:
    - tags: [motor1_speed, motor1_running]
      interval_ms: 1000
      sinks: [csv_main]
)YAML";

} // namespace

TEST_CASE("ConfigLoader parses a valid config into typed structs") {
    auto path = writeTempYaml(kValidConfig);
    AppConfig cfg = ConfigLoader::load(path);

    CHECK(cfg.plc.ip == "192.168.0.10");
    CHECK(cfg.plc.reconnectBackoffMs == std::vector<int>{500, 1000});
    REQUIRE(cfg.tags.size() == 2);
    CHECK(cfg.tags[0].id == "motor1_speed");
    CHECK(cfg.tags[0].type == PlcType::Real);
    CHECK(cfg.tags[1].bit == 0);
    REQUIRE(cfg.panels.size() == 1);
    CHECK(cfg.panels[0].widgets.size() == 2);
    CHECK(cfg.panels[0].widgets[0].params.at("min") == "0");
    REQUIRE(cfg.logging.sinks.size() == 1);
    REQUIRE(cfg.logging.rules.size() == 1);
    CHECK(cfg.logging.rules[0].tags.size() == 2);

    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses an optional plc name, defaulting to \"PLC\" when omitted") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4", name: "PLC 1" }
tags: []
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    CHECK(cfg.plc.name == "PLC 1");
    std::remove(path.c_str());

    auto path2 = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg2 = ConfigLoader::load(path2);
    CHECK(cfg2.plc.name == "PLC");
    std::remove(path2.c_str());
}

TEST_CASE("ConfigLoader throws ConfigError with the field path on a missing required field") {
    auto path = writeTempYaml(R"(
plc:
  rack: 0
  slot: 1
tags: []
panels: []
logging:
  sinks: []
  rules: []
)");
    CHECK_THROWS_WITH_AS(ConfigLoader::load(path), doctest::Contains("plc.ip"), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses an optional tag label, defaulting to empty when omitted") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: cam1_finished, db: 1, type: bool, offset: 6, bit: 0, update_interval_ms: 100, label: "Finished" }
  - { id: cam1_ok, db: 1, type: bool, offset: 6, bit: 1, update_interval_ms: 100 }
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.tags.size() == 2);
    CHECK(cfg.tags[0].label == "Finished");
    CHECK(cfg.tags[1].label == "");
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses an optional tag device, defaulting to empty when omitted") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: cam1_finished, db: 1, type: bool, offset: 6, bit: 0, update_interval_ms: 100, device: "Camera 1" }
  - { id: cam2_finished, db: 3, type: bool, offset: 6, bit: 0, update_interval_ms: 100 }
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.tags.size() == 2);
    CHECK(cfg.tags[0].device == "Camera 1");
    CHECK(cfg.tags[1].device == "");
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses uint and usint tag types") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: frame_counter, db: 1, type: uint, offset: 0, update_interval_ms: 100 }
  - { id: global_result, db: 1, type: usint, offset: 8, update_interval_ms: 100 }
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.tags.size() == 2);
    CHECK(cfg.tags[0].type == PlcType::UInt);
    CHECK(cfg.tags[1].type == PlcType::USInt);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses a time tag type") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: timeout, db: 1, type: time, offset: 2, update_interval_ms: 100 }
panels: []
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.tags.size() == 1);
    CHECK(cfg.tags[0].type == PlcType::Time);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses an influxdb sink with all its fields") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: int, offset: 0, update_interval_ms: 100 }
panels: []
logging:
  sinks:
    - id: influx_main
      type: influxdb
      url: "http://localhost:8086"
      token: "my-token"
      org: "my-org"
      bucket: "my-bucket"
      measurement: "plc_readings"
  rules:
    - tags: [t1]
      interval_ms: 500
      sinks: [influx_main]
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.logging.sinks.size() == 1);
    const auto& s = cfg.logging.sinks[0];
    CHECK(s.type == "influxdb");
    CHECK(s.url == "http://localhost:8086");
    CHECK(s.token == "my-token");
    CHECK(s.org == "my-org");
    CHECK(s.bucket == "my-bucket");
    CHECK(s.measurement == "plc_readings");
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader defaults influxdb measurement to plc_readings when omitted") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
logging:
  sinks:
    - id: influx_main
      type: influxdb
      url: "http://localhost:8086"
      token: "my-token"
      org: "my-org"
      bucket: "my-bucket"
  rules: []
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.logging.sinks.size() == 1);
    CHECK(cfg.logging.sinks[0].measurement == "plc_readings");
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader parses panel groups, nesting widgets inside") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: bool, offset: 0, update_interval_ms: 100 }
  - { id: t2, db: 1, type: bool, offset: 0, bit: 1, update_interval_ms: 100 }
panels:
  - title: "Camera 1"
    layout: vertical
    groups:
      - title: "Inputs"
        widgets:
          - { type: toggle, tag: t1 }
      - title: "Outputs"
        widgets:
          - { type: led_indicator, tag: t2 }
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);
    REQUIRE(cfg.panels.size() == 1);
    CHECK(cfg.panels[0].title == "Camera 1");
    CHECK(cfg.panels[0].widgets.empty());
    REQUIRE(cfg.panels[0].groups.size() == 2);
    CHECK(cfg.panels[0].groups[0].title == "Inputs");
    REQUIRE(cfg.panels[0].groups[0].widgets.size() == 1);
    CHECK(cfg.panels[0].groups[0].widgets[0].tag == "t1");
    CHECK(cfg.panels[0].groups[1].title == "Outputs");
    REQUIRE(cfg.panels[0].groups[1].widgets.size() == 1);
    CHECK(cfg.panels[0].groups[1].widgets[0].tag == "t2");
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a group widget referencing an unknown tag") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: bool, offset: 0, update_interval_ms: 100 }
panels:
  - title: "Camera 1"
    groups:
      - title: "Inputs"
        widgets:
          - { type: toggle, tag: does_not_exist }
logging: { sinks: [], rules: [] }
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects an unknown widget type") {
    auto path = writeTempYaml(R"(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: int, offset: 0, update_interval_ms: 100 }
panels:
  - title: "P"
    layout: vertical
    widgets:
      - { type: bogus_widget, tag: t1 }
logging: { sinks: [], rules: [] }
)");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a widget referencing an unknown tag") {
    auto path = writeTempYaml(R"(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: int, offset: 0, update_interval_ms: 100 }
panels:
  - title: "P"
    layout: vertical
    widgets:
      - { type: text_display, tag: does_not_exist }
logging: { sinks: [], rules: [] }
)");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a logging rule referencing an unknown sink") {
    auto path = writeTempYaml(R"(
plc: { ip: "1.2.3.4" }
tags:
  - { id: t1, db: 1, type: int, offset: 0, update_interval_ms: 100 }
panels: []
logging:
  sinks: []
  rules:
    - tags: [t1]
      interval_ms: 500
      sinks: [does_not_exist]
)");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a non-vertical panel layout") {
    auto path = writeTempYaml(R"(
plc: { ip: "1.2.3.4" }
tags: []
panels:
  - title: "P"
    layout: horizontal
    widgets: []
logging: { sinks: [], rules: [] }
)");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

namespace {
const char* kDeviceBlueprintConfig = R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - id: in
        type: bool
        offset: 0
        bit: 0
        label: "In"
        writable: true
        widget: toggle
        group: "Inputs"
      - id: timeout
        type: time
        offset: 2
        label: "Timeout (ms)"
        writable: true
        widget: input_number
        group: "Inputs"
      - id: finished
        type: bool
        offset: 6
        bit: 0
        label: "Finished"
        widget: led_indicator
        group: "Outputs"
      - id: ready
        type: bool
        offset: 6
        bit: 1
        label: "Ready"
        widget: led_indicator
        group: "Outputs"
devices:
  - type: camera_inspection
    id: cam1
    db: 1
    label: "Camera 1"
  - type: camera_inspection
    id: cam2
    db: 3
    label: "Camera 2"
logging:
  sinks: []
  rules:
    - devices: [cam1, cam2]
      interval_ms: 1000
      sinks: []
)YAML";
} // namespace

TEST_CASE("ConfigLoader expands device_types + devices into tags with correct fields per instance") {
    auto path = writeTempYaml(kDeviceBlueprintConfig);
    AppConfig cfg = ConfigLoader::load(path);

    REQUIRE(cfg.tags.size() == 8); // 4 fields x 2 devices
    auto find = [&](const std::string& id) -> const TagConfig* {
        for (const auto& t : cfg.tags) if (t.id == id) return &t;
        return nullptr;
    };

    const TagConfig* cam1In = find("cam1_in");
    REQUIRE(cam1In != nullptr);
    CHECK(cam1In->db == 1);
    CHECK(cam1In->type == PlcType::Bool);
    CHECK(cam1In->offset == 0);
    CHECK(cam1In->bit == 0);
    CHECK(cam1In->label == "In");
    CHECK(cam1In->device == "Camera 1");
    CHECK(cam1In->updateIntervalMs == 200);

    const TagConfig* cam2Timeout = find("cam2_timeout");
    REQUIRE(cam2Timeout != nullptr);
    CHECK(cam2Timeout->db == 3);
    CHECK(cam2Timeout->type == PlcType::Time);
    CHECK(cam2Timeout->offset == 2);
    CHECK(cam2Timeout->device == "Camera 2");

    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader generates one panel per device with fields grouped by their blueprint group") {
    auto path = writeTempYaml(kDeviceBlueprintConfig);
    AppConfig cfg = ConfigLoader::load(path);

    REQUIRE(cfg.panels.size() == 2);
    CHECK(cfg.panels[0].title == "Camera 1");
    CHECK(cfg.panels[1].title == "Camera 2");

    REQUIRE(cfg.panels[0].groups.size() == 2);
    CHECK(cfg.panels[0].groups[0].title == "Inputs");
    REQUIRE(cfg.panels[0].groups[0].widgets.size() == 2);
    CHECK(cfg.panels[0].groups[0].widgets[0].type == "toggle");
    CHECK(cfg.panels[0].groups[0].widgets[0].tag == "cam1_in");
    CHECK(cfg.panels[0].groups[1].title == "Outputs");
    REQUIRE(cfg.panels[0].groups[1].widgets.size() == 2);
    CHECK(cfg.panels[0].groups[1].widgets[0].type == "led_indicator");
    CHECK(cfg.panels[0].groups[1].widgets[0].tag == "cam1_finished");

    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader expands a logging rule's devices list into every field of every listed device") {
    auto path = writeTempYaml(kDeviceBlueprintConfig);
    AppConfig cfg = ConfigLoader::load(path);

    REQUIRE(cfg.logging.rules.size() == 1);
    const auto& tags = cfg.logging.rules[0].tags;
    CHECK(tags.size() == 8);
    CHECK(std::find(tags.begin(), tags.end(), "cam1_in") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam1_timeout") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam1_finished") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam1_ready") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam2_in") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam2_timeout") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam2_finished") != tags.end());
    CHECK(std::find(tags.begin(), tags.end(), "cam2_ready") != tags.end());

    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a device instance referencing an unknown device type") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
devices:
  - type: does_not_exist
    id: cam1
    db: 1
    label: "Camera 1"
logging: { sinks: [], rules: [] }
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects two devices sharing the same id") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - { id: in, type: bool, offset: 0, bit: 0, label: "In", widget: led_indicator, group: "Outputs" }
devices:
  - { type: camera_inspection, id: cam1, db: 1, label: "Camera 1" }
  - { type: camera_inspection, id: cam1, db: 3, label: "Camera 2 (oops, forgot to rename id)" }
logging: { sinks: [], rules: [] }
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a logging rule's devices list referencing an unknown device id") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - { id: in, type: bool, offset: 0, bit: 0, label: "In", widget: led_indicator, group: "Outputs" }
devices:
  - { type: camera_inspection, id: cam1, db: 1, label: "Camera 1" }
logging:
  sinks: []
  rules:
    - devices: [does_not_exist]
      interval_ms: 1000
      sinks: []
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a writable:true field using a read-only widget") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - { id: in, type: bool, offset: 0, bit: 0, label: "In", writable: true, widget: led_indicator, group: "Inputs" }
devices: []
logging: { sinks: [], rules: [] }
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader rejects a writable widget without writable:true") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags: []
panels: []
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - { id: in, type: bool, offset: 0, bit: 0, label: "In", widget: toggle, group: "Inputs" }
devices: []
logging: { sinks: [], rules: [] }
)YAML");
    CHECK_THROWS_AS(ConfigLoader::load(path), ConfigError);
    std::remove(path.c_str());
}

TEST_CASE("ConfigLoader lets device blueprints coexist with directly-specified tags and panels") {
    auto path = writeTempYaml(R"YAML(
plc: { ip: "1.2.3.4" }
tags:
  - { id: manual_tag, db: 9, type: int, offset: 0, update_interval_ms: 500 }
panels:
  - title: "Manual Panel"
    layout: vertical
    widgets:
      - { type: text_display, tag: manual_tag }
device_types:
  camera_inspection:
    update_interval_ms: 200
    fields:
      - { id: in, type: bool, offset: 0, bit: 0, label: "In", widget: led_indicator, group: "Inputs" }
devices:
  - { type: camera_inspection, id: cam1, db: 1, label: "Camera 1" }
logging: { sinks: [], rules: [] }
)YAML");
    AppConfig cfg = ConfigLoader::load(path);

    REQUIRE(cfg.tags.size() == 2);
    REQUIRE(cfg.panels.size() == 2);
    CHECK(cfg.panels[0].title == "Manual Panel");
    CHECK(cfg.panels[1].title == "Camera 1");

    std::remove(path.c_str());
}
