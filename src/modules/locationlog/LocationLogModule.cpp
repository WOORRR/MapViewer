#include "modules/locationlog/LocationLogModule.h"

#include "geo/CoordTransform.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace mv::modules {

LocationLogModule::LocationLogModule() : ModuleBase("locationlog") {}

LocationLogModule::~LocationLogModule() {
    stop_replay();
}

void LocationLogModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<ConfigUpdatedMsg>(this);
    bus_->subscribe<ShutdownMsg>(this);
}

void LocationLogModule::on_message(const AnyMessage& msg) {
    if (std::get_if<BootstrapMsg>(&msg)) {
        start_replay(csv_path_, enabled_.load());
        return;
    }
    if (auto* cfg = std::get_if<ConfigUpdatedMsg>(&msg)) {
        if (cfg->key != "location_log") return;
        try {
            const auto j = nlohmann::json::parse(cfg->json_value);
            if (j.contains("enabled"))   enabled_   = j.at("enabled").get<bool>();
            if (j.contains("csv_path"))  csv_path_  = j.at("csv_path").get<std::string>();
        } catch (...) {}
        stop_replay();
        start_replay(csv_path_, enabled_.load());
    }
    if (std::get_if<ShutdownMsg>(&msg)) {
        stop_replay();
    }
}

void LocationLogModule::start_replay(std::string csv_path, bool enabled) {
    if (!enabled) return;
    namespace fs = std::filesystem;
    if (!fs::exists(csv_path)) {
        fs::create_directories(fs::path(csv_path).parent_path());
        generate_sample_csv(csv_path);
    }
    auto samples = read_csv(csv_path);
    if (samples.empty()) return;
    replay_running_ = true;
    replay_thread_ = std::thread(&LocationLogModule::replay_loop, this, std::move(samples));
}

void LocationLogModule::stop_replay() {
    if (replay_running_.exchange(false)) {
        if (replay_thread_.joinable()) replay_thread_.join();
    }
}

void LocationLogModule::replay_loop(std::vector<Sample> samples) {
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    geo::CoordTransform xf;

    for (const auto& s : samples) {
        if (!replay_running_.load()) return;
        const auto target = t0 + std::chrono::milliseconds(static_cast<int>(s.offset_s * 1000.0));
        std::this_thread::sleep_until(target);
        if (!replay_running_.load()) return;

        const auto utmk = xf.to_utmk({s.lat_deg, s.lon_deg});
        LocationFixMsg msg;
        msg.timestamp_ns   = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count();
        msg.utmk_x         = utmk.x;
        msg.utmk_y         = utmk.y;
        msg.alt_m          = s.alt_m;
        msg.speed_mps      = s.speed_mps;
        msg.bearing_deg    = s.bearing_deg;
        msg.accuracy_m     = s.accuracy_m;
        msg.alt_accuracy_m = s.alt_accuracy_m;
        bus_->publish(msg);
    }
}

std::vector<LocationLogModule::Sample> LocationLogModule::read_csv(const std::string& path) {
    std::vector<Sample> out;
    std::ifstream f(path);
    if (!f) return out;
    std::string line;
    if (!std::getline(f, line)) return out;  // header

    std::tm t0{}; bool first = true;
    auto parse_iso = [](const std::string& s, std::tm& out_tm) -> bool {
        std::istringstream iss(s);
        iss >> std::get_time(&out_tm, "%Y-%m-%dT%H:%M:%S");
        return !iss.fail();
    };

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::vector<std::string> tok;
        std::string cur;
        for (char c : line) {
            if (c == ',') { tok.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        tok.push_back(cur);
        if (tok.size() < 10) continue;

        std::tm tm{};
        if (!parse_iso(tok[0], tm)) continue;
        if (first) { t0 = tm; first = false; }

        const std::time_t t  = std::mktime(&tm);
        const std::time_t t_0 = std::mktime(&t0);

        Sample s;
        s.offset_s        = static_cast<double>(t - t_0);
        s.lat_deg         = std::stod(tok[1]);
        s.lon_deg         = std::stod(tok[2]);
        s.accuracy_m      = std::stod(tok[3]);
        s.alt_m           = std::stod(tok[4]);
        s.alt_accuracy_m  = std::stod(tok[5]);
        s.speed_mps       = std::stod(tok[6]);
        s.bearing_deg     = std::stod(tok[8]);
        out.push_back(s);
    }
    return out;
}

void LocationLogModule::generate_sample_csv(const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        std::fprintf(stderr, "[locationlog] failed to open %s for write\n", path.c_str());
        return;
    }
    f << "datetime,latitude,longitude,accuracy_m,altitude_m,altitude_accuracy_m,"
         "speed_mps,speed_accuracy_mps,bearing_deg,bearing_accuracy_deg\n";

    // Linear walk from City Hall (37.5665, 126.9780) to Gwanghwamun (37.5759,
    // 126.9769) over 300 seconds at 1 Hz. Bearing approximated as ~355°.
    const double lat0 = 37.5665, lon0 = 126.9780;
    const double lat1 = 37.5759, lon1 = 126.9769;
    constexpr int total = 300;
    std::time_t base_time = std::time(nullptr);
    std::tm base_tm{};
#ifdef _WIN32
    gmtime_s(&base_tm, &base_time);
#else
    base_tm = *std::gmtime(&base_time);
#endif
    char buf[64];
    for (int i = 0; i <= total; ++i) {
        const double tnorm = static_cast<double>(i) / total;
        const double lat = lat0 + (lat1 - lat0) * tnorm;
        const double lon = lon0 + (lon1 - lon0) * tnorm;
        std::tm tm = base_tm;
        tm.tm_sec += i;
        std::time_t tt = _mkgmtime(&tm);  // Windows-specific
        std::tm now_tm{};
#ifdef _WIN32
        gmtime_s(&now_tm, &tt);
#else
        now_tm = *std::gmtime(&tt);
#endif
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &now_tm);
        f << buf << ',' << lat << ',' << lon
          << ",5.0,30.0,8.0,1.3,0.5,355.0,4.0\n";
    }
}

}  // namespace mv::modules
