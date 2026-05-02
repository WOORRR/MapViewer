#include "modules/render/RenderModule.h"

#include "core/MessageBus.h"
#include "modules/ui/UiModule.h"

#include <GLFW/glfw3.h>
#include "geo/Picking.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>

namespace mv::modules {

RenderModule::RenderModule()
    : ModuleBase("render"),
      renderer_(std::make_unique<render::gl::Renderer>()) {
    last_published_pos_   = renderer_->camera().position;
    last_published_yaw_   = renderer_->camera().yaw_deg;
    last_published_pitch_ = renderer_->camera().pitch_deg;
    init_imgui();
}

RenderModule::~RenderModule() {
    shutdown_imgui();
}

void RenderModule::on_init() {
    bus_->subscribe<CameraMovedMsg>(this);
    bus_->subscribe<CameraFovChangedMsg>(this);
    bus_->subscribe<MapTileLoadedMsg>(this);
    bus_->subscribe<ObjectAddedMsg>(this);
    bus_->subscribe<LocationFixMsg>(this);
    bus_->subscribe<ConfigUpdatedMsg>(this);
    bus_->subscribe<ShutdownMsg>(this);
}

void RenderModule::on_message(const AnyMessage& msg) {
    if (auto* m = std::get_if<CameraMovedMsg>(&msg)) {
        renderer_->camera().position = glm::dvec3{m->pos_x, m->pos_y, m->pos_z};
        renderer_->camera().yaw_deg   = static_cast<float>(m->yaw_deg);
        renderer_->camera().pitch_deg = static_cast<float>(m->pitch_deg);
        last_published_pos_   = renderer_->camera().position;
        last_published_yaw_   = renderer_->camera().yaw_deg;
        last_published_pitch_ = renderer_->camera().pitch_deg;
    } else if (auto* f = std::get_if<CameraFovChangedMsg>(&msg)) {
        renderer_->camera().fov_delta_deg = f->fov_deg;
        last_published_fov_ = f->fov_deg;
    } else if (auto* tile = std::get_if<MapTileLoadedMsg>(&msg)) {
        renderer_->load_tile(*tile, building_height_m_);
    } else if (auto* obj = std::get_if<ObjectAddedMsg>(&msg)) {
        renderer_->add_balloon(obj->instance_id,
                                glm::dvec3{obj->world_x, obj->world_y, obj->world_z},
                                glm::vec3{1.0f, 0.6f, 0.2f});
    } else if (auto* fix = std::get_if<LocationFixMsg>(&msg)) {
        renderer_->append_trail(glm::dvec3{fix->utmk_x, fix->utmk_y, 0.0});
    } else if (auto* cfg = std::get_if<ConfigUpdatedMsg>(&msg)) {
        if (cfg->key == "render") {
            try {
                auto j = nlohmann::json::parse(cfg->json_value);
                if (j.contains("building_fixed_height_m")) {
                    building_height_m_ = j["building_fixed_height_m"].get<float>();
                }
            } catch (...) { /* ignore malformed */ }
        }
    }
}

bool RenderModule::should_close() const {
    return renderer_->context().should_close();
}

void RenderModule::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // Korean font fallback: try Malgun Gothic from Windows fonts.
    const char* malgun = "C:/Windows/Fonts/malgun.ttf";
    ImFontConfig fc;
    fc.MergeMode = false;
    fc.OversampleH = 2;
    fc.OversampleV = 2;
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, 0x3131, 0x318F, 0xAC00, 0xD7AF, 0
    };
    if (FILE* f = std::fopen(malgun, "rb"); f != nullptr) {
        std::fclose(f);
        io.Fonts->AddFontFromFileTTF(malgun, 16.0f, &fc, ranges);
    }

    ImGui_ImplGlfw_InitForOpenGL(renderer_->context().window(), true);
    ImGui_ImplOpenGL3_Init("#version 460");
    imgui_inited_ = true;
}

