#include "gl_helper.hpp"
#include "log.hpp"
#include <GLES2/gl2.h>
#include <memory>
#include <vector>

namespace {
bool compile_shader(GLuint s, const char *shader) {
    int length = strlen(shader);
    glShaderSource(s, 1, static_cast<const GLchar**>(&shader), &length);
    glCompileShader(s);

    GLint status;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);

        std::vector<GLchar> error(len);
        glGetShaderInfoLog(s, len, &len, error.data());

        if (len > 0) {
            LOG("compile_shader error: %s", error.data());
        }

        return false;
    }

    return true;
}
} // namespace

void Shader::use() {
    glUseProgram(program);
}

GLint Shader::get_loc(const char *name) {
    return glGetUniformLocation(program, name);
}

ShaderPtr make_shader(const char *vertex_code, const char *fragment_code) {
    auto cleanup = [](Shader *s) {
        glDeleteShader(s->vertex);
        glDeleteShader(s->fragment);
        glDeleteProgram(s->program);
    };

    ShaderPtr s(new Shader, cleanup);

    s->program = glCreateProgram();
    s->vertex = glCreateShader(GL_VERTEX_SHADER);
    s->fragment = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(s->vertex, vertex_code)) {
        LOG("failed to compile vertex shader");
        return {{}, cleanup};
    }

    if (!compile_shader(s->fragment, fragment_code)) {
        LOG("failed to compile fragment shader");
        return {{}, cleanup};
    }

    glAttachShader(s->program, s->vertex);
    glAttachShader(s->program, s->fragment);
    glLinkProgram(s->program);
    
    return s;
}

