#pragma once

#include "core/MessageBus.h"
#include "core/ModuleBase.h"
#include "render/opengl/GLRenderer.h"

#include <memory>

namespace mv::modules {

class UiModule;

// Main-thread module that owns the GL context plus the ImGui overlay. Each
// frame:
//   1) drain queued messages targeting the renderer
//   2) poll keyboard for camera movement (WASD/QE) and FoV ([/])
//   3) re-publish CameraMovedMsg/CameraFovChangedMsg if state changed
//   4) render the scene + UI
class RenderModule : public ModuleBase {
public:
    RenderModule();
    ~RenderModule() override;

    // Bind the UI data source so we can read toast/pick state inside ImGui.
    void set_ui_module(UiModule* ui) { ui_ = ui; }

    // Must be called every frame from the main thread until should_close()
    // returns true.
    void frame();
    bool should_close() const;

protected:
    void on_init() override;
    void on_message(const AnyMessage& msg) override;

private:
    void handle_input(float dt_s);
    void check_collisions(double now_s);
    void draw_ui_overlay();
    void init_imgui();
    void shutdown_imgui();
    void run_pick(int screen_x, int screen_y);

    std::unique_ptr<render::gl::Renderer> renderer_;
    UiModule* ui_{nullptr};
    bool imgui_inited_{false};
    char cli_buffer_[256]{0};
    float building_height_m_{10.0f};
    double last_collision_publish_s_{0.0};
    std::string last_collision_id_;
    double last_frame_s_{0.0};
    // right-click mouse-look state
    double last_mouse_x_{0.0};
    double last_mouse_y_{0.0};
    bool   mouse_initialized_{false};

    // middle-click pan state
    double last_pan_x_{0.0};
    double last_pan_y_{0.0};
    bool   pan_initialized_{false};

    // map tile request tracking (trigger re-fetch when camera moves > threshold)
    glm::dvec3 last_tile_center_{953900.0, 1952030.0, 0.0};

    // Building click popup
    bool        show_pick_popup_{false};
    std::string pick_popup_name_;
    std::string pick_popup_addr_;
    std::string pick_popup_json_;

    int    last_published_fov_{-15};
    glm::dvec3 last_published_pos_{953900.0, 1952030.0, 200.0};
    float  last_published_yaw_{0.0f};
    float  last_published_pitch_{-90.0f};
};

}  // namespace mv::modules
