#pragma once
#include "config/config_types.hpp"
#include "plc/plc_io_thread.hpp"
#include "plc/value_cache.hpp"
#include <string>

// Base class of every on-screen control. Each widget is bound to a single
// tag. render() is called once per frame on the UI thread with the latest
// value snapshot; writable widgets send changes to the PLC through
// io.requestWrite().
class Widget {
public:
    explicit Widget(std::string tagId) : tagId_(std::move(tagId)) {}
    virtual ~Widget() = default;

    virtual void render(const ValueMap& snapshot, PlcIoThread& io) = 0;
    const std::string& tagId() const { return tagId_; }

protected:
    std::string tagId_;
};

// Returns the widget's YAML param `key`, or `fallback` if it isn't set.
// Widgets use this for their caption as getParam(cfg, "label",
// tagDisplayLabel(tag)): a widget's own `label` wins over the tag's.
inline std::string getParam(const WidgetConfig& cfg, const std::string& key, const std::string& fallback) {
    auto it = cfg.params.find(key);
    return it != cfg.params.end() ? it->second : fallback;
}
