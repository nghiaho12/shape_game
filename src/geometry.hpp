#pragma once

#include <vector>
#include <map>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <SDL3/SDL_opengles2.h>

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};

// Wrapper for GL_TRIANGLES
struct GLPrimitive {
    GLuint vbo_vertex;
    GLuint vbo_index;
    int index_count;
    glm::vec4 color;
    glm::vec2 trans{0.0f, 0.0f};
    float scale = 1.0f;
    float theta = 0.0f; // rotation in radians
};

struct Shape {
    float radius;
    int rotation_direction = 1;

    GLPrimitive line;
    GLPrimitive line_highlight;
    GLPrimitive fill;

    void set_trans(const glm::vec2 &trans) {
        line.trans = trans;
        line_highlight.trans = trans;
        fill.trans = trans;
    }

    void set_scale(float scale) {
        line.scale = scale;
        line_highlight.scale = scale;
        fill.scale = scale;
    }

    void set_theta(float theta) {
        line.theta = theta;
        line_highlight.theta = theta;
        fill.theta = theta;
    }
};

// Low level functions to create polygon vertex and index.
std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius);
VertexIndex make_fill(const std::vector<glm::vec2> &vert);
VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness);

GLPrimitive make_gl_primitive(const VertexIndex &vi, const glm::vec4 &color);
void free_gl_primitive(GLPrimitive &p);
void draw_gl_primitive(GLuint program, const GLPrimitive &p);

Shape make_shape(int sides, const std::vector<float> &radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color);

// Create all shapes for the duration of the game.
// All shapes are normalized to radius of 1.0 unit and scaled according to screen size.
std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const std::map<std::string, glm::vec4> &palette);
