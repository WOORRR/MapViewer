#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <deque>
#include <mutex>
#include <variant>

namespace mv::modules {

// Owns the undo/redo stacks. Listens for state-changing events and stores a
// minimal command record per change. Re-applying or rolling back republishes
// the canonical message with parent_event_id != 0 — modules treat this as a
// "do not record" hint to avoid infinite loops.
class StateModule : public ModuleBase {
public:
    StateModule();
    ~StateModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    struct CamSnapshot {
        double pos_x{0.0}, pos_y{0.0}, pos_z{0.0};
        double yaw_deg{0.0}, pitch_deg{0.0};
    };
    struct FovSnapshot {
        int fov_deg{0};
    };
    struct ObjectAddRecord {
        std::string instance_id;
        std::string mesh_ref;
        double x{0.0}, y{0.0}, z{0.0};
    };
    using Command = std::variant<CamSnapshot, FovSnapshot, ObjectAddRecord>;

    void   record_camera(double x, double y, double z, double yaw, double pitch);
    void   record_fov(int fov);
    void   record_object(const std::string& id, const std::string& mesh,
                          double x, double y, double z);
    void   apply_command(const Command& cmd, const std::string& kind);
    void   notify_applied(const std::string& kind);

    std::mutex mtx_;
    std::deque<Command> undo_;
    std::deque<Command> redo_;
    CamSnapshot   last_cam_{};
    FovSnapshot   last_fov_{};
    bool          have_cam_baseline_{false};
    bool          have_fov_baseline_{false};
    std::uint64_t snapshot_id_{0};
};

}  // namespace mv::modules
