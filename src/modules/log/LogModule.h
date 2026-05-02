#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <fstream>
#include <mutex>
#include <string>

namespace mv::modules {

// Lightweight OTel-shaped logger. Until we wire up the OTLP exporter we just
// emit JSONL records to %LOCALAPPDATA%/MapViewer/logs/session-<sid>.jsonl and
// echo info+ to stderr. Every message that flows through subscribed types
// produces one record so the bus order is recoverable from disk.
class LogModule : public ModuleBase {
public:
    LogModule();
    ~LogModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void open_sink();
    void write_record(const std::string& kind, std::uint64_t event_id,
                       std::uint64_t parent_id, const std::string& payload_json);

    std::mutex mtx_;
    std::ofstream sink_;
    std::string sink_path_;
};

}  // namespace mv::modules
