#include "modules/state/StateModule.h"

#include <cmath>

namespace mv::modules {

StateModule::StateModule() : ModuleBase("state") {}
StateModule::~StateModule() = default;

void StateModule::on_init() {
    bus_->subscribe<CameraMovedMsg>(this);
    bus_->subscribe<CameraFovChangedMsg>(this);
    bus_->subscribe<ObjectAddedMsg>(this);
    bus_->subscribe<UndoRequestMsg>(this);
    bus_->subscribe<RedoRequestMsg>(this);
}

void StateModule::on_message(const AnyMessage& msg) {
    // parent_event_id != 0 indicates a replay/restore — don't record.
    const auto& hdr = header_of(msg);
    const bool is_restore = hdr.parent_event_id != 0;

    if (auto* m = std::get_if<CameraMovedMsg>(&msg)) {
        if (!is_restore) {
            record_camera(m->pos_x, m->pos_y, m->pos_z, m->yaw_deg, m->pitch_deg);
        } else {
            last_cam_ = {m->pos_x, m->pos_y, m->pos_z, m->yaw_deg, m->pitch_deg};
            have_cam_baseline_ = true;
        }
    } else if (auto* f = std::get_if<CameraFovChangedMsg>(&msg)) {
        if (!is_restore) {
            record_fov(f->fov_deg);
        } else {
            last_fov_ = {f->fov_deg};
            have_fov_baseline_ = true;
        }
    } else if (auto* oa = std::get_if<ObjectAddedMsg>(&msg)) {
        if (!is_restore) {
            record_object(oa->instance_id, oa->mesh_ref, oa->world_x, oa->world_y, oa->world_z);
        }
    } else if (std::get_if<UndoRequestMsg>(&msg)) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (undo_.empty()) return;
        Command cmd = undo_.back(); undo_.pop_back();
        redo_.push_back(cmd);
        apply_command(cmd, "undo");
    } else if (std::get_if<RedoRequestMsg>(&msg)) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (redo_.empty()) return;
        Command cmd = redo_.back(); redo_.pop_back();
        undo_.push_back(cmd);
        apply_command(cmd, "redo");
    }
}

void StateModule::record_camera(double x, double y, double z, double yaw, double pitch) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (have_cam_baseline_) {
        // Push the *previous* snapshot so undo restores the prior pose.
        undo_.push_back(last_cam_);
        if (undo_.size() > 256) undo_.pop_front();
        redo_.clear();
    }
    last_cam_ = {x, y, z, yaw, pitch};
    have_cam_baseline_ = true;
}

void StateModule::record_fov(int fov) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (have_fov_baseline_ && last_fov_.fov_deg != fov) {
        undo_.push_back(last_fov_);
        if (undo_.size() > 256) undo_.pop_front();
        redo_.clear();
    }
    last_fov_ = {fov};
    have_fov_baseline_ = true;
}

void StateModule::record_object(const std::string& id, const std::string& mesh,
                                  double x, double y, double z) {
    std::lock_guard<std::mutex> lk(mtx_);
    undo_.push_back(ObjectAddRecord{id, mesh, x, y, z});
    if (undo_.size() > 256) undo_.pop_front();
    redo_.clear();
}

void StateModule::apply_command(const Command& cmd, const std::string& kind) {
    if (auto* c = std::get_if<CamSnapshot>(&cmd)) {
        CameraMovedMsg out;
        out.header.parent_event_id = ++snapshot_id_;
        out.pos_x   = c->pos_x; out.pos_y = c->pos_y; out.pos_z = c->pos_z;
        out.yaw_deg = c->yaw_deg; out.pitch_deg = c->pitch_deg;
        bus_->publish(out);
        notify_applied(std::string{"camera_"} + kind);
    } else if (auto* f = std::get_if<FovSnapshot>(&cmd)) {
        CameraFovChangedMsg out;
        out.header.parent_event_id = ++snapshot_id_;
        out.fov_deg = f->fov_deg;
        bus_->publish(out);
        notify_applied(std::string{"fov_"} + kind);
    } else if (auto* o = std::get_if<ObjectAddRecord>(&cmd)) {
        // For a prototype undo we just notify; renderer will treat as a soft
        // remove via id matching (the StateAppliedMsg carries the kind so the
        // renderer knows to call remove_balloon).
        StateAppliedMsg n;
        n.snapshot_id = ++snapshot_id_;
        n.applied_command_kind = (kind == "undo")
            ? std::string{"object_undo:"} + o->instance_id
            : std::string{"object_redo:"} + o->instance_id;
        bus_->publish(n);
        return;
    }
}

void StateModule::notify_applied(const std::string& kind) {
    StateAppliedMsg n;
    n.snapshot_id = snapshot_id_;
    n.applied_command_kind = kind;
    bus_->publish(n);
}

}  // namespace mv::modules
