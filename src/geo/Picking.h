#pragma once

#include "geo/BoundingBox.h"

#include <glm/glm.hpp>

#include <utility>
#include <vector>

namespace mv::geo {

struct Ray {
    glm::dvec3 origin{0.0};
    glm::dvec3 dir{0.0, 0.0, -1.0};
};

// Build a world-space ray from a screen pixel. proj/view are single-precision
// matrices already prepared by the renderer (view is camera-relative; we add
// the camera world position to the ray origin).
Ray screen_ray(int sx, int sy, int sw, int sh,
                const glm::mat4& view_local, const glm::mat4& proj,
                const glm::dvec3& cam_world);

// Slab AABB intersection. Returns true and the entry distance in t_out.
bool ray_aabb(const Ray& r, const AABB& box, double& t_out);

// Closest distance from the ground-plane intersection of the ray to a
// polyline segment. Returns the squared distance and -1 if the polyline is
// shorter than two vertices.
double ray_to_polyline_xy(const Ray& r,
                            const std::vector<std::pair<double, double>>& line,
                            double z_plane = 0.05);

}  // namespace mv::geo
