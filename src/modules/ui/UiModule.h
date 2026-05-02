#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace mv::modules {

// State container the render module reads each frame to draw the ImGui
// overlay. Separating data from rendering keeps GL strictly on the main
// thread while still letting the UI module run as a regular worker for the
// CLI/parsing work.
class UiModule : public ModuleBase {
public:
    struct ToastEntry {
        std::string text;
        std::chrono::steady_clock::time_point expires_at;
    };

    struct PickInfo {
        std::string kind;   // "building" / "road" / ""
        std::string id;
        std::string props_json;
    };

    UiModule();
    ~UiModule() override;

    // Snapshot accessors for the render module (main-thread reader).
    std::deque<ToastEntry> active_toasts();
    PickInfo               current_pick();

    // Called by the render module when the user submits a CLI line.
    void submit_cli(std::string line);

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void push_toast(std::string text, std::chrono::seconds ttl);

    std::mutex mtx_;
    std::deque<ToastEntry> toasts_;
    PickInfo pick_;
};

}  // namespace mv::modules
