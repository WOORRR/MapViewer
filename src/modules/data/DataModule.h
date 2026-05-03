#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <atomic>
#include <string>

namespace mv::modules {

// Loads map data from PostgreSQL/PostGIS (wrr-postgres) and publishes
// MapTileLoadedMsg.  Falls back to a hard-coded sample tile when the DB is
// unreachable.  The spatial query uses the geom column (EPSG:5179) with a
// GIST index, so the initial ST_GeomFromText population must complete first.
class DataModule : public ModuleBase {
public:
    DataModule();
    ~DataModule() override;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    // Try to connect; returns true on success.
    bool try_connect();

    // Query buildings and roads inside the bbox and publish a tile.
    void query_and_publish(double cx, double cy, double half);

    // Fallback when DB is unavailable.
    void publish_fallback_tile(double cx, double cy);

    uint32_t cell_hash(int gx, int gy) const;

    bool        db_ok_{false};
    uint32_t    version_{0};
    std::string conn_str_;
};

}  // namespace mv::modules
