#include "modules/data/DataModule.h"

#include "core/Messages.h"

#include <pqxx/pqxx>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mv::modules {

// ---------------------------------------------------------------------------
// WKT polygon parser  (handles POLYGON ((x y, x y, ...)) in UTM-K text)
// Returns empty vector on parse error.
// ---------------------------------------------------------------------------
static std::vector<std::pair<double,double>> parse_wkt_polygon(const std::string& wkt) {
    std::vector<std::pair<double,double>> pts;
    // Find first '('
    const auto ring_start = wkt.find('(');
    if (ring_start == std::string::npos) return pts;
    const auto ring_end = wkt.rfind(')');
    if (ring_end == std::string::npos || ring_end <= ring_start) return pts;
    // Skip outer parenthesis of POLYGON(( ... ))
    std::size_t pos = ring_start + 1;
    while (pos < ring_end && wkt[pos] == '(') ++pos;

    std::istringstream ss(wkt.substr(pos, ring_end - pos));
    std::string token;
    while (std::getline(ss, token, ',')) {
        // trim
        while (!token.empty() && (token.front() == ' ' || token.front() == '(')) token.erase(token.begin());
        while (!token.empty() && (token.back()  == ' ' || token.back()  == ')')) token.pop_back();
        std::istringstream pair(token);
        double x = 0.0, y = 0.0;
        if (pair >> x >> y) pts.push_back({x, y});
    }
    return pts;
}

// ---------------------------------------------------------------------------
// Module lifecycle
// ---------------------------------------------------------------------------

DataModule::DataModule() : ModuleBase("data") {
    conn_str_ = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=Vtlnrl1225!";
}

DataModule::~DataModule() = default;

void DataModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<MapBoundsRequestMsg>(this);
    db_ok_ = try_connect();
    if (!db_ok_) {
        std::fprintf(stderr, "[data] DB unreachable — using fallback sample tile\n");
    } else {
        std::fprintf(stderr, "[data] PostgreSQL connected\n");
    }
}

void DataModule::on_message(const AnyMessage& msg) {
    if (auto* b = std::get_if<BootstrapMsg>(&msg)) {
        (void)b;
        constexpr double kCHX = 953900.0, kCHY = 1952030.0;
        if (db_ok_) query_and_publish(kCHX, kCHY, 800.0);
        else         publish_fallback_tile(kCHX, kCHY);
        return;
    }
    if (auto* req = std::get_if<MapBoundsRequestMsg>(&msg)) {
        if (db_ok_) query_and_publish(req->center_x, req->center_y, req->radius_m);
        else         publish_fallback_tile(req->center_x, req->center_y);
    }
}

// ---------------------------------------------------------------------------
// DB helpers
// ---------------------------------------------------------------------------

bool DataModule::try_connect() {
    try {
        pqxx::connection c(conn_str_);
        return c.is_open();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[data] connect error: %s\n", e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// Main query
// ---------------------------------------------------------------------------

void DataModule::query_and_publish(double cx, double cy, double half) {
    const double minx = cx - half, maxx = cx + half;
    const double miny = cy - half, maxy = cy + half;

    MapTileLoadedMsg tile;
    tile.bounds_minx = minx; tile.bounds_maxx = maxx;
    tile.bounds_miny = miny; tile.bounds_maxy = maxy;
    tile.version     = ++version_;

    try {
        pqxx::connection c(conn_str_);
        pqxx::work txn(c);

        // ---- Buildings -------------------------------------------------------
        // Use ST_Intersects with a bbox envelope; falls back to WKT text filter
        // when the geom column is not yet populated (index build in progress).
        const std::string bq = R"(
SELECT b.bul_man_no, b.buld_nm, b.bdtyp_cd, b.gro_flo_co,
       b.geom_wkt
FROM raw_rd_addr.tl_spbd_buld b
WHERE b.geom IS NOT NULL
  AND ST_Intersects(
        b.geom,
        ST_MakeEnvelope($1,$2,$3,$4,5179))
LIMIT 2000;
)";
        auto bres = txn.exec(bq, pqxx::params{minx, miny, maxx, maxy});

        for (const auto& row : bres) {
            const std::string wkt = row["geom_wkt"].as<std::string>("");
            auto pts = parse_wkt_polygon(wkt);
            if (pts.size() < 3) continue;

            BuildingFeature bf;
            bf.id        = "b" + std::to_string(row["bul_man_no"].as<long long>(0));
            bf.buld_nm   = row["buld_nm"].as<std::string>("");
            bf.bdtyp_cd  = row["bdtyp_cd"].as<std::string>("");
            bf.gro_floors = row["gro_flo_co"].as<int>(1);
            PolygonRing ring;
            ring.points = std::move(pts);
            bf.rings.push_back(std::move(ring));
            tile.buildings.push_back(std::move(bf));
        }

        // ---- Roads -----------------------------------------------------------
        // tl_sprd_rw holds the road polygon; join tl_sprd_manage for name/width.
        const std::string rq = R"(
SELECT r.rw_sn, m.rn, m.road_bt, m.roa_cls_se,
       r.geom_wkt
FROM raw_rd_addr.tl_sprd_rw r
LEFT JOIN raw_rd_addr.tl_sprd_manage m ON r.rw_sn = m.rds_man_no
WHERE r.geom IS NOT NULL
  AND ST_Intersects(
        r.geom,
        ST_MakeEnvelope($1,$2,$3,$4,5179))
LIMIT 1000;
)";
        auto rres = txn.exec(rq, pqxx::params{minx, miny, maxx, maxy});

        for (const auto& row : rres) {
            const std::string wkt = row["geom_wkt"].as<std::string>("");
            // Road geometry in DB is already a polygon (actual road surface).
            // Extract the centrepoints of each side to build an approximate
            // polyline, or simply pass the polygon ring as a polyline.
            auto pts = parse_wkt_polygon(wkt);
            if (pts.size() < 2) continue;

            RoadFeature rf;
            rf.id        = "r" + std::to_string(row["rw_sn"].as<long long>(0));
            rf.rn        = row["rn"].is_null() ? "" : row["rn"].as<std::string>("");
            rf.width_m   = row["road_bt"].is_null() ? 6.0 : row["road_bt"].as<double>(6.0);
            rf.roa_cls_se = row["roa_cls_se"].is_null() ? "4" : row["roa_cls_se"].as<std::string>("4");

            // The road is stored as a polygon; use its ring as the polyline
            // boundary (renderer will use width_m to offset a quad strip).
            // For display, extract just the midpoints of the first two pairs.
            rf.line = std::move(pts);
            tile.roads.push_back(std::move(rf));
        }

        txn.commit();
        std::fprintf(stderr, "[data] tile v%llu: %zu buildings, %zu roads (cx=%.0f cy=%.0f)\n",
                     static_cast<unsigned long long>(tile.version), tile.buildings.size(), tile.roads.size(), cx, cy);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[data] query error: %s — using fallback\n", e.what());
        db_ok_ = false;
        publish_fallback_tile(cx, cy);
        return;
    }

    bus_->publish(std::move(tile));
}

