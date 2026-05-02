#pragma once

#include <string>

struct GLFWwindow;

namespace mv::render::gl {

struct ContextOptions {
    int width{1280};
    int height{800};
    std::string title{"MapViewer"};
    bool debug_context{true};
    int  swap_interval{1};
};

class Context {
public:
    explicit Context(const ContextOptions& opts = {});
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    GLFWwindow* window() const { return window_; }
    bool        should_close() const;
    void        swap_buffers();
    static void poll_events();

    void framebuffer_size(int& width, int& height) const;

private:
    GLFWwindow* window_{nullptr};
};

}  // namespace mv::render::gl
