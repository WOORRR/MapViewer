#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

namespace mv::modules {

// Reads `assets/config/settings.json` on Bootstrap and publishes one
// ConfigUpdatedMsg per top-level key (the value carries the entire sub-tree
// as a JSON string). Subscribers parse what they need.
class ConfigModule : public ModuleBase {
public:
    ConfigModule();
    ~ConfigModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void load_and_publish(const std::string& path);
};

}  // namespace mv::modules
