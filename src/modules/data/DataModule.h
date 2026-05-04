#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"

#include <atomic>
#include <string>

namespace mv::modules {

// Loads map data from PostgreSQL/PostGIS (wrr-postgres) and publishes
// MapTileLoadedMsg.  When the DB is unreachable an empty tile is published
// (no procedural fallback — data must come from the real DB).
// The spatial query uses the geom column (EPSG:5179) with a GIST index.
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

    // DB 미연결 시 빈 타일 발행 (임의 데이터 없음).
    void publish_empty_tile(double cx, double cy);

    bool        db_ok_{false};
    uint32_t    version_{0};
    std::string conn_str_;
};

}  // namespace mv::modules
