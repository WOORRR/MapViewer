#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

namespace mv::modules {

// Loads map data and publishes MapTileLoadedMsg. The prototype ships with a
// hard-coded sample tile around Seoul City Hall (UTM-K). The PostgreSQL/
// PostGIS backend will be wired in once we confirm the live schema; the
// modular boundary is already in place so the producer can be swapped without
// touching the rest of the bus.
class DataModule : public ModuleBase {
public:
    DataModule();
    ~DataModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void publish_sample_tile();
};

}  // namespace mv::modules
