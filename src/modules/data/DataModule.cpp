#include "modules/data/DataModule.h"

#include "core/Messages.h"

#include <pqxx/pqxx>

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

// 성남시 수정구 전체 조감 중심점
// calc_sujeong.exe 결과: bbox center X=966875, Y=1939158, 대각+5% ≈ 6103m
static constexpr double kHomeX = 966875.0;
static constexpr double kHomeY = 1939158.0;
static constexpr double kTileRadius = 6000.0;  // 수정구 전체 커버

DataModule::DataModule() : ModuleBase("data") {
    conn_str_ = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=Vtlnrl1225!";
}

DataModule::~DataModule() = default;

void DataModule::on_init() {
    bus_->subscribe<BootstrapMsg>(this);
    bus_->subscribe<MapBoundsRequestMsg>(this);
    db_ok_ = try_connect();
    if (!db_ok_) {
        std::fprintf(stderr, "[data] ⚠ DB unreachable — empty tiles will be published\n");
    } else {
        std::fprintf(stderr, "[data] PostgreSQL connected (home: %.0f, %.0f)\n", kHomeX, kHomeY);
    }
}

void DataModule::on_message(const AnyMessage& msg) {
    if (auto* b = std::get_if<BootstrapMsg>(&msg)) {
        (void)b;
        if (db_ok_) query_and_publish(kHomeX, kHomeY, kTileRadius);
        else         publish_empty_tile(kHomeX, kHomeY);
        return;
    }
    if (auto* req = std::get_if<MapBoundsRequestMsg>(&msg)) {
        if (db_ok_) query_and_publish(req->center_x, req->center_y, req->radius_m);
        else         publish_empty_tile(req->center_x, req->center_y);
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
        const std::string bq = R"(
SELECT b.bul_man_no, b.buld_nm, b.bdtyp_cd, b.gro_flo_co,
       b.geom_wkt
FROM raw_rd_addr.tl_spbd_buld b
WHERE b.geom IS NOT NULL
  AND ST_Intersects(
        b.geom,
        ST_MakeEnvelope($1,$2,$3,$4,5179));
)";
        auto bres = txn.exec(bq, pqxx::params{minx, miny, maxx, maxy});

        for (const auto& row : bres) {
            const std::string wkt = row["geom_wkt"].as<std::string>("");
            auto pts = parse_wkt_polygon(wkt);
            if (pts.size() < 3) continue;

            BuildingFeature bf;
            bf.id         = "b" + std::to_string(row["bul_man_no"].as<long long>(0));
            bf.buld_nm    = row["buld_nm"].as<std::string>("");
            bf.bdtyp_cd   = row["bdtyp_cd"].as<std::string>("");
            bf.gro_floors = row["gro_flo_co"].as<int>(1);
            PolygonRing ring;
            ring.points = std::move(pts);
            bf.rings.push_back(std::move(ring));
            tile.buildings.push_back(std::move(bf));
        }

        // ---- Roads -----------------------------------------------------------
        // tl_sprd_rw: rw_sn is NOT unique (same road has many polygon segments).
        // LATERAL subquery picks ONE manage row per rw_sn without duplicating
        // geometry rows — returns ALL 7,628 road polygons in this area.
        const std::string rq = R"(
SELECT r.rw_sn, m.rn, m.road_bt, m.roa_cls_se,
       r.geom_wkt
FROM raw_rd_addr.tl_sprd_rw r
LEFT JOIN LATERAL (
    SELECT rn, road_bt, roa_cls_se
    FROM raw_rd_addr.tl_sprd_manage
    WHERE rds_man_no = r.rw_sn
    LIMIT 1
) m ON true
WHERE r.geom IS NOT NULL
  AND ST_Intersects(r.geom, ST_MakeEnvelope($1,$2,$3,$4,5179))
  AND ST_Area(r.geom) < 500000;  -- 상위 3개 이상 폴리곤만 제외 (842k/565k/522k)
)";
        auto rres = txn.exec(rq, pqxx::params{minx, miny, maxx, maxy});

        for (const auto& row : rres) {
            const std::string wkt = row["geom_wkt"].as<std::string>("");
            auto pts = parse_wkt_polygon(wkt);
            if (pts.size() < 2) continue;

            RoadFeature rf;
            rf.id         = "r" + std::to_string(row["rw_sn"].as<long long>(0));
            rf.rn         = row["rn"].is_null() ? "" : row["rn"].as<std::string>("");
            rf.width_m    = row["road_bt"].is_null() ? 6.0 : row["road_bt"].as<double>(6.0);
            rf.roa_cls_se = row["roa_cls_se"].is_null() ? "4" : row["roa_cls_se"].as<std::string>("4");
            rf.line       = std::move(pts);
            tile.roads.push_back(std::move(rf));
        }

        txn.commit();
        std::fprintf(stderr, "[data] tile v%llu: %zu buildings, %zu roads (cx=%.0f cy=%.0f)\n",
                     static_cast<unsigned long long>(tile.version),
                     tile.buildings.size(), tile.roads.size(), cx, cy);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[data] query error: %s — publishing empty tile\n", e.what());
        db_ok_ = false;
        publish_empty_tile(cx, cy);
        return;
    }

    bus_->publish(std::move(tile));
}

// ---------------------------------------------------------------------------
// Empty tile (DB unavailable — no procedural fallback)
// ---------------------------------------------------------------------------

void DataModule::publish_empty_tile(double cx, double cy) {
    MapTileLoadedMsg t;
    t.bounds_minx = cx - kTileRadius; t.bounds_maxx = cx + kTileRadius;
    t.bounds_miny = cy - kTileRadius; t.bounds_maxy = cy + kTileRadius;
    t.version     = ++version_;
    // buildings / roads 벡터는 비어있음 — DB가 없으면 빈 화면
    std::fprintf(stderr, "[data] ⚠ DB 없음 — 빈 타일 발행 (cx=%.0f cy=%.0f)\n", cx, cy);
    bus_->publish(std::move(t));
}

}  // namespace mv::modules