void RenderModule::shutdown_imgui() {
    if (!imgui_inited_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imgui_inited_ = false;
}

void RenderModule::frame() {
    const double now = glfwGetTime();
    const float dt = (last_frame_s_ == 0.0) ? 1.0f / 60.0f
                                            : static_cast<float>(now - last_frame_s_);
    last_frame_s_ = now;

    drain_all();

    // Start new frame for ImGui first so its WantCapture flags can suppress
    // game input when typing in the CLI box.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse) {
        handle_input(dt);

        // left-click pick edge detect
        static int prev_lmb = GLFW_RELEASE;
        const int lmb = glfwGetMouseButton(renderer_->context().window(),
                                            GLFW_MOUSE_BUTTON_LEFT);
        if (lmb == GLFW_PRESS && prev_lmb == GLFW_RELEASE) {
            double mx = 0.0, my = 0.0;
            glfwGetCursorPos(renderer_->context().window(), &mx, &my);
            run_pick(static_cast<int>(mx), static_cast<int>(my));
        }
        prev_lmb = lmb;
    }
    check_collisions(now);

    renderer_->begin_frame();
    renderer_->draw_grid();
    renderer_->draw_tile();
    renderer_->draw_balloons();
    renderer_->draw_trail();

    draw_ui_overlay();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    renderer_->end_frame();
    render::gl::Context::poll_events();
}

