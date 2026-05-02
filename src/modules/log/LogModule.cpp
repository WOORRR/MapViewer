#include "modules/log/LogModule.h"

#include "core/SessionId.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace mv::modules {

LogModule::LogModule() : ModuleBase("log") {}
LogModule::~LogModule() = default;

void LogModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<ShutdownMsg>(this);
    bus_->subscribe<LogEventMsg>(this);
    bus_->subscribe<CameraCollisionMsg>(this);
    bus_->subscribe<MapTileLoadedMsg>(this);
    bus_->subscribe<PickResultMsg>(this);
    bus_->subscribe<ObjectAddedMsg>(this);
    bus_->subscribe<StateAppliedMsg>(this);
    bus_->subscribe<UiCommandMsg>(this);
    bus_->subscribe<TtsRecognizedMsg>(this);
    open_sink();
}

void LogModule::open_sink() {
    namespace fs = std::filesystem;
    const char* la = std::getenv("LOCALAPPDATA");
    fs::path base = (la != nullptr) ? fs::path{la} / "MapViewer" / "logs"
                                     : fs::path{"./logs"};
    std::error_code ec;
    fs::create_directories(base, ec);
    sink_path_ = (base / ("session-" + std::to_string(IdGen::session()) + ".jsonl")).string();
    sink_.open(sink_path_, std::ios::out | std::ios::app);
    std::fprintf(stderr, "[log] writing JSONL to %s\n", sink_path_.c_str());
}

void LogModule::write_record(const std::string& kind, std::uint64_t event_id,
                               std::uint64_t parent_id, const std::string& payload_json) {
    nlohmann::json j;
    j["ts_ns"]      = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count();
    j["session_id"] = IdGen::session();
    j["event_id"]   = event_id;
    j["parent_id"]  = parent_id;
    j["kind"]       = kind;
    if (!payload_json.empty()) {
        try { j["payload"] = nlohmann::json::parse(payload_json); }
        catch (...) { j["payload"] = payload_json; }
    }
    std::lock_guard<std::mutex> lk(mtx_);
    if (sink_.is_open()) {
        sink_ << j.dump() << '\n';
        sink_.flush();
    }
}

void LogModule::on_message(const AnyMessage& msg) {
    const auto& hdr = header_of(msg);

    if (auto* b = std::get_if<BootstrapMsg>(&msg)) {
        nlohmann::json p; p["config_path"] = b->config_path;
        write_record("bootstrap", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* s = std::get_if<ShutdownMsg>(&msg)) {
        nlohmann::json p; p["reason"] = s->reason;
        write_record("shutdown", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* l = std::get_if<LogEventMsg>(&msg)) {
        nlohmann::json p;
        p["level"] = l->level; p["scope"] = l->scope;
        p["kv"]    = l->kv_json;
        write_record("log", hdr.event_id, hdr.parent_event_id, p.dump());
        if (l->level >= 2) {
            std::fprintf(stderr, "[%s] %s\n", l->scope.c_str(), l->kv_json.c_str());
        }
    } else if (auto* c = std::get_if<CameraCollisionMsg>(&msg)) {
        nlohmann::json p;
        p["with"] = (c->with == CameraCollisionMsg::With::Ground) ? "ground" : "building";
        p["building_id"] = c->building_id;
        p["pos"] = {c->corrected_x, c->corrected_y, c->corrected_z};
        write_record("collision", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* t = std::get_if<MapTileLoadedMsg>(&msg)) {
        nlohmann::json p;
        p["bounds"]      = {t->bounds_minx, t->bounds_miny, t->bounds_maxx, t->bounds_maxy};
        p["building_n"]  = t->buildings.size();
        p["road_n"]      = t->roads.size();
        p["version"]     = t->version;
        write_record("map_tile", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* pr = std::get_if<PickResultMsg>(&msg)) {
        nlohmann::json p;
        p["kind"] = (pr->kind == PickResultMsg::Kind::Building) ? "building"
                  : (pr->kind == PickResultMsg::Kind::Road)     ? "road" : "none";
        p["id"]   = pr->id;
        write_record("pick", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* oa = std::get_if<ObjectAddedMsg>(&msg)) {
        nlohmann::json p;
        p["instance_id"] = oa->instance_id;
        p["mesh_ref"]    = oa->mesh_ref;
        p["world"]       = {oa->world_x, oa->world_y, oa->world_z};
        write_record("object_added", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* sa = std::get_if<StateAppliedMsg>(&msg)) {
        nlohmann::json p;
        p["snapshot_id"] = sa->snapshot_id;
        p["kind"]        = sa->applied_command_kind;
        write_record("state_applied", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* ui = std::get_if<UiCommandMsg>(&msg)) {
        nlohmann::json p; p["cli"] = ui->cli_text;
        write_record("ui_command", hdr.event_id, hdr.parent_event_id, p.dump());
    } else if (auto* tts = std::get_if<TtsRecognizedMsg>(&msg)) {
        nlohmann::json p; p["text"] = tts->cli_text; p["confidence"] = tts->confidence;
        write_record("tts", hdr.event_id, hdr.parent_event_id, p.dump());
    }
}

}  // namespace mv::modules
