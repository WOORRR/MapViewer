#pragma once

// Korean UTM-K (EPSG:5179, GRS80, central meridian 127.5°, latitude of origin
// 38°, false easting 1,000,000 m, false northing 2,000,000 m, scale factor
// 0.9996). The datum is ITRF2000 / GRS80; for prototype-level accuracy we
// treat WGS84 input as identical (sub-cm divergence over the Korean
// peninsula).

#include <memory>

namespace GeographicLib { class TransverseMercator; }

namespace mv::geo {

struct LatLon { double lat_deg{0.0}; double lon_deg{0.0}; };
struct UtmK   { double x{0.0};       double y{0.0}; };  // metres in EPSG:5179

class CoordTransform {
public:
    CoordTransform();
    ~CoordTransform();

    UtmK   to_utmk(const LatLon& ll) const;
    LatLon to_latlon(const UtmK& xy) const;

    static constexpr double kFalseEasting   = 1'000'000.0;
    static constexpr double kFalseNorthing  = 2'000'000.0;
    static constexpr double kCentralMeridian = 127.5;
    static constexpr double kLatOfOrigin    = 38.0;
    static constexpr double kScaleFactor    = 0.9996;
    // GRS80
    static constexpr double kSemiMajor      = 6'378'137.0;
    static constexpr double kInvFlattening  = 298.257222101;

private:
    std::unique_ptr<GeographicLib::TransverseMercator> tm_;
    double y_origin_offset_{0.0};  // meridian arc length at lat0, scaled by k0
};

inline bool is_within_korea(const LatLon& ll) {
    return ll.lat_deg > 32.0 && ll.lat_deg < 39.5 &&
           ll.lon_deg > 124.0 && ll.lon_deg < 132.0;
}

}  // namespace mv::geo
