#include "render/opengl/GLShader.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace mv::render::gl {

namespace {

GLuint compile_one(GLenum stage, std::string_view src) {
    const GLuint sh = glCreateShader(stage);
    const char* ptr = src.data();
    const GLint len = static_cast<GLint>(src.size());
    glShaderSource(sh, 1, &ptr, &len);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint log_len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log(static_cast<std::size_t>(log_len + 1), '\0');
        glGetShaderInfoLog(sh, log_len, nullptr, log.data());
        glDeleteShader(sh);
        throw std::runtime_error(std::string("shader compile failed: ") + log.data());
    }
    return sh;
}

}  // namespace

Shader::Shader(std::string_view vertex_src, std::string_view fragment_src) {
    const GLuint vs = compile_one(GL_VERTEX_SHADER,   vertex_src);
    const GLuint fs = compile_one(GL_FRAGMENT_SHADER, fragment_src);

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint log_len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log(static_cast<std::size_t>(log_len + 1), '\0');
        glGetProgramInfoLog(program_, log_len, nullptr, log.data());
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program_);
        program_ = 0;
        throw std::runtime_error(std::string("shader link failed: ") + log.data());
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

void Shader::set_mat4(const char* name, const float* value) const {
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, value);
}

void Shader::set_vec3(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(program_, name), x, y, z);
}

void Shader::set_float(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(program_, name), v);
}

}  // namespace mv::render::gl
