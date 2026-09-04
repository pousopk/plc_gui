#pragma once
#include "config/config_types.hpp"
#include "plc/plc_io_thread.hpp"
#include "plc/value_cache.hpp"
#include <string>

class IWidget {
public:
    virtual ~IWidget() = default;
    virtual void render(const ValueMap& snapshot, PlcIoThread& io) = 0;
    virtual const std::string& tagId() const = 0;
};

class WidgetBase : public IWidget {
public:
    explicit WidgetBase(std::string tagId) : tagId_(std::move(tagId)) {}
    const std::string& tagId() const override { return tagId_; }
protected:
    std::string tagId_;
};

inline std::string getParam(const WidgetConfig& cfg, const std::string& key, const std::string& def) {
    auto it = cfg.params.find(key);
    return it != cfg.params.end() ? it->second : def;
}
