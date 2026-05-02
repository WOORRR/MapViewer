#include "modules/data/DataModule.h"

#include <utility>
#include <vector>

namespace mv::modules {

DataModule::DataModule() : ModuleBase("data") {}
DataModule::~DataModule() = default;

void DataModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<MapBoundsRequestMsg>(this);
}

void DataModule::on_message(const AnyMessage& msg) {
    if (std::get_if<BootstrapMsg>(&msg)) {
        publish_sample_tile();
        return;
    }
    if (std::get_if<MapBoundsRequestMsg>(&msg)) {
        // Prototype: re-publish the same tile regardless of bbox. A future
        // PostGIS-backed implementation will run a spatial query here.
        publish_sample_tile();
    }
}

namespace {

BuildingFeature make_building(std::string id, double cx, double cy, double w, double h,
                               int floors, std::string addr, std::string nm,
                               std::string bdtyp = "10102") {
    BuildingFeature b;
    b.id          = std::move(id);
    b.gro_floors  = floors;
    b.addr_road   = std::move(addr);
    b.buld_nm     = std::move(nm);
    b.bdtyp_cd    = std::move(bdtyp);
    PolygonRing ring;
    ring.points = {
        {cx - w / 2, cy - h / 2},
        {cx + w / 2, cy - h / 2},
        {cx + w / 2, cy + h / 2},
        {cx - w / 2, cy + h / 2}
    };
    b.rings.push_back(std::move(ring));
    return b;
}

RoadFeature make_road(std::string id, std::vector<std::pair<double,double>> line,
                       std::string rn, std::string roa_cls_se, double width_m) {
    RoadFeature r;
    r.id          = std::move(id);
    r.line        = std::move(line);
    r.rn          = std::move(rn);
    r.roa_cls_se  = std::move(roa_cls_se);
    r.width_m     = width_m;
    return r;
}

}  // namespace

void DataModule::publish_sample_tile() {
    MapTileLoadedMsg t;
    t.bounds_minx = 953700.0; t.bounds_miny = 1951900.0;
    t.bounds_maxx = 954100.0; t.bounds_maxy = 1952200.0;
    t.version     = 1;

    t.buildings.push_back(make_building("city_hall",      953900.0, 1952030.0, 60.0, 80.0, 13, "세종대로 110", "서울특별시청",       "10102"));
    t.buildings.push_back(make_building("press_center",   953850.0, 1952090.0, 30.0, 40.0, 10, "세종대로 124", "서울언론회관",       "10299"));
    t.buildings.push_back(make_building("plaza_north",    953890.0, 1952140.0, 50.0, 30.0,  5, "세종대로 130", "광장 북측 빌딩",     "04999"));
    t.buildings.push_back(make_building("dongbang",       953960.0, 1952040.0, 40.0, 35.0, 12, "세종대로 95",  "동방빌딩",           "10299"));
    t.buildings.push_back(make_building("hall_west",      953830.0, 1952005.0, 25.0, 25.0,  4, "세종대로 134", "서편 부속",          "03100"));
    t.buildings.push_back(make_building("city_hall_old",  953940.0, 1952090.0, 35.0, 28.0,  3, "세종대로 110", "서울도서관(구 청사)", "08005"));

    t.roads.push_back(make_road("sejong_main", {{953800.0, 1952180.0}, {953905.0, 1952080.0}, {953980.0, 1951950.0}}, "세종대로",   "2", 30.0));
    t.roads.push_back(make_road("eulji_cross", {{953700.0, 1952050.0}, {954060.0, 1952050.0}},                         "을지로",     "3", 16.0));
    t.roads.push_back(make_road("alley_11",    {{953860.0, 1951970.0}, {953920.0, 1952090.0}},                         "세종대로11길","4", 6.0));

    bus_->publish(std::move(t));
}

}  // namespace mv::modules
