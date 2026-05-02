#include "render/opengl/GLRenderer.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mv::render::gl {

namespace {

constexpr const char* kLineVS = R"GLSL(
#version 460 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec3 v_color;
void main() {
    v_color = a_color;
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
}
)GLSL";

constexpr const char* kLineFS = R"GLSL(
#version 460 core
in  vec3 v_color;
out vec4 frag;
void main() { frag = vec4(v_color, 1.0); }
)GLSL";

constexpr const char* kMeshVS = R"GLSL(
#version 460 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec3 a_normal;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec3 v_color;
out vec3 v_normal;
void main() {
    v_color  = a_color;
    v_normal = a_normal;
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
}
)GLSL";

constexpr const char* kMeshFS = R"GLSL(
#version 460 core
in  vec3 v_color;
in  vec3 v_normal;
out vec4 frag;
uniform vec3 u_sun_dir;
void main() {
    vec3 N = normalize(v_normal);
    float L = max(dot(N, normalize(u_sun_dir)), 0.0);
    float intensity = 0.35 + 0.65 * L;
    frag = vec4(v_color * intensity, 1.0);
}
)GLSL";

void push_vertex(std::vector<float>& v,
                  float px, float py, float pz,
                  float cr, float cg, float cb,
                  float nx, float ny, float nz) {
    v.insert(v.end(), {px, py, pz, cr, cg, cb, nx, ny, nz});
}

}  // namespace

GLuint Renderer::make_vao_pcn(const std::vector<float>& interleaved,
                                const std::vector<unsigned>& indices,
                                GLuint& vbo_out, GLuint& ebo_out) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_out);
    glGenBuffers(1, &ebo_out);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_out);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                 interleaved.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_out);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)),
                 indices.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 9 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    return vao;
}

Renderer::Renderer() {
    line_shader_ = std::make_unique<Shader>(kLineVS, kLineFS);
    mesh_shader_ = std::make_unique<Shader>(kMeshVS, kMeshFS);

    glGenVertexArrays(1, &grid_vao_);
    glGenBuffers(1, &grid_vbo_);
    build_grid_around(camera_.position);
}

Renderer::~Renderer() {
    if (grid_vbo_ != 0) glDeleteBuffers(1, &grid_vbo_);
    if (grid_vao_ != 0) glDeleteVertexArrays(1, &grid_vao_);
    if (tile_buildings_vbo_ != 0) glDeleteBuffers(1, &tile_buildings_vbo_);
    if (tile_buildings_ebo_ != 0) glDeleteBuffers(1, &tile_buildings_ebo_);
    if (tile_buildings_vao_ != 0) glDeleteVertexArrays(1, &tile_buildings_vao_);
    if (tile_roads_vbo_ != 0) glDeleteBuffers(1, &tile_roads_vbo_);
    if (tile_roads_ebo_ != 0) glDeleteBuffers(1, &tile_roads_ebo_);
    if (tile_roads_vao_ != 0) glDeleteVertexArrays(1, &tile_roads_vao_);
    if (balloon_vbo_ != 0) glDeleteBuffers(1, &balloon_vbo_);
    if (balloon_ebo_ != 0) glDeleteBuffers(1, &balloon_ebo_);
    if (balloon_vao_ != 0) glDeleteVertexArrays(1, &balloon_vao_);
    if (trail_vbo_ != 0) glDeleteBuffers(1, &trail_vbo_);
    if (trail_vao_ != 0) glDeleteVertexArrays(1, &trail_vao_);
}

