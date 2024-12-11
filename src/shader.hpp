#pragma once

#include <SDL3/SDL_opengles2.h>
#include <cstring>
#include <vector>
#include "debug.hpp"

static const char *vertex_shader = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec2 position;
uniform float scale;
uniform float theta;
uniform vec2 trans;
uniform vec4 color;
uniform mat4 projection_matrix;
out vec4 v_color;

void main() {
    float c = cos(theta);
    float s = sin(theta);
    mat2 R = mat2(c, s, -s, c);

    gl_Position = projection_matrix * vec4(R*position*scale + trans, 0.0, 1.0);
    v_color = color;
})";

static const char *fragment_shader = R"(#version 300 es
precision mediump float;

in vec4 v_color;
out vec4 o_color;

void main() {
    o_color = v_color;
})";

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
            debug("compile_shder error: %s", error.data());
        }

        return false;
    }

    return true;
}

