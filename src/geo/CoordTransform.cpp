#include "geo/CoordTransform.h"

#include <GeographicLib/TransverseMercator.hpp>

namespace mv::geo {

CoordTransform::CoordTransform()
    : tm_(std::make_unique<GeographicLib::TransverseMercator>(
          kSemiMajor,
          1.0 / kInvFlattening,
          kScaleFactor)) {
    // GeographicLib's TransverseMercator measures northing from the equator.
    // EPSG:5179 measures from the latitude of origin (38°N). Compute the
    // northing of (lat0, lon0) in the equator-referenced projection so we can
    // shift to the EPSG-defined origin.
    double x0{0.0}, y0{0.0};
    tm_->Forward(kCentralMeridian, kLatOfOrigin, kCentralMeridian, x0, y0);
    y_origin_offset_ = y0;  // x0 is 0 by construction
}

CoordTransform::~CoordTransform() = default;

UtmK CoordTransform::to_utmk(const LatLon& ll) const {
    double x_local{0.0}, y_local{0.0};
    tm_->Forward(kCentralMeridian, ll.lat_deg, ll.lon_deg, x_local, y_local);
    return UtmK{
        x_local + kFalseEasting,
        (y_local - y_origin_offset_) + kFalseNorthing
    };
}

LatLon CoordTransform::to_latlon(const UtmK& xy) const {
    const double x_local = xy.x - kFalseEasting;
    const double y_local = (xy.y - kFalseNorthing) + y_origin_offset_;
    double lat{0.0}, lon{0.0};
    tm_->Reverse(kCentralMeridian, x_local, y_local, lat, lon);
    return LatLon{lat, lon};
}

}  // namespace mv::geo
