#include "modules/object/ObjectModule.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace mv::modules {

ObjectModule::ObjectModule() : ModuleBase("object") {}
ObjectModule::~ObjectModule() = default;

void ObjectModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<ObjectSpawnRequestMsg>(this);
    load_palette();
}

void ObjectModule::on_message(const AnyMessage& msg) {
    if (std::get_if<BootstrapMsg>(&msg)) {
        load_palette();
        return;
    }
    if (auto* req = std::get_if<ObjectSpawnRequestMsg>(&msg)) {
        ObjectAddedMsg added;
        added.instance_id = "obj-" + std::to_string(req->header.event_id);
        added.mesh_ref    = req->obj_path.empty() ? "balloon" : req->obj_path;
        // Default placement: 100 m above the requested hint, with a small
        // random horizontal offset so multiple spawns are visible.
        const auto entry = palette("balloon");
        const double jitter_x = (static_cast<int>(req->header.event_id) % 7) * 12.0 - 36.0;
        const double jitter_y = (static_cast<int>(req->header.event_id) % 11) * 8.0 - 40.0;
        added.world_x = (req->hint_x != 0.0 ? req->hint_x : 953900.0) + jitter_x;
        added.world_y = (req->hint_y != 0.0 ? req->hint_y : 1952030.0) + jitter_y;
        added.world_z = (req->hint_z != 0.0 ? req->hint_z : 130.0);
        (void)entry;
        bus_->publish(added);
    }
}

void ObjectModule::load_palette() {
    constexpr const char* kPath = "assets/config/object_palette.json";
    namespace fs = std::filesystem;
    if (!fs::exists(kPath)) {
        return;
    }
    try {
        std::ifstream f(kPath);
        nlohmann::json j;
        f >> j;
        std::lock_guard<std::mutex> lk(mtx_);
        palette_.clear();
        for (auto it = j.begin(); it != j.end(); ++it) {
            PaletteEntry e;
            e.name = it.key();
            if (it->contains("color") && it->at("color").is_array()) {
                for (std::size_t i = 0; i < 4 && i < it->at("color").size(); ++i) {
                    e.color[i] = it->at("color").at(i).get<float>();
                }
            }
            if (it->contains("scale")) {
                e.scale = it->at("scale").get<float>();
            }
            palette_[e.name] = std::move(e);
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[object] palette parse failed: %s\n", ex.what());
    }
}

ObjectModule::PaletteEntry ObjectModule::palette(const std::string& name) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (auto it = palette_.find(name); it != palette_.end()) {
        return it->second;
    }
    return PaletteEntry{name, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f};
}

}  // namespace mv::modules
