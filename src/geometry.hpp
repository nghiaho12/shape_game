#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <SDL3/SDL_opengles2.h>

#include "gl_helper.hpp"

// Wrapper for GL_TRIANGLES
struct GLPrimitive {
    GLPrimitive() : 
        vertex_buffer({}, {}) {
    }

    VertexBufferPtr vertex_buffer;

    glm::vec4 color{};
    glm::vec2 trans{};
    float scale = 1.0f;
    float theta = 0.0f; // rotation in radians

    void draw(const ShaderPtr &shader);
};

struct Shape {
    float radius;
    int rotation_direction = 1;

    GLPrimitive line;
    GLPrimitive line_highlight;
    GLPrimitive fill;

    void set_trans(const glm::vec2 &trans);
    void set_scale(float scale);
    void set_theta(float theta);
};

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};

ShaderPtr make_shape_shader();

// Create all possible shapes for the game.
// All shapes are normalized to radius of 1.0 unit and scaled according to screen size.
std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const std::map<std::string, glm::vec4> &palette);

