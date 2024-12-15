# pragma once

#include <SDL3/SDL_opengles2.h>
#include <memory>

struct Shader {
    GLuint program = 0;
    GLuint vertex = 0;
    GLuint fragment = 0;

    void use(); // glUseProgram
    GLint get_loc(const char *name); // glGetUniformLocation
};

using ShaderPtr = std::unique_ptr<Shader, void(*)(Shader*)>;

ShaderPtr make_shader(const char *vertex_code, const char *fragment_code);