void Renderer::build_grid_around(const glm::dvec3& origin) {
    grid_origin_ = origin;
    constexpr float half_size = 1500.0f;
    constexpr float spacing   = 50.0f;
    constexpr int   spans     = static_cast<int>(half_size * 2.0f / spacing);

    std::vector<float> verts;
    auto emit = [&](float x0, float y0, float x1, float y1,
                    float r, float g, float b) {
        verts.insert(verts.end(), {x0, y0, 0.0f, r, g, b,
                                    x1, y1, 0.0f, r, g, b});
    };
    for (int i = 0; i <= spans; ++i) {
        const float t = -half_size + static_cast<float>(i) * spacing;
        const bool axis = (i % 10 == 0);
        const float c = axis ? 0.45f : 0.18f;
        emit(t, -half_size, t, +half_size, c, c, c + 0.05f);
        emit(-half_size, t, +half_size, t, c, c, c + 0.05f);
    }
    emit(-half_size, 0.0f, +half_size, 0.0f, 0.65f, 0.10f, 0.10f);
    emit(0.0f, -half_size, 0.0f, +half_size, 0.10f, 0.65f, 0.10f);

    grid_vertex_count_ = static_cast<GLsizei>(verts.size() / 6);
    glBindVertexArray(grid_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::bind_view_proj_uniforms(Shader& s) const {
    int w = 0, h = 0;
    ctx_.framebuffer_size(w, h);
    const float aspect = (h <= 0) ? 1.0f : static_cast<float>(w) / static_cast<float>(h);
    const glm::mat4 proj = camera_.projection(aspect);
    const glm::dvec3 cam_local_d = camera_.position - tile_origin_;
    const glm::vec3  cam_local{cam_local_d};
    glm::mat4 view = camera_.view_local();
    view = glm::translate(view, -cam_local);
    s.set_mat4("u_view", glm::value_ptr(view));
    s.set_mat4("u_proj", glm::value_ptr(proj));
}

void Renderer::begin_frame() {
    int w = 0, h = 0;
    ctx_.framebuffer_size(w, h);
    if (w <= 0 || h <= 0) return;
    glViewport(0, 0, w, h);
    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::dvec3 d = camera_.position - grid_origin_;
    if (std::abs(d.x) > 1000.0 || std::abs(d.y) > 1000.0) {
        build_grid_around(camera_.position);
    }
}

void Renderer::draw_grid() {
    line_shader_->use();
    int w = 0, h = 0;
    ctx_.framebuffer_size(w, h);
    const float aspect = (h <= 0) ? 1.0f : static_cast<float>(w) / static_cast<float>(h);
    const glm::mat4 proj = camera_.projection(aspect);
    const glm::dvec3 cam_local_d = camera_.position - grid_origin_;
    const glm::vec3  cam_local{cam_local_d};
    glm::mat4 view = camera_.view_local();
    view = glm::translate(view, -cam_local);
    line_shader_->set_mat4("u_view", glm::value_ptr(view));
    line_shader_->set_mat4("u_proj", glm::value_ptr(proj));
    glBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, grid_vertex_count_);
    glBindVertexArray(0);
}

void Renderer::load_tile(const MapTileLoadedMsg& tile, float building_height_m) {
    last_building_height_m_ = building_height_m;
    tile_origin_ = glm::dvec3{
        (tile.bounds_minx + tile.bounds_maxx) * 0.5,
        (tile.bounds_miny + tile.bounds_maxy) * 0.5,
        0.0
    };

    // Buildings — extrude polygon ring to fixed height. Assume convex CCW.
    constexpr float kWallR = 0.45f, kWallG = 0.55f, kWallB = 0.78f;
    constexpr float kRoofR = 0.30f, kRoofG = 0.40f, kRoofB = 0.62f;

    std::vector<float> b_verts;
    std::vector<unsigned> b_idx;
    buildings_.clear();
    buildings_.reserve(tile.buildings.size());

    for (const BuildingFeature& bf : tile.buildings) {
        if (bf.rings.empty() || bf.rings.front().points.size() < 3) {
            continue;
        }
        const auto& ring = bf.rings.front().points;
        const std::size_t N = ring.size();

        BuildingHit hit;
        hit.id = bf.id;
        hit.feature = bf;

        // local coordinates relative to tile_origin_, +z up
        std::vector<glm::vec3> base(N);
        std::vector<glm::vec3> top(N);
        for (std::size_t i = 0; i < N; ++i) {
            const double lx = ring[i].first  - tile_origin_.x;
            const double ly = ring[i].second - tile_origin_.y;
            base[i] = glm::vec3{static_cast<float>(lx), static_cast<float>(ly), 0.0f};
            top[i]  = glm::vec3{static_cast<float>(lx), static_cast<float>(ly), building_height_m};
            hit.aabb.expand(glm::dvec3{ring[i].first, ring[i].second, 0.0});
            hit.aabb.expand(glm::dvec3{ring[i].first, ring[i].second, building_height_m});
        }
        buildings_.push_back(std::move(hit));

        // side faces
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t j = (i + 1) % N;
            const glm::vec3 edge = base[j] - base[i];
            // outward normal for CCW ring: cross(edge, +Z)
            const glm::vec3 nrm = glm::normalize(glm::vec3{ edge.y, -edge.x, 0.0f });
            const unsigned i0 = static_cast<unsigned>(b_verts.size() / 9);
            push_vertex(b_verts, base[i].x, base[i].y, base[i].z, kWallR, kWallG, kWallB, nrm.x, nrm.y, nrm.z);
            push_vertex(b_verts, base[j].x, base[j].y, base[j].z, kWallR, kWallG, kWallB, nrm.x, nrm.y, nrm.z);
            push_vertex(b_verts, top[j].x,  top[j].y,  top[j].z,  kWallR, kWallG, kWallB, nrm.x, nrm.y, nrm.z);
            push_vertex(b_verts, top[i].x,  top[i].y,  top[i].z,  kWallR, kWallG, kWallB, nrm.x, nrm.y, nrm.z);
            b_idx.insert(b_idx.end(), {i0, i0 + 1, i0 + 2, i0, i0 + 2, i0 + 3});
        }
        // roof fan (assumes convex)
        const unsigned r0 = static_cast<unsigned>(b_verts.size() / 9);
        for (std::size_t i = 0; i < N; ++i) {
            push_vertex(b_verts,
                        top[i].x, top[i].y, top[i].z,
                        kRoofR, kRoofG, kRoofB,
                        0.0f, 0.0f, 1.0f);
        }
        for (std::size_t i = 1; i + 1 < N; ++i) {
            b_idx.insert(b_idx.end(),
                         {r0,
                          r0 + static_cast<unsigned>(i),
                          r0 + static_cast<unsigned>(i + 1)});
        }
    }

    if (tile_buildings_vao_ != 0) {
        glDeleteBuffers(1, &tile_buildings_vbo_);
        glDeleteBuffers(1, &tile_buildings_ebo_);
        glDeleteVertexArrays(1, &tile_buildings_vao_);
        tile_buildings_vao_ = 0;
    }
    if (!b_verts.empty()) {
        tile_buildings_vao_ = make_vao_pcn(b_verts, b_idx,
                                            tile_buildings_vbo_, tile_buildings_ebo_);
        tile_buildings_index_count_ = static_cast<GLsizei>(b_idx.size());
    } else {
        tile_buildings_index_count_ = 0;
    }

    // Roads — offset polyline into a quad strip at z=0.05
    constexpr float kRoadR = 0.94f, kRoadG = 0.84f, kRoadB = 0.30f;

    std::vector<float> r_verts;
    std::vector<unsigned> r_idx;
    for (const RoadFeature& rf : tile.roads) {
        if (rf.line.size() < 2) continue;
        const float w = static_cast<float>(rf.width_m * 0.5);
        const std::size_t M = rf.line.size();
        std::vector<glm::vec2> tang(M, glm::vec2{0.0f});
        // segment tangents
        for (std::size_t i = 0; i + 1 < M; ++i) {
            const glm::vec2 t{
                static_cast<float>(rf.line[i + 1].first  - rf.line[i].first),
                static_cast<float>(rf.line[i + 1].second - rf.line[i].second)
            };
            tang[i] = glm::length(t) > 1e-3f ? glm::normalize(t) : glm::vec2{1.0f, 0.0f};
        }
        tang.back() = tang[M - 2];

        const unsigned start = static_cast<unsigned>(r_verts.size() / 9);
        for (std::size_t i = 0; i < M; ++i) {
            // average tangent at vertex
            glm::vec2 t = tang[i];
            if (i > 0) t = glm::normalize(t + tang[i - 1]);
            const glm::vec2 n{-t.y, t.x};
            const float lx = static_cast<float>(rf.line[i].first  - tile_origin_.x);
            const float ly = static_cast<float>(rf.line[i].second - tile_origin_.y);
            push_vertex(r_verts, lx + n.x * w, ly + n.y * w, 0.05f,
                        kRoadR, kRoadG, kRoadB,
                        0.0f, 0.0f, 1.0f);
            push_vertex(r_verts, lx - n.x * w, ly - n.y * w, 0.05f,
                        kRoadR, kRoadG, kRoadB,
                        0.0f, 0.0f, 1.0f);
        }
        for (std::size_t i = 0; i + 1 < M; ++i) {
            const unsigned a = start + static_cast<unsigned>(i * 2);
            const unsigned b = a + 1;
            const unsigned c = a + 2;
            const unsigned d = a + 3;
            r_idx.insert(r_idx.end(), {a, b, c, b, d, c});
        }
    }
    if (tile_roads_vao_ != 0) {
        glDeleteBuffers(1, &tile_roads_vbo_);
        glDeleteBuffers(1, &tile_roads_ebo_);
        glDeleteVertexArrays(1, &tile_roads_vao_);
        tile_roads_vao_ = 0;
    }
    if (!r_verts.empty()) {
        tile_roads_vao_ = make_vao_pcn(r_verts, r_idx,
                                        tile_roads_vbo_, tile_roads_ebo_);
        tile_roads_index_count_ = static_cast<GLsizei>(r_idx.size());
    } else {
        tile_roads_index_count_ = 0;
    }

    tile_loaded_ = true;
}