void RenderModule::draw_ui_overlay() {
    auto& cam = renderer_->camera();

    // Camera info + FoV slider
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Camera (UTM-K)", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("X: %.1f m", cam.position.x);
        ImGui::Text("Y: %.1f m", cam.position.y);
        ImGui::Text("Z: %.1f m", cam.position.z);
        ImGui::Separator();
        ImGui::Text("Yaw: %.1f°  Pitch: %.1f°", cam.yaw_deg, cam.pitch_deg);
        int fov = cam.fov_delta_deg;
        if (ImGui::SliderInt("FoV delta (°)", &fov,
                              render::Camera::kFovDeltaMin,
                              render::Camera::kFovDeltaMax)) {
            cam.fov_delta_deg = fov;
            CameraFovChangedMsg m; m.fov_deg = fov;
            bus_->publish(m);
            last_published_fov_ = fov;
        }
        ImGui::Text("Effective FoV: %.1f°", cam.effective_fov_deg());
    }
    ImGui::End();

    // CLI box (bottom)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(12, vp->WorkPos.y + vp->WorkSize.y - 70),
                             ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 24, 60), ImGuiCond_Always);
    if (ImGui::Begin("CLI", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted("> ");
        ImGui::SameLine();
        if (ImGui::InputText("##cli", cli_buffer_, sizeof(cli_buffer_),
                              ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (ui_ != nullptr && cli_buffer_[0] != '\0') {
                ui_->submit_cli(cli_buffer_);
            }
            cli_buffer_[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(goto/teleport/fov/load_obj/undo/redo/trail)");
    }
    ImGui::End();

    // Toasts
    if (ui_ != nullptr) {
        const auto toasts = ui_->active_toasts();
        if (!toasts.empty()) {
            ImGui::SetNextWindowPos(ImVec2(vp->WorkSize.x - 360, 12), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
            if (ImGui::Begin("Notifications", nullptr,
                              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
                for (const auto& t : toasts) {
                    ImGui::TextWrapped("%s", t.text.c_str());
                }
            }
            ImGui::End();
        }
        // Pick info
        const auto pk = ui_->current_pick();
        if (!pk.kind.empty()) {
            ImGui::SetNextWindowPos(ImVec2(vp->WorkSize.x - 360,
                                            vp->WorkSize.y - 240), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(340, 160), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Pick info", nullptr, ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("kind: %s", pk.kind.c_str());
                ImGui::Text("id:   %s", pk.id.c_str());
                ImGui::Separator();
                ImGui::TextWrapped("%s", pk.props_json.c_str());
            }
            ImGui::End();
        }
    }
}

void RenderModule::handle_input(float dt_s) {
    GLFWwindow* w = renderer_->context().window();
    auto& cam = renderer_->camera();

    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
    }

    const float move_speed = (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 200.0f : 50.0f;
    const float turn_speed = 60.0f;

    glm::vec3 forward = cam.forward();
    forward.z = 0.0f;
    if (glm::length(forward) > 1e-4f) forward = glm::normalize(forward);
    const glm::vec3 right_v = cam.right();

    glm::dvec3 delta{0.0};
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) delta += glm::dvec3{forward * (move_speed * dt_s)};
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) delta -= glm::dvec3{forward * (move_speed * dt_s)};
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) delta += glm::dvec3{right_v * (move_speed * dt_s)};
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) delta -= glm::dvec3{right_v * (move_speed * dt_s)};
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) delta.z += move_speed * dt_s;
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) delta.z -= move_speed * dt_s;
    cam.position += delta;

    if (glfwGetKey(w, GLFW_KEY_LEFT)  == GLFW_PRESS) cam.yaw_deg -= turn_speed * dt_s;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) cam.yaw_deg += turn_speed * dt_s;
    if (glfwGetKey(w, GLFW_KEY_UP)    == GLFW_PRESS) cam.pitch_deg += turn_speed * dt_s;
    if (glfwGetKey(w, GLFW_KEY_DOWN)  == GLFW_PRESS) cam.pitch_deg -= turn_speed * dt_s;

    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(w, &mx, &my);
        if (!mouse_initialized_) {
            last_mouse_x_ = mx; last_mouse_y_ = my;
            mouse_initialized_ = true;
        }
        const float sens = 0.15f;
        cam.yaw_deg   += static_cast<float>(mx - last_mouse_x_) * sens;
        cam.pitch_deg -= static_cast<float>(my - last_mouse_y_) * sens;
        last_mouse_x_ = mx; last_mouse_y_ = my;
    } else {
        mouse_initialized_ = false;
    }

    cam.pitch_deg = glm::clamp(cam.pitch_deg, -89.0f, 89.0f);

    static int prev_lb = GLFW_RELEASE, prev_rb = GLFW_RELEASE;
    const int lb = glfwGetKey(w, GLFW_KEY_LEFT_BRACKET);
    const int rb = glfwGetKey(w, GLFW_KEY_RIGHT_BRACKET);
    if (lb == GLFW_PRESS && prev_lb == GLFW_RELEASE) {
        cam.fov_delta_deg = std::max(render::Camera::kFovDeltaMin, cam.fov_delta_deg - 1);
    }
    if (rb == GLFW_PRESS && prev_rb == GLFW_RELEASE) {
        cam.fov_delta_deg = std::min(render::Camera::kFovDeltaMax, cam.fov_delta_deg + 1);
    }
    prev_lb = lb; prev_rb = rb;

    constexpr double kPosEps   = 0.05;
    constexpr float  kAngleEps = 0.1f;
    if (glm::distance(cam.position, last_published_pos_) > kPosEps ||
        std::abs(cam.yaw_deg   - last_published_yaw_)   > kAngleEps ||
        std::abs(cam.pitch_deg - last_published_pitch_) > kAngleEps) {
        CameraMovedMsg msg;
        msg.pos_x = cam.position.x; msg.pos_y = cam.position.y; msg.pos_z = cam.position.z;
        msg.yaw_deg = cam.yaw_deg;  msg.pitch_deg = cam.pitch_deg; msg.roll_deg = 0.0;
        bus_->publish(msg);
        last_published_pos_ = cam.position;
        last_published_yaw_ = cam.yaw_deg;
        last_published_pitch_ = cam.pitch_deg;
    }
    if (cam.fov_delta_deg != last_published_fov_) {
        CameraFovChangedMsg msg;
        msg.fov_deg = cam.fov_delta_deg;
        bus_->publish(msg);
        last_published_fov_ = cam.fov_delta_deg;
    }
}

