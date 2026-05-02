#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <memory>
#include <string>
#include <vector>

namespace mv {

// Owns the bus and the registered sub-modules. The render module (or any
// module that must run on the main thread) is registered with `main_thread =
// true`, in which case start() will not spawn a worker — the host has to pump
// it via drain_all() each frame.
class MainModule {
public:
    MainModule();
    ~MainModule();

    MessageBus& bus() { return bus_; }

    void register_module(std::shared_ptr<ModuleBase> mod, bool main_thread = false);

    // Initializes all modules (calls attach() in registration order).
    void init(const std::string& config_path);

    // Starts worker threads for non-main-thread modules and broadcasts a
    // BootstrapMsg.
    void start();

    // Pumps every main-thread module's queue. Call once per frame.
    void pump_main_thread(std::size_t max_per_module = 256);

    // Sends ShutdownMsg, joins worker threads in reverse registration order.
    void shutdown(const std::string& reason);

private:
    MessageBus bus_;
    struct Slot {
        std::shared_ptr<ModuleBase> mod;
        bool main_thread{false};
    };
    std::vector<Slot> mods_;
    bool started_{false};
};

}  // namespace mv
