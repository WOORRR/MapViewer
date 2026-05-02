#include "geo/BoundingBox.h"
#include "geo/Picking.h"

#include <gtest/gtest.h>

using mv::geo::AABB;
using mv::geo::Ray;
using mv::geo::ray_aabb;
using mv::geo::ray_to_polyline_xy;

TEST(Picking, RayHitsAABB) {
    AABB box;
    box.expand({-1.0, -1.0, -1.0});
    box.expand({ 1.0,  1.0,  1.0});

    Ray r;
    r.origin = {0.0, 0.0, 5.0};
    r.dir    = {0.0, 0.0, -1.0};

    double t = 0.0;
    EXPECT_TRUE(ray_aabb(r, box, t));
    EXPECT_NEAR(t, 4.0, 1e-9);
}

TEST(Picking, RayMissesAABB) {
    AABB box;
    box.expand({-1.0, -1.0, -1.0});
    box.expand({ 1.0,  1.0,  1.0});

    Ray r;
    r.origin = {5.0, 0.0, 5.0};
    r.dir    = {0.0, 0.0, -1.0};

    double t = 0.0;
    EXPECT_FALSE(ray_aabb(r, box, t));
}

TEST(Picking, PolylineDistance) {
    Ray r;
    r.origin = {10.0, 0.0, 5.0};
    r.dir    = {0.0, 0.0, -1.0};

    std::vector<std::pair<double, double>> line = {{0.0, 0.0}, {20.0, 0.0}};
    const double d = ray_to_polyline_xy(r, line, 0.0);
    EXPECT_NEAR(d, 0.0, 1e-9);
}
