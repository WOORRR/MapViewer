#include "render/opengl/GLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>

namespace mv::render::gl {

namespace {

void glfw_error_cb(int code, const char* desc) {
    std::fprintf(stderr, "[GLFW %d] %s\n", code, desc);
}

void GLAPIENTRY gl_debug_cb(GLenum /*src*/, GLenum type, GLuint /*id*/,
                             GLenum severity, GLsizei /*len*/,
                             const GLchar* msg, const void* /*user*/) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }
    std::fprintf(stderr, "[GL type=%#x sev=%#x] %s\n", type, severity, msg);
}

bool g_glfw_inited = false;

}  // namespace

Context::Context(const ContextOptions& opts) {
    if (!g_glfw_inited) {
        glfwSetErrorCallback(glfw_error_cb);
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("glfwInit failed");
        }
        g_glfw_inited = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    if (opts.debug_context) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    }

    window_ = glfwCreateWindow(opts.width, opts.height,
                                opts.title.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        throw std::runtime_error("glfwCreateWindow failed (need GL 4.6 core)");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(opts.swap_interval);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        throw std::runtime_error("gladLoadGLLoader failed");
    }

    if (opts.debug_context) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_cb, nullptr);
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
}

Context::~Context() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    if (g_glfw_inited) {
        glfwTerminate();
        g_glfw_inited = false;
    }
}

bool Context::should_close() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Context::swap_buffers() {
    glfwSwapBuffers(window_);
}

void Context::poll_events() {
    glfwPollEvents();
}

void Context::framebuffer_size(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

}  // namespace mv::render::gl
