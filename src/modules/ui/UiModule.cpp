#include "modules/ui/UiModule.h"

#include "geo/CoordTransform.h"

#include <cstdio>
#include <sstream>

namespace mv::modules {

using namespace std::chrono_literals;

UiModule::UiModule() : ModuleBase("ui") {}
UiModule::~UiModule() = default;

void UiModule::on_init() {
    bus_->subscribe<CameraCollisionMsg>(this);
    bus_->subscribe<PickResultMsg>(this);
    bus_->subscribe<ObjectAddedMsg>(this);
    bus_->subscribe<StateAppliedMsg>(this);
}

void UiModule::on_message(const AnyMessage& msg) {
    if (auto* col = std::get_if<CameraCollisionMsg>(&msg)) {
        std::string kind = (col->with == CameraCollisionMsg::With::Ground)
                             ? "땅" : "건물(" + col->building_id + ")";
        push_toast("[충돌 안내] " + kind + "에서 외부로 자동 이동", 3s);
    } else if (auto* pk = std::get_if<PickResultMsg>(&msg)) {
        std::lock_guard<std::mutex> lk(mtx_);
        pick_.kind = (pk->kind == PickResultMsg::Kind::Building) ? "building"
                   : (pk->kind == PickResultMsg::Kind::Road)     ? "road"
                                                                  : "";
        pick_.id = pk->id;
        pick_.props_json = pk->props_json;
    } else if (auto* oa = std::get_if<ObjectAddedMsg>(&msg)) {
        push_toast("오브젝트 생성: " + oa->mesh_ref, 3s);
    } else if (auto* sa = std::get_if<StateAppliedMsg>(&msg)) {
        push_toast("상태 적용: " + sa->applied_command_kind, 2s);
    }
}

std::deque<UiModule::ToastEntry> UiModule::active_toasts() {
    const auto now = std::chrono::steady_clock::now();
    std::deque<ToastEntry> live;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        while (!toasts_.empty() && toasts_.front().expires_at < now) {
            toasts_.pop_front();
        }
        live = toasts_;
    }
    return live;
}

UiModule::PickInfo UiModule::current_pick() {
    std::lock_guard<std::mutex> lk(mtx_);
    return pick_;
}

void UiModule::push_toast(std::string text, std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lk(mtx_);
    toasts_.push_back({std::move(text), std::chrono::steady_clock::now() + ttl});
    if (toasts_.size() > 8) {
        toasts_.pop_front();
    }
}

void UiModule::submit_cli(std::string line) {
    UiCommandMsg msg;
    msg.cli_text = std::move(line);
    bus_->publish(msg);
}

}  // namespace mv::modules
