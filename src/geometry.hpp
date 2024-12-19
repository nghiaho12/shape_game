#pragma once

#include <SDL3/SDL_opengles2.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <map>
#include <string>
#include <vector>

#include "gl_helper.hpp"

// Wrapper for GL_TRIANGLES
struct ShapePrimitive {
    VertexBufferPtr vertex_buffer{{}, {}};

    glm::vec4 color{};
    glm::vec2 trans{};
    float scale = 1.0f;
    float theta = 0.0f;  // rotation in radians

    void draw(const ShaderPtr &shader);
};

struct Shape {
    float radius = 1.0f;
    float rotation_direction = 1.f;

    ShapePrimitive line;
    ShapePrimitive line_highlight;
    ShapePrimitive fill;

    void set_trans(const glm::vec2 &trans);
    void set_scale(float scale);
    void set_theta(float theta);
};

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};

ShaderPtr make_shape_shader();

// Create all possible shapes for the game
// All shapes are normalized to radius of 1.0 unit
std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const std::map<std::string, glm::vec4> &palette);

std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius);
VertexIndex make_fill(const std::vector<glm::vec2> &vert);
VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness);
Shape make_shape(const std::vector<glm::vec2> &vert,
                 float line_thickness,
                 const glm::vec4 &line_color,
                 const glm::vec4 &fill_color);
Shape make_shape_polygon(int sides,
                         const std::vector<float> &radius,
                         float line_thickness,
                         const glm::vec4 &line_color,
                         const glm::vec4 &fill_color);
Shape make_oval(float radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color);
