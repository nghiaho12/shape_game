#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <SDL3/SDL_opengles2.h>

// Wrapper for GL_TRIANGLES
struct GLPrimitive {
    GLuint vbo_vertex;
    GLuint vbo_index;
    int index_count;
    glm::vec4 color;
    glm::vec2 trans{0.0f, 0.0f};
    float scale = 1.0f;
};

struct Shape {
    glm::vec2 center;
    float radius;
    float line_thickness;

    GLPrimitive line;
    GLPrimitive fill;
};

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};

std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius, float theta_offset=0.0f);
VertexIndex make_fill(const std::vector<glm::vec2> &vert);
VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness);

GLPrimitive make_gl_primitive(const VertexIndex &vi, const glm::vec4 &color);
void free_gl_primitive(GLPrimitive &p);
void draw_gl_primitive(GLuint program, const GLPrimitive &p);
Shape make_shape( int sides, const std::vector<float> &radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color, float theta_offset=0.0f);
std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const glm::vec4 &fill_color);
