#include "geo/Picking.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mv::geo {

Ray screen_ray(int sx, int sy, int sw, int sh,
                const glm::mat4& view_local, const glm::mat4& proj,
                const glm::dvec3& cam_world) {
    const float nx = 2.0f * static_cast<float>(sx) / static_cast<float>(sw) - 1.0f;
    const float ny = 1.0f - 2.0f * static_cast<float>(sy) / static_cast<float>(sh);
    const glm::vec4 ndc{nx, ny, -1.0f, 1.0f};
    glm::vec4 eye = glm::inverse(proj) * ndc;
    eye.z = -1.0f; eye.w = 0.0f;
    const glm::vec4 world = glm::inverse(view_local) * eye;
    Ray r;
    r.origin = cam_world;
    r.dir = glm::normalize(glm::dvec3{world.x, world.y, world.z});
    return r;
}

bool ray_aabb(const Ray& r, const AABB& box, double& t_out) {
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax =  std::numeric_limits<double>::infinity();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(r.dir[i]) < 1e-12) {
            if (r.origin[i] < box.min[i] || r.origin[i] > box.max[i]) return false;
            continue;
        }
        const double inv = 1.0 / r.dir[i];
        double t1 = (box.min[i] - r.origin[i]) * inv;
        double t2 = (box.max[i] - r.origin[i]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (tmax < 0.0) return false;
    t_out = (tmin >= 0.0) ? tmin : tmax;
    return true;
}

double ray_to_polyline_xy(const Ray& r,
                            const std::vector<std::pair<double, double>>& line,
                            double z_plane) {
    if (line.size() < 2) return std::numeric_limits<double>::infinity();
    if (std::abs(r.dir.z) < 1e-9) return std::numeric_limits<double>::infinity();
    const double t = (z_plane - r.origin.z) / r.dir.z;
    if (t < 0.0) return std::numeric_limits<double>::infinity();
    const double px = r.origin.x + r.dir.x * t;
    const double py = r.origin.y + r.dir.y * t;

    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const double x0 = line[i].first,     y0 = line[i].second;
        const double x1 = line[i + 1].first, y1 = line[i + 1].second;
        const double dx = x1 - x0, dy = y1 - y0;
        const double len2 = dx * dx + dy * dy;
        if (len2 < 1e-9) continue;
        double s = ((px - x0) * dx + (py - y0) * dy) / len2;
        s = std::clamp(s, 0.0, 1.0);
        const double cx = x0 + dx * s;
        const double cy = y0 + dy * s;
        const double d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);
        if (d2 < best) best = d2;
    }
    return std::sqrt(best);
}

}  // namespace mv::geo
