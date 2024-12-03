#include "geometry.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <random>

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

GLPrimitive make_gl_primitive(const VertexIndex &vi, const glm::vec4 &color) {
    GLPrimitive ret;

    glGenBuffers(1, &ret.vbo_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, ret.vbo_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*vi.vertex.size(), vi.vertex.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ret.vbo_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret.vbo_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::vec2)*vi.index.size(), vi.index.data(), GL_STATIC_DRAW);

    ret.index_count = vi.index.size();
    ret.color = color;

    return ret;
}

void free_gl_primitive(GLPrimitive &p) {
    if (p.vbo_vertex) {
        glDeleteBuffers(1, &p.vbo_vertex);
        p.vbo_vertex = 0;
    }

    if (p.vbo_index) {
        glDeleteBuffers(1, &p.vbo_index);
        p.vbo_index= 0;
    }
}

void draw_gl_primitive(GLuint program, const GLPrimitive &p) {
    glUniform1f(glGetUniformLocation(program, "scale"), p.scale);
    glUniform1f(glGetUniformLocation(program, "theta"), p.theta);
    glUniform2fv(glGetUniformLocation(program, "trans"), 1, &p.trans[0]);
    glUniform4fv(glGetUniformLocation(program, "color"), 1, &p.color[0]);

    glBindBuffer(GL_ARRAY_BUFFER, p.vbo_vertex);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.vbo_index);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawElements(GL_TRIANGLES, p.index_count, GL_UNSIGNED_INT, 0);
}
  
Shape make_shape(int sides, const std::vector<float> &radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color) {
    Shape shape;

    shape.radius = *std::max_element(radius.begin(), radius.end());

    std::vector<glm::vec2> vert = make_polygon(sides, radius);

    shape.fill = make_gl_primitive(make_fill(vert), fill_color);
    shape.line = make_gl_primitive(make_line(vert, line_thickness), line_color);

    return shape;
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

    shape.fill = make_gl_primitive(make_fill(vert), fill_color);
    shape.line = make_gl_primitive(make_line(vert, line_thickness), line_color);

    return shape;
}

std::vector<Shape> make_shape_set(const glm::vec4 &line_color, const std::map<std::string, glm::vec4> &palette) {
    std::vector<Shape> ret;

    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_real_distribution<float> dice_radius(0.4, 1.0);

    int idx = 0;
    auto next_Color = [&]() -> glm::vec4 {
        auto it = palette.begin();
        std::advance(it, idx);
        idx = (idx + 1) % palette.size();

        return it->second;
    };

    for (int sides=3; sides <= 8; sides++) {
        Shape s = make_shape(sides, {1.f}, 0.1f, line_color, next_Color());
        ret.push_back(s);
    }

    Shape circle = make_shape(36, {1.f}, 0.1f, line_color, next_Color());
    ret.push_back(circle);

    Shape oval = make_oval(1.f, 0.1f, line_color, next_Color());
    ret.push_back(oval);

    Shape star = make_shape(10, {1.0f, 0.5f}, 0.1f, line_color, next_Color());
    ret.push_back(star);

    Shape rhombus = make_shape(4, {1.0f, 0.5f}, 0.1f, line_color, next_Color());
    ret.push_back(rhombus);

    Shape crystal = make_shape(5, {1.0f, 0.5f, 0.5f, 0.5f, 0.5f}, 0.1f, line_color, next_Color());
    ret.push_back(crystal);

    return ret;
}

