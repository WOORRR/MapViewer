#include "modules/input/InputModule.h"

#include "geo/CoordTransform.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace mv::modules {

InputModule::InputModule() : ModuleBase("input") {}
InputModule::~InputModule() = default;

void InputModule::on_init() {
    bus_->subscribe<UiCommandMsg>(this);
    bus_->subscribe<TtsRecognizedMsg>(this);
}

void InputModule::on_message(const AnyMessage& msg) {
    if (auto* ui = std::get_if<UiCommandMsg>(&msg)) {
        handle_cli(ui->cli_text);
    } else if (auto* tts = std::get_if<TtsRecognizedMsg>(&msg)) {
        handle_cli(tts->cli_text);
    }
}

namespace {
std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}
}  // namespace

void InputModule::handle_cli(const std::string& line) {
    const auto t = split_ws(line);
    if (t.empty()) return;

    try {
        if (t[0] == "goto" && t.size() >= 3) {
            const double lat = std::stod(t[1]);
            const double lon = std::stod(t[2]);
            mv::geo::CoordTransform xf;
            const auto xy = xf.to_utmk({lat, lon});
            CameraMovedMsg out;
            out.pos_x = xy.x; out.pos_y = xy.y; out.pos_z = 80.0;
            out.yaw_deg = 0.0; out.pitch_deg = -25.0; out.roll_deg = 0.0;
            bus_->publish(out);
            return;
        }
        if (t[0] == "teleport" && t.size() >= 3) {
            CameraMovedMsg out;
            out.pos_x = std::stod(t[1]);
            out.pos_y = std::stod(t[2]);
            out.pos_z = (t.size() >= 4) ? std::stod(t[3]) : 80.0;
            out.yaw_deg = 0.0; out.pitch_deg = -25.0; out.roll_deg = 0.0;
            bus_->publish(out);
            return;
        }
        if (t[0] == "fov" && t.size() >= 2) {
            CameraFovChangedMsg out;
            out.fov_deg = std::stoi(t[1]);
            bus_->publish(out);
            return;
        }
        if (t[0] == "load_obj" && t.size() >= 2) {
            ObjectSpawnRequestMsg out;
            out.obj_path = t[1];
            // Render module will substitute the actual sky placement.
            bus_->publish(out);
            return;
        }
        if (t[0] == "undo") { UndoRequestMsg out; bus_->publish(out); return; }
        if (t[0] == "redo") { RedoRequestMsg out; bus_->publish(out); return; }
        if (t[0] == "trail" && t.size() >= 2) {
            ConfigUpdatedMsg out;
            out.key = "location_log";
            out.json_value = nlohmann::json{{"enabled", t[1] == "on"}}.dump();
            bus_->publish(out);
            return;
        }
        std::fprintf(stderr, "[input] unknown command: %s\n", line.c_str());
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[input] parse error: %s (%s)\n", line.c_str(), ex.what());
    }
}

}  // namespace mv::modules