void RenderModule::run_pick(int sx, int sy) {
    auto& cam = renderer_->camera();
    int sw = 0, sh = 0;
    renderer_->context().framebuffer_size(sw, sh);
    if (sw <= 0 || sh <= 0) return;

    const float aspect = static_cast<float>(sw) / static_cast<float>(sh);
    const glm::mat4 proj = cam.projection(aspect);
    const glm::mat4 view = cam.view_local();
    const auto ray = mv::geo::screen_ray(sx, sy, sw, sh, view, proj, cam.position);

    // Buildings: closest hit AABB
    std::string best_id;
    double best_t = std::numeric_limits<double>::infinity();
    const render::gl::BuildingHit* best_b = nullptr;
    for (const auto& b : renderer_->buildings()) {
        double t = 0.0;
        if (mv::geo::ray_aabb(ray, b.aabb, t) && t < best_t) {
            best_t = t;
            best_id = b.id;
            best_b = &b;
        }
    }

    PickResultMsg pr;
    if (best_b != nullptr) {
        pr.kind = PickResultMsg::Kind::Building;
        pr.id   = best_b->id;
        const auto& f = best_b->feature;
        nlohmann::json j;
        j["id"]         = f.id;
        j["addr_road"]  = f.addr_road;
        j["addr_lot"]   = f.addr_lot;
        j["bdtyp_cd"]   = f.bdtyp_cd;
        j["buld_nm"]    = f.buld_nm;
        j["gro_floors"] = f.gro_floors;
        pr.props_json = j.dump(2);
    } else {
        pr.kind = PickResultMsg::Kind::None;
    }
    bus_->publish(pr);
}

void RenderModule::check_collisions(double now_s) {
    auto& cam = renderer_->camera();
    constexpr double kGroundFloor = 0.5;
    constexpr double kPushOutMargin = 0.5;
    constexpr double kCooldownS = 0.4;

    bool collided = false;
    CameraCollisionMsg pub;

    if (cam.position.z < kGroundFloor) {
        cam.position.z = kGroundFloor;
        pub.with = CameraCollisionMsg::With::Ground;
        pub.corrected_x = cam.position.x;
        pub.corrected_y = cam.position.y;
        pub.corrected_z = cam.position.z;
        collided = true;
    }

    for (const auto& b : renderer_->buildings()) {
        if (!b.aabb.contains(cam.position, -0.05)) continue;
        const double dx_l = cam.position.x - b.aabb.min.x;
        const double dx_r = b.aabb.max.x - cam.position.x;
        const double dy_b = cam.position.y - b.aabb.min.y;
        const double dy_t = b.aabb.max.y - cam.position.y;
        const double dz_t = b.aabb.max.z - cam.position.z;
        double best = dx_l; int dir = 0;
        if (dx_r < best) { best = dx_r; dir = 1; }
        if (dy_b < best) { best = dy_b; dir = 2; }
        if (dy_t < best) { best = dy_t; dir = 3; }
        if (dz_t < best) { best = dz_t; dir = 4; }
        switch (dir) {
            case 0: cam.position.x = b.aabb.min.x - kPushOutMargin; break;
            case 1: cam.position.x = b.aabb.max.x + kPushOutMargin; break;
            case 2: cam.position.y = b.aabb.min.y - kPushOutMargin; break;
            case 3: cam.position.y = b.aabb.max.y + kPushOutMargin; break;
            case 4: cam.position.z = b.aabb.max.z + kPushOutMargin; break;
            default: break;
        }
        pub.with        = CameraCollisionMsg::With::Building;
        pub.building_id = b.id;
        pub.corrected_x = cam.position.x;
        pub.corrected_y = cam.position.y;
        pub.corrected_z = cam.position.z;
        collided = true;
        break;
    }

    if (collided) {
        const std::string key = (pub.with == CameraCollisionMsg::With::Ground)
                                 ? std::string("__ground__") : pub.building_id;
        if (key != last_collision_id_ || now_s - last_collision_publish_s_ > kCooldownS) {
            bus_->publish(pub);
            last_collision_publish_s_ = now_s;
            last_collision_id_ = key;
            std::fprintf(stderr, "[collision] with=%s pos=(%.1f, %.1f, %.1f)\n",
                         key.c_str(), cam.position.x, cam.position.y, cam.position.z);
        }
    } else if (!last_collision_id_.empty() && now_s - last_collision_publish_s_ > kCooldownS) {
        last_collision_id_.clear();
    }
}

}  // namespace mv::modules
