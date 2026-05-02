#include "geo/CoordTransform.h"

#include <gtest/gtest.h>

#include <cmath>

using mv::geo::CoordTransform;
using mv::geo::LatLon;
using mv::geo::UtmK;

namespace {

// Round-trip a known landmark. Tolerance is 0.01 m (1 cm) per CLAUDE.md
// verification step.
void expect_roundtrip(double lat_deg, double lon_deg) {
    CoordTransform xf;
    const UtmK xy = xf.to_utmk({lat_deg, lon_deg});
    const LatLon back = xf.to_latlon(xy);

    // 1e-7 deg ≈ 1.1 cm at this latitude — comfortably under the 1 cm budget
    // when expressed in metres.
    EXPECT_NEAR(back.lat_deg, lat_deg, 1e-7) << "lat round-trip drift";
    EXPECT_NEAR(back.lon_deg, lon_deg, 1e-7) << "lon round-trip drift";
}

}  // namespace

TEST(CoordTransform, OriginAtFalseOffsets) {
    // (lat0=38°, lon0=127.5°) must project exactly to the false-origin point.
    CoordTransform xf;
    const UtmK xy = xf.to_utmk({CoordTransform::kLatOfOrigin,
                                 CoordTransform::kCentralMeridian});
    EXPECT_NEAR(xy.x, CoordTransform::kFalseEasting,  1e-6);
    EXPECT_NEAR(xy.y, CoordTransform::kFalseNorthing, 1e-6);
}

TEST(CoordTransform, SeoulCityHallRoundTrip) {
    expect_roundtrip(37.5665, 126.9780);
}

TEST(CoordTransform, GwanghwamunRoundTrip) {
    expect_roundtrip(37.5759, 126.9769);
}

TEST(CoordTransform, BusanRoundTrip) {
    expect_roundtrip(35.1796, 129.0756);
}

TEST(CoordTransform, SeoulCityHallReasonableNorthEast) {
    // Seoul City Hall in EPSG:5179 lands near (953900, 1952030). 50 m
    // tolerance accommodates the WGS84 vs ITRF2000 datum simplification.
    CoordTransform xf;
    const UtmK xy = xf.to_utmk({37.5665, 126.9780});
    EXPECT_NEAR(xy.x,   953900.0, 50.0) << "got x=" << xy.x;
    EXPECT_NEAR(xy.y,  1952030.0, 50.0) << "got y=" << xy.y;
}
