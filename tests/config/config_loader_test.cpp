#include <doctest/doctest.h>
#include "config/config_loader.hpp"
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
