// Temporary: calculate UTM-K centre and radius for Sujeong-gu (수정구)
// Build target: calc_sujeong
#include "geo/CoordTransform.h"
#include <cmath>
#include <cstdio>

int main() {
    mv::geo::CoordTransform xf;

    // Known reference (verified via PostGIS)
    const auto ref = xf.to_utmk({37.4199, 127.1265});
    std::printf("성남시청 UTM-K: x=%.1f  y=%.1f\n", ref.x, ref.y);

    // Sujeong-gu approximate bounding corners (from Korean administrative map)
    // West boundary ~127.068°E, East ~127.183°E
    // South ~37.426°N,  North ~37.476°N
    struct Pt { double lat, lon; const char* name; };
    Pt pts[] = {
        {37.426, 127.068, "SW corner"},
        {37.476, 127.068, "NW corner"},
        {37.476, 127.183, "NE corner"},
        {37.426, 127.183, "SE corner"},
        // Also district-office area (산성동) as candidate centre
        {37.4499, 127.1390, "수정구청"},
        // Geographic centroid estimate
        {37.451,  127.126,  "centroid estimate"},
    };

    double xmin=1e18, xmax=-1e18, ymin=1e18, ymax=-1e18;
    for (const auto& p : pts) {
        const auto u = xf.to_utmk({p.lat, p.lon});
        std::printf("%-18s  lat=%7.4f lon=%8.4f  x=%9.1f  y=%10.1f\n",
                    p.name, p.lat, p.lon, u.x, u.y);
        if (u.x < xmin) xmin = u.x;
        if (u.x > xmax) xmax = u.x;
        if (u.y < ymin) ymin = u.y;
        if (u.y > ymax) ymax = u.y;
    }
    const double cx = (xmin + xmax) / 2.0;
    const double cy = (ymin + ymax) / 2.0;
    const double rx = (xmax - xmin) / 2.0;
    const double ry = (ymax - ymin) / 2.0;
    const double r  = std::sqrt(rx*rx + ry*ry) * 1.05; // 5% margin
    std::printf("\nBbox center:  x=%.1f  y=%.1f\n", cx, cy);
    std::printf("Half-spans:   rx=%.1f  ry=%.1f\n", rx, ry);
    std::printf("Radius (diag+5%%): %.1f m\n", r);
    return 0;
}
