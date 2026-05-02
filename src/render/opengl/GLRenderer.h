#pragma once

#include "core/Messages.h"
#include "geo/BoundingBox.h"
#include "render/Camera.h"
#include "render/opengl/GLContext.h"
#include "render/opengl/GLShader.h"

#include <glad/glad.h>

#include <memory>
#include <string>
#include <vector>

namespace mv::render::gl {

struct BuildingHit {
    std::string id;
    geo::AABB   aabb;          // metres in UTM-K
    BuildingFeature feature;   // for popup display
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    Context& context() { return ctx_; }
    Camera&  camera()  { return camera_; }

    void begin_frame();
    void draw_grid();
    void load_tile(const MapTileLoadedMsg& tile, float building_height_m);
    void draw_tile();
    void add_balloon(const std::string& instance_id,
                      const glm::dvec3& world_pos,
                      const glm::vec3& color);
    void remove_balloon(const std::string& instance_id);
    void draw_balloons();
    void append_trail(const glm::dvec3& world_pos);
    void clear_trail();
    void draw_trail();
    void end_frame();

    // Read-only access for collision/picking helpers in future steps.
    const std::vector<BuildingHit>& buildings() const { return buildings_; }
    float building_height_m() const { return last_building_height_m_; }

private:
    void   build_grid_around(const glm::dvec3& origin);
    void   bind_view_proj_uniforms(Shader& s) const;
    static GLuint make_vao_pcn(const std::vector<float>& interleaved,
                                 const std::vector<unsigned>& indices,
                                 GLuint& vbo_out, GLuint& ebo_out);

    Context  ctx_;
    Camera   camera_;
    std::unique_ptr<Shader> line_shader_;
    std::unique_ptr<Shader> mesh_shader_;

    // Grid
    GLuint  grid_vao_{0};
    GLuint  grid_vbo_{0};
    GLsizei grid_vertex_count_{0};
    glm::dvec3 grid_origin_{0.0, 0.0, 0.0};

    // Tile (buildings + roads)
    GLuint  tile_buildings_vao_{0}, tile_buildings_vbo_{0}, tile_buildings_ebo_{0};
    GLsizei tile_buildings_index_count_{0};
    GLuint  tile_roads_vao_{0}, tile_roads_vbo_{0}, tile_roads_ebo_{0};
    GLsizei tile_roads_index_count_{0};
    glm::dvec3 tile_origin_{0.0, 0.0, 0.0};
    bool   tile_loaded_{false};
    float  last_building_height_m_{10.0f};

    std::vector<BuildingHit> buildings_;  // for picking/collision

    // Balloons (object instances)
    struct Balloon {
        std::string id;
        glm::dvec3 world_pos{0.0};
        glm::vec3  color{1.0f, 0.6f, 0.2f};
    };
    std::vector<Balloon> balloons_;
    GLuint balloon_vao_{0}, balloon_vbo_{0}, balloon_ebo_{0};
    GLsizei balloon_index_count_{0};
    void ensure_balloon_mesh();

    // Location-log trail
    std::vector<glm::dvec3> trail_pts_;
    GLuint trail_vao_{0}, trail_vbo_{0};
    GLsizei trail_vertex_count_{0};
    void rebuild_trail_buffer();
};

}  // namespace mv::render::gl
