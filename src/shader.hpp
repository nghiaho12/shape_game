#pragma once

#include <SDL3/SDL_opengles2.h>
#include <cstring>

bool compile_shader(GLuint s, const char *shader);

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

