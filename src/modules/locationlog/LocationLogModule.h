#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace mv::modules {

// Plays back a recorded GPS trail from CSV, publishing one LocationFixMsg per
// row at the wall-clock cadence implied by the file's `datetime` column. If
// the file is missing on Bootstrap we synthesise a 5-minute walk between
// Seoul City Hall and Gwanghwamun so the prototype always has something to
// visualise.
class LocationLogModule : public ModuleBase {
public:
    LocationLogModule();
    ~LocationLogModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    struct Sample {
        double offset_s{0.0};   // seconds since the first sample
        double lat_deg{0.0};
        double lon_deg{0.0};
        double alt_m{0.0};
        double speed_mps{0.0};
        double bearing_deg{0.0};
        double accuracy_m{5.0};
        double alt_accuracy_m{8.0};
    };

    void start_replay(std::string csv_path, bool enabled);
    void stop_replay();
    static std::vector<Sample> read_csv(const std::string& path);
    static void                generate_sample_csv(const std::string& path);
    void                       replay_loop(std::vector<Sample> samples);

    std::atomic<bool> replay_running_{false};
    std::thread replay_thread_;
    std::atomic<bool> enabled_{true};
    std::string csv_path_{"assets/samples/location_log_sample.csv"};
};

}  // namespace mv::modules