void Renderer::draw_tile() {
    if (!tile_loaded_) return;

    mesh_shader_->use();
    bind_view_proj_uniforms(*mesh_shader_);
    mesh_shader_->set_vec3("u_sun_dir", 0.40f, 0.55f, 0.85f);

    if (tile_roads_index_count_ > 0) {
        glBindVertexArray(tile_roads_vao_);
        glDrawElements(GL_TRIANGLES, tile_roads_index_count_, GL_UNSIGNED_INT, nullptr);
    }
    if (tile_buildings_index_count_ > 0) {
        glBindVertexArray(tile_buildings_vao_);
        glDrawElements(GL_TRIANGLES, tile_buildings_index_count_, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

void Renderer::ensure_balloon_mesh() {
    if (balloon_vao_ != 0) return;
    // Octahedron-ish balloon: 6 vertices + 8 faces. Compact and works as
    // proof-of-concept; replaced by tinyobjloader output in a follow-up.
    constexpr float r = 4.0f;
    const float pos[6][3] = {
        { 0.0f,  0.0f,  r}, { 0.0f,  0.0f, -r},
        { r,     0.0f, 0.0f}, {-r,    0.0f, 0.0f},
        { 0.0f,  r,    0.0f}, { 0.0f, -r,   0.0f}
    };
    const unsigned tri[8][3] = {
        {0,2,4},{0,4,3},{0,3,5},{0,5,2},
        {1,4,2},{1,3,4},{1,5,3},{1,2,5}
    };
    std::vector<float> v;
    std::vector<unsigned> idx;
    for (int t = 0; t < 8; ++t) {
        const auto& tr = tri[t];
        glm::vec3 a{pos[tr[0]][0], pos[tr[0]][1], pos[tr[0]][2]};
        glm::vec3 b{pos[tr[1]][0], pos[tr[1]][1], pos[tr[1]][2]};
        glm::vec3 c{pos[tr[2]][0], pos[tr[2]][1], pos[tr[2]][2]};
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        unsigned base = static_cast<unsigned>(v.size() / 9);
        push_vertex(v, a.x, a.y, a.z, 1, 1, 1, n.x, n.y, n.z);
        push_vertex(v, b.x, b.y, b.z, 1, 1, 1, n.x, n.y, n.z);
        push_vertex(v, c.x, c.y, c.z, 1, 1, 1, n.x, n.y, n.z);
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }
    balloon_vao_ = make_vao_pcn(v, idx, balloon_vbo_, balloon_ebo_);
    balloon_index_count_ = static_cast<GLsizei>(idx.size());
}

void Renderer::add_balloon(const std::string& id, const glm::dvec3& wp,
                             const glm::vec3& color) {
    ensure_balloon_mesh();
    balloons_.push_back({id, wp, color});
}

void Renderer::remove_balloon(const std::string& id) {
    balloons_.erase(std::remove_if(balloons_.begin(), balloons_.end(),
                                    [&](const Balloon& b){ return b.id == id; }),
                     balloons_.end());
}

void Renderer::draw_balloons() {
    if (balloons_.empty() || balloon_vao_ == 0) return;
    int w = 0, h = 0;
    ctx_.framebuffer_size(w, h);
    const float aspect = (h <= 0) ? 1.0f : static_cast<float>(w) / static_cast<float>(h);
    const glm::mat4 proj = camera_.projection(aspect);
    mesh_shader_->use();
    mesh_shader_->set_mat4("u_proj", glm::value_ptr(proj));
    mesh_shader_->set_vec3("u_sun_dir", 0.40f, 0.55f, 0.85f);
    glBindVertexArray(balloon_vao_);
    for (const auto& b : balloons_) {
        const glm::dvec3 cam_d = camera_.position - b.world_pos;
        const glm::vec3  cam_l{cam_d};
        glm::mat4 view = camera_.view_local();
        view = glm::translate(view, -cam_l);
        // Tint by uniform color via the per-vertex color already 1,1,1; we
        // re-scale via light intensity. Apply color through the matrix is
        // overkill; instead, push per-balloon color via mesh_shader uniform
        // would require new uniform — skip for prototype, all balloons use
        // the palette default.
        mesh_shader_->set_mat4("u_view", glm::value_ptr(view));
        glDrawElements(GL_TRIANGLES, balloon_index_count_, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

void Renderer::append_trail(const glm::dvec3& wp) {
    trail_pts_.push_back(wp);
    rebuild_trail_buffer();
}

void Renderer::clear_trail() {
    trail_pts_.clear();
    trail_vertex_count_ = 0;
}

void Renderer::rebuild_trail_buffer() {
    if (trail_pts_.size() < 2) {
        trail_vertex_count_ = 0;
        return;
    }
    if (trail_vao_ == 0) {
        glGenVertexArrays(1, &trail_vao_);
        glGenBuffers(1, &trail_vbo_);
    }
    std::vector<float> v;
    v.reserve(trail_pts_.size() * 6);
    for (const auto& p : trail_pts_) {
        const float lx = static_cast<float>(p.x - tile_origin_.x);
        const float ly = static_cast<float>(p.y - tile_origin_.y);
        const float lz = static_cast<float>(p.z) + 0.2f;
        v.insert(v.end(), {lx, ly, lz, 0.20f, 0.95f, 0.55f});
    }
    trail_vertex_count_ = static_cast<GLsizei>(trail_pts_.size());
    glBindVertexArray(trail_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, trail_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(v.size() * sizeof(float)),
                 v.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::draw_trail() {
    if (trail_vertex_count_ < 2) return;
    line_shader_->use();
    int w = 0, h = 0;
    ctx_.framebuffer_size(w, h);
    const float aspect = (h <= 0) ? 1.0f : static_cast<float>(w) / static_cast<float>(h);
    const glm::mat4 proj = camera_.projection(aspect);
    const glm::dvec3 cam_d = camera_.position - tile_origin_;
    const glm::vec3 cam_l{cam_d};
    glm::mat4 view = camera_.view_local();
    view = glm::translate(view, -cam_l);
    line_shader_->set_mat4("u_view", glm::value_ptr(view));
    line_shader_->set_mat4("u_proj", glm::value_ptr(proj));
    glBindVertexArray(trail_vao_);
    glDrawArrays(GL_LINE_STRIP, 0, trail_vertex_count_);
    glBindVertexArray(0);
}

void Renderer::end_frame() {
    ctx_.swap_buffers();
}

}  // namespace mv::render::gl
