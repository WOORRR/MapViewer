#pragma once

#include <glm/glm.hpp>
#include <limits>

namespace mv::geo {

struct AABB {
    glm::dvec3 min{ std::numeric_limits<double>::infinity()};
    glm::dvec3 max{-std::numeric_limits<double>::infinity()};

    void expand(const glm::dvec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    bool contains_xy(double x, double y) const {
        return x >= min.x && x <= max.x && y >= min.y && y <= max.y;
    }

    bool contains(const glm::dvec3& p, double eps = 0.0) const {
        return p.x >= min.x - eps && p.x <= max.x + eps &&
               p.y >= min.y - eps && p.y <= max.y + eps &&
               p.z >= min.z - eps && p.z <= max.z + eps;
    }
};

}  // namespace mv::geo
