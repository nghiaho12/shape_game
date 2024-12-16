#include "geometry.hpp"
#include "gl_helper.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace {
const char *vertex_shader = R"(#version 300 es
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

const char *fragment_shader = R"(#version 300 es
precision mediump float;

in vec4 v_color;
out vec4 o_color;

void main() {
    o_color = v_color;
})";

} // namespace 
  // :
std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius) {
    std::vector<glm::vec2> vert;

    for (int i=0; i < sides; i++) {
        float theta = i * 2*M_PI / sides;

        float r = radius[(i % radius.size())];
        float x = r*std::cos(theta);
        float y = r*std::sin(theta);

        vert.push_back(glm::vec2{x, y});
    }

    return vert;
}

VertexIndex make_fill(const std::vector<glm::vec2> &vert) {
    std::vector<glm::vec2> fill_vert;
    std::vector<uint32_t> fill_idx;

    fill_vert = vert;
    fill_vert.push_back(glm::vec2{0.0f, 0.0f}); // cener of shape

    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        fill_idx.push_back(i);
        fill_idx.push_back(j);
        fill_idx.push_back(fill_vert.size() - 1);
    }

    return {fill_vert, fill_idx};
}

VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness) {
    // quads for each line
    std::vector<glm::vec2> tri_pts;
    std::vector<uint32_t> tri_idx;
    std::vector<glm::vec2> inner, outer;

    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        glm::vec2 v = vert[j] - vert[i];
        v /= glm::length(v);

        // normal to the line
        glm::vec2 n{-v[1], v[0]};

        tri_idx.push_back(tri_pts.size() + 0);
        tri_idx.push_back(tri_pts.size() + 1);
        tri_idx.push_back(tri_pts.size() + 2);

        tri_idx.push_back(tri_pts.size() + 0);
        tri_idx.push_back(tri_pts.size() + 2);
        tri_idx.push_back(tri_pts.size() + 3);

        tri_pts.push_back(vert[i] + n*thickness*0.5f);
        tri_pts.push_back(vert[j] + n*thickness*0.5f);
        tri_pts.push_back(vert[j] - n*thickness*0.5f);
        tri_pts.push_back(vert[i] - n*thickness*0.5f);

        outer.push_back(vert[i] + n*thickness*0.5f);
        outer.push_back(vert[j] + n*thickness*0.5f);
        inner.push_back(vert[j] - n*thickness*0.5f);
        inner.push_back(vert[i] - n*thickness*0.5f);
    }

     // miter
    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        for (const auto &pts: {inner, outer}) {
            glm::vec2 p0 = pts[i*2];
            glm::vec2 p1 = pts[i*2 + 1];
            glm::vec2 p2 = pts[j*2];
            glm::vec2 p3 = pts[j*2 + 1];

            glm::vec2 v0 = p1 - p0;
            glm::vec2 v1 = p3 - p2;

            // find intersection of 2 lines
            // Ax = b
            // A = [v0 | -v1]
            // b = p2 - p0
            // use Cramer's rule
           
            glm::vec2 b = p2 - p0;

            double det = v0[0]*(-v1[1]) - (-v1[0]*v0[1]);
            double c0 =  b[0]*(-v1[1]) - (-v1[0]*b[1]);
            float t0 = c0 / det;

            glm::vec2 pp = p0 + v0*t0;

            tri_idx.push_back(tri_pts.size() + 0);
            tri_idx.push_back(tri_pts.size() + 1);
            tri_idx.push_back(tri_pts.size() + 2);

            tri_idx.push_back(tri_pts.size() + 0);
            tri_idx.push_back(tri_pts.size() + 1);
            tri_idx.push_back(tri_pts.size() + 3);

            tri_pts.push_back(vert[j]);
            tri_pts.push_back(pp);
            tri_pts.push_back(p1);
            tri_pts.push_back(p2);
        }
    }

    return {tri_pts, tri_idx};
}

