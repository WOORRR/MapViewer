#pragma once

// Catalog of all messages that flow on the bus. Sub-modules subscribe to the
// concrete struct they care about; visit AnyMessage when a generic dispatch is
// needed (e.g. for logging).

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace mv {

using EventId   = std::uint64_t;
using SessionId = std::uint64_t;

struct MessageHeader {
    EventId   event_id{0};
    SessionId session_id{0};
    EventId   parent_event_id{0};  // 0 means "no parent"
};

// ---------- lifecycle ----------
struct BootstrapMsg { MessageHeader header; std::string config_path; };
struct ShutdownMsg  { MessageHeader header; std::string reason; };

// ---------- config ----------
struct ConfigUpdatedMsg { MessageHeader header; std::string key; std::string json_value; };

// ---------- location ----------
struct LocationFixMsg {
    MessageHeader header;
    std::int64_t timestamp_ns{0};
    double utmk_x{0.0};
    double utmk_y{0.0};
    double alt_m{0.0};
    double speed_mps{0.0};
    double bearing_deg{0.0};
    double accuracy_m{0.0};
    double alt_accuracy_m{0.0};
};

// ---------- map data ----------
struct MapBoundsRequestMsg {
    MessageHeader header;
    double center_x{0.0};
    double center_y{0.0};
    double radius_m{500.0};
    int    lod{0};
};

struct PolygonRing {
    std::vector<std::pair<double, double>> points;  // outer ring + holes (we keep one ring per object for prototype)
};

struct BuildingFeature {
    std::string id;
    std::vector<PolygonRing> rings;
    int gro_floors{1};
    std::string addr_road;
    std::string addr_lot;
    std::string bdtyp_cd;   // 건물용도코드
    std::string buld_nm;
};

struct RoadFeature {
    std::string id;
    std::vector<std::pair<double, double>> line;
    std::string rn;          // 도로명
    std::string roa_cls_se;  // 도로위계구분
    double width_m{4.0};
};

struct MapTileLoadedMsg {
    MessageHeader header;
    double bounds_minx{0.0};
    double bounds_miny{0.0};
    double bounds_maxx{0.0};
    double bounds_maxy{0.0};
    std::vector<BuildingFeature> buildings;
    std::vector<RoadFeature> roads;
    std::uint64_t version{0};
};

// ---------- camera ----------
struct CameraMovedMsg {
    MessageHeader header;
    double pos_x{0.0};
    double pos_y{0.0};
    double pos_z{0.0};
    double yaw_deg{0.0};
    double pitch_deg{0.0};
    double roll_deg{0.0};
};

struct CameraFovChangedMsg {
    MessageHeader header;
    int fov_deg{0};  // clamped to [-60, +60] by InputModule
};

struct CameraCollisionMsg {
    MessageHeader header;
    enum class With { Ground, Building };
    With with{With::Ground};
    std::string building_id;
    double corrected_x{0.0};
    double corrected_y{0.0};
    double corrected_z{0.0};
};

// ---------- pick ----------
struct PickRequestMsg {
    MessageHeader header;
    int screen_x{0};
    int screen_y{0};
};

struct PickResultMsg {
    MessageHeader header;
    enum class Kind { None, Building, Road };
    Kind kind{Kind::None};
    std::string id;
    std::string props_json;
};

// ---------- objects ----------
struct ObjectSpawnRequestMsg {
    MessageHeader header;
    std::string obj_path;
    double hint_x{0.0};
    double hint_y{0.0};
    double hint_z{0.0};
};

struct ObjectAddedMsg {
    MessageHeader header;
    std::string instance_id;
    std::string mesh_ref;
    double world_x{0.0};
    double world_y{0.0};
    double world_z{0.0};
};

// ---------- state / undo ----------
struct UndoRequestMsg { MessageHeader header; };
struct RedoRequestMsg { MessageHeader header; };
struct StateAppliedMsg {
    MessageHeader header;
    std::uint64_t snapshot_id{0};
    std::string applied_command_kind;
};

// ---------- ui ----------
struct UiCommandMsg { MessageHeader header; std::string cli_text; };

// ---------- log ----------
struct LogEventMsg {
    MessageHeader header;
    int level{2};            // 0=trace 1=debug 2=info 3=warn 4=error
    std::string scope;
    std::string kv_json;     // key/value JSON object as string
};

// ---------- sound / tts ----------
struct SoundPlayMsg {
    MessageHeader header;
    std::string clip_id;
    int priority{0};
    enum class Mode { Play, Queue, Preempt };
    Mode mode{Mode::Play};
};

struct TtsRecognizedMsg {
    MessageHeader header;
    std::string cli_text;
    float confidence{0.0f};
};

using AnyMessage = std::variant<
    BootstrapMsg, ShutdownMsg,
    ConfigUpdatedMsg,
    LocationFixMsg,
    MapBoundsRequestMsg, MapTileLoadedMsg,
    CameraMovedMsg, CameraFovChangedMsg, CameraCollisionMsg,
    PickRequestMsg, PickResultMsg,
    ObjectSpawnRequestMsg, ObjectAddedMsg,
    UndoRequestMsg, RedoRequestMsg, StateAppliedMsg,
    UiCommandMsg,
    LogEventMsg,
    SoundPlayMsg, TtsRecognizedMsg
>;

inline const MessageHeader& header_of(const AnyMessage& m) {
    return std::visit([](const auto& x) -> const MessageHeader& { return x.header; }, m);
}

}  // namespace mv
