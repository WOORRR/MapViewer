#include "modules/config/ConfigModule.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace mv::modules {

ConfigModule::ConfigModule() : ModuleBase("config") {}
ConfigModule::~ConfigModule() = default;

void ConfigModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
}

void ConfigModule::on_message(const AnyMessage& msg) {
    if (auto* b = std::get_if<BootstrapMsg>(&msg)) {
        const std::string p = b->config_path.empty()
            ? std::string{"assets/config/settings.json"}
            : b->config_path;
        load_and_publish(p);
    }
}

void ConfigModule::load_and_publish(const std::string& path) {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) {
        std::fprintf(stderr, "[config] missing %s — using empty config\n", path.c_str());
        return;
    }
    try {
        std::ifstream f(path);
        nlohmann::json j;
        f >> j;
        for (auto it = j.begin(); it != j.end(); ++it) {
            ConfigUpdatedMsg m;
            m.key        = it.key();
            m.json_value = it.value().dump();
            bus_->publish(m);
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[config] parse failed (%s): %s\n", path.c_str(), ex.what());
    }
}

}  // namespace mv::modules
