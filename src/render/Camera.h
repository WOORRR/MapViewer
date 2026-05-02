#pragma once

// Free-look camera in UTM-K world space (x=east, y=north, z=up, all metres).
// FoV interpretation per CLAUDE.md "−60..+60° in 1° steps": the value is a
// delta around a base of 60°, so the effective vertical FoV is clamp(60 +
// delta, 1, 119)°.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mv::render {

struct Camera {
    glm::dvec3 position{953900.0, 1952030.0, 80.0};
    float yaw_deg{0.0f};       // 0 = looking north (+y), positive = clockwise from above
    float pitch_deg{-25.0f};   // negative = looking down toward ground
    int   fov_delta_deg{0};    // clamped to [-60, +60]

    static constexpr float kBaseFovDeg = 60.0f;
    static constexpr int   kFovDeltaMin = -60;
    static constexpr int   kFovDeltaMax = +60;

    float effective_fov_deg() const {
        const float f = kBaseFovDeg + static_cast<float>(fov_delta_deg);
        return glm::clamp(f, 1.0f, 119.0f);
    }

    glm::vec3 forward() const {
        const float yaw   = glm::radians(yaw_deg);
        const float pitch = glm::radians(pitch_deg);
        return glm::vec3{
            std::cos(pitch) * std::sin(yaw),
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch)
        };
    }

    glm::vec3 right() const {
        const float yaw = glm::radians(yaw_deg);
        return glm::vec3{std::cos(yaw), -std::sin(yaw), 0.0f};
    }

    // View matrix rendered in a local (camera-relative) frame: callers should
    // subtract camera position from world coords before submitting to GPU to
    // avoid float precision loss far from origin.
    glm::mat4 view_local() const {
        const glm::vec3 f = forward();
        const glm::vec3 up{0.0f, 0.0f, 1.0f};
        return glm::lookAt(glm::vec3{0.0f}, f, up);
    }

    glm::mat4 projection(float aspect, float near = 1.0f, float far = 20000.0f) const {
        return glm::perspective(glm::radians(effective_fov_deg()), aspect, near, far);
    }
};

}  // namespace mv::render