// ---------------------------------------------------------------------------
// Fallback (hard-coded sample tile)
// ---------------------------------------------------------------------------

namespace {

BuildingFeature make_building(std::string id, double cx, double cy,
                               double w, double h, int floors,
                               std::string addr, std::string nm,
                               std::string bdtyp = "10102") {
    BuildingFeature b;
    b.id = std::move(id); b.gro_floors = floors;
    b.addr_road = std::move(addr); b.buld_nm = std::move(nm);
    b.bdtyp_cd  = std::move(bdtyp);
    PolygonRing ring;
    ring.points = {{cx-w/2,cy-h/2},{cx+w/2,cy-h/2},{cx+w/2,cy+h/2},{cx-w/2,cy+h/2}};
    b.rings.push_back(std::move(ring));
    return b;
}

RoadFeature make_road(std::string id, std::vector<std::pair<double,double>> line,
                       std::string rn, std::string cls, double width_m) {
    RoadFeature r;
    r.id=std::move(id); r.line=std::move(line);
    r.rn=std::move(rn); r.roa_cls_se=std::move(cls); r.width_m=width_m;
    return r;
}

} // namespace

uint32_t DataModule::cell_hash(int gx, int gy) const {
    uint32_t h = static_cast<uint32_t>(gx*73856093) ^ static_cast<uint32_t>(gy*19349663);
    h^=h>>16; h*=0x45d9f3bU; h^=h>>16; h*=0x45d9f3bU; h^=h>>16;
    return h;
}

void DataModule::publish_fallback_tile(double cx, double cy) {
    constexpr double kHalf=800.0, kGrid=30.0;
    MapTileLoadedMsg t;
    t.bounds_minx=cx-kHalf; t.bounds_miny=cy-kHalf;
    t.bounds_maxx=cx+kHalf; t.bounds_maxy=cy+kHalf;
    t.version=++version_;

    constexpr double kCHX=953900.0,kCHY=1952030.0;
    if(std::abs(cx-kCHX)<kHalf+200&&std::abs(cy-kCHY)<kHalf+200){
        t.buildings.push_back(make_building("city_hall",    953900,1952030,60,80,13,"세종대로 110","서울특별시청","10102"));
        t.buildings.push_back(make_building("press_center", 953850,1952090,30,40,10,"세종대로 124","서울언론회관","10299"));
        t.buildings.push_back(make_building("dongbang",     953960,1952040,40,35,12,"세종대로 95", "동방빌딩","10299"));
        t.roads.push_back(make_road("sejong",{{953800,1952180},{953905,1952080},{953980,1951950}},"세종대로","2",30));
        t.roads.push_back(make_road("eulji", {{953700,1952050},{954060,1952050}}, "을지로","3",16));
    }

    // Procedural fill
    const int gi0=int(std::floor((cx-kHalf)/kGrid)), gi1=int(std::ceil((cx+kHalf)/kGrid));
    const int gj0=int(std::floor((cy-kHalf)/kGrid)), gj1=int(std::ceil((cy+kHalf)/kGrid));
    int idx=0;
    for(int gi=gi0;gi<gi1;++gi) for(int gj=gj0;gj<gj1;++gj){
        const uint32_t h=cell_hash(gi,gj);
        if((h&0xFF)<89) continue;
        const double bx=gi*kGrid+2+(h&0x1F)*(kGrid*0.75/31.0);
        const double by=gj*kGrid+2+((h>>5)&0x1F)*(kGrid*0.75/31.0);
        if(std::abs(bx-kCHX)<80&&std::abs(by-kCHY)<80) continue;
        const double bw=5+((h>>10)&0x0F)*1.1, bh=5+((h>>14)&0x0F)*1.1;
        const int fl=1+int((h>>18)&0x1F);
        t.buildings.push_back(make_building("p_"+std::to_string(gi)+"_"+std::to_string(gj),
            bx,by,bw,bh,fl,"자동생성로 "+std::to_string(++idx),"건물 "+std::to_string(idx),"10299"));
    }
    bus_->publish(std::move(t));
}

}  // namespace mv::modules