Shape make_shape(const std::vector<glm::vec2> &vert, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color) {
    Shape shape;
    {
    auto[vertex, index] = make_fill(vert);
    shape.fill.vertex_buffer = make_vertex_buffer(vertex, index);
    shape.fill.color = fill_color;
    }

    {
    auto[vertex, index] = make_line(vert, line_thickness);
    shape.line.vertex_buffer = make_vertex_buffer(vertex, index);
    shape.line.color = line_color;
    }

    {
    auto[vertex, index] = make_line(vert, line_thickness*2);
    shape.line_highlight.vertex_buffer = make_vertex_buffer(vertex, index);
    shape.line_highlight.color = line_color;
    }

    return shape;
}

Shape make_shape_polygon(int sides, const std::vector<float> &radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color) {
    std::vector<glm::vec2> vert = make_polygon(sides, radius);

    Shape s= make_shape(vert, line_thickness, line_color, fill_color);
    s.radius = *std::max_element(radius.begin(), radius.end());

    return s;
}

Shape make_oval(float radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color) {
    Shape shape;

    shape.radius = radius;

    std::vector<glm::vec2> vert;

    int sides = 36;
    for (int i=0; i < sides; i++) {
        float theta = i * 2*M_PI / sides;

        float x = radius*std::cos(theta);
        float y = radius*0.5*std::sin(theta);

        vert.push_back(glm::vec2{x, y});
    }

    return make_shape(vert, line_thickness, line_color, fill_color);
}

  
void GLPrimitive::draw(const ShaderPtr &shader) {
    shader->use();

    glUniform1f(shader->get_loc("scale"), scale);
    glUniform1f(shader->get_loc("theta"), theta);
    glUniform2fv(shader->get_loc("trans"), 1, &trans[0]);
    glUniform4fv(shader->get_loc("color"), 1, &color[0]);

    vertex_buffer->bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    vertex_buffer->draw();
}
  
std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const std::map<std::string, glm::vec4> &palette) {
    // All shapes have a normalized radius of 1.0 unit
    
    constexpr float line_thickness = 0.1f; // normalize

    std::vector<Shape> ret;

    int idx = 0;
    auto next_color = [&]() -> glm::vec4 {
        auto it = palette.begin();
        std::advance(it, idx);
        idx = (idx + 1) % palette.size();

        return it->second;
    };

    for (int sides=3; sides <= 8; sides++) {
        Shape s = make_shape_polygon(sides, {1.f}, line_thickness, line_color, next_color());
        ret.push_back(std::move(s));
    }

    Shape circle = make_shape_polygon(36, {1.f}, line_thickness, line_color, next_color());
    ret.push_back(std::move(circle));

    Shape oval = make_oval(1.f, line_thickness, line_color, next_color());
    ret.push_back(std::move(oval));

    for (int i=0; i < 3; i++) {
        Shape star = make_shape_polygon(8 + i*2, {1.0f, 0.5f}, line_thickness, line_color, next_color());
        ret.push_back(std::move(star));
    }

    Shape rhombus = make_shape_polygon(4, {1.0f, 0.5f}, line_thickness, line_color, next_color());
    ret.push_back(std::move(rhombus));

    Shape crystal = make_shape_polygon(5, {1.0f, 0.5f, 0.5f, 0.5f, 0.5f}, line_thickness, line_color, next_color());
    ret.push_back(std::move(crystal));

    return ret;
}

ShaderPtr make_shape_shader() {
    return make_shader(vertex_shader, fragment_shader);
}

void Shape::set_trans(const glm::vec2 &trans) {
    line.trans = trans;
    line_highlight.trans = trans;
    fill.trans = trans;
}

void Shape::set_scale(float scale) {
    line.scale = scale;
    line_highlight.scale = scale;
    fill.scale = scale;
}

void Shape::set_theta(float theta) {
    line.theta = theta;
    line_highlight.theta = theta;
    fill.theta = theta;
}
