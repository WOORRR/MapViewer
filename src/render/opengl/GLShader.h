#pragma once

#include <glad/glad.h>

#include <string>
#include <string_view>

namespace mv::render::gl {

class Shader {
public:
    Shader(std::string_view vertex_src, std::string_view fragment_src);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const { glUseProgram(program_); }
    GLuint program() const { return program_; }

    void set_mat4(const char* name, const float* value) const;
    void set_vec3(const char* name, float x, float y, float z) const;
    void set_float(const char* name, float v) const;

private:
    GLuint program_{0};
};

}  // namespace mv::render::gl
