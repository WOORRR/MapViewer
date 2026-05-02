#include "core/MainModule.h"

#include <utility>

namespace mv {

MainModule::MainModule() = default;

MainModule::~MainModule() {
    if (started_) {
        shutdown("destructor");
    }
}

void MainModule::register_module(std::shared_ptr<ModuleBase> mod, bool main_thread) {
    mods_.push_back({std::move(mod), main_thread});
}

void MainModule::init(const std::string& config_path) {
    for (auto& slot : mods_) {
        slot.mod->attach(&bus_);
    }
    BootstrapMsg msg;
    msg.config_path = config_path;
    bus_.publish(msg);
}

void MainModule::start() {
    started_ = true;
    for (auto& slot : mods_) {
        if (!slot.main_thread) {
            slot.mod->start();
        }
    }
}

void MainModule::pump_main_thread(std::size_t max_per_module) {
    for (auto& slot : mods_) {
        if (slot.main_thread) {
            slot.mod->drain_all(max_per_module);
        }
    }
}

void MainModule::shutdown(const std::string& reason) {
    if (!started_) {
        return;
    }
    started_ = false;

    ShutdownMsg msg;
    msg.reason = reason;
    bus_.publish(msg);

    for (auto it = mods_.rbegin(); it != mods_.rend(); ++it) {
        if (!it->main_thread) {
            it->mod->stop();
        } else {
            it->mod->drain_all();  // flush any tail messages
        }
    }
}

}  // namespace mv
