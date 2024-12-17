#include <SDL3/SDL_surface.h>
#include <memory>
#include <fstream>

#include "font.hpp"
#include "gl_helper.hpp"
#include "log.hpp"

namespace {
const char *font_vertex_shader = R"(#version 300 es
precision mediump float;    

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 atlas_tex_coord;

uniform mat4 projection_matrix;
uniform vec2 trans;
uniform float scale;
out vec2 texCoord;

void main() {
    gl_Position = projection_matrix * vec4(position*scale + trans, 0.0, 1.0);
    texCoord = atlas_tex_coord;
})";
    
const char *font_fragment_shader = R"(#version 300 es
precision mediump float;

in vec2 texCoord;
out vec4 color;
uniform sampler2D msdf;
uniform vec4 bg_color;
uniform vec4 fg_color;
uniform vec4 outline_color;
uniform float outline_factor;
uniform float distance_range;
uniform float distance_scale;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(msdf, texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    float screen_px_range = distance_range * distance_scale;
    float dist_px = screen_px_range*(sd - 0.5) + 0.5;
    float outline_dist = screen_px_range*outline_factor;

    if (dist_px > 0.0 && dist_px < 1.0) { 
        // inner and start of outline
        color = mix(outline_color, fg_color, dist_px);
    } else if (dist_px > -outline_dist && dist_px < -outline_dist + 1.0) {
        // end of outline and background
        float opacity = clamp(dist_px + outline_dist, 0.0, 1.0);
        color = mix(bg_color, outline_color, opacity);
    } else if (dist_px > -outline_dist && dist_px < 1.0) {
        color = outline_color;
    } else {
        float opacity = clamp(dist_px, 0.0, 1.0);
        color = mix(bg_color, fg_color, opacity);
    }

    // color = vec4(0.5, 0.5, 0.5, 0.5);
})";
}

bool FontAtlas::load(const std::string &atlas_path, const std::string &atlas_txt) {
    shader = make_shader(font_vertex_shader, font_fragment_shader);

    if (!shader) {
        return false;
    }

    tex = make_texture(atlas_path);

    if (!tex) {
        return false;
    }

    // quad for letter
    std::vector<float> empty_vert(16);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};
    letter = make_vertex_buffer(empty_vert, index);

    std::ifstream in(atlas_txt);
    
    if (!in) {
        LOG("failed to open: %s", atlas_txt.c_str());
        return false;
    }

    std::string label;

    in >> label; assert(label == "distance_range");
    in >> distance_range;
    in >> label; assert(label == "em_size");
    in >> em_size;
    in >> label; assert(label == "grid_width");
    in >> grid_width;
    in >> label; assert(label == "grid_height");
    in >> grid_height;
    in >> label; assert(label == "unicode");

    while (true) {
        int unicode;
        in >> unicode;

        if (in.eof()) {
            break;
        }

        Glyph g;
        in >> g.advance;
        in >> g.plane_left;
        in >> g.plane_bottom;
        in >> g.plane_right;
        in >> g.plane_top;
        in >> g.atlas_left;
        in >> g.atlas_bottom;
        in >> g.atlas_right;
        in >> g.atlas_top;

        glyph[unicode] = g;
    }

    shader->use();
    glUniform1i(shader->get_loc("msdf"), 0); 
    glUniform1f(shader->get_loc("distance_range"), static_cast<float>(distance_range)); 
    glUniform1f(shader->get_loc("scale"), 1.0);
    glUniform2fv(shader->get_loc("trans"), 1, &glm::vec2(0)[0]);

    return true;
}

std::pair<glm::vec2, glm::vec2> FontAtlas::get_char_uv(char ch) {
    const Glyph &g = glyph[static_cast<int>(ch)];

    glm::vec2 start{g.atlas_left/tex->width, 1 - g.atlas_bottom/tex->height};
    glm::vec2 end{g.atlas_right/tex->width, 1  - g.atlas_top/tex->height};

    return {start, end};
}

std::vector<float> FontAtlas::make_letter(float x, float y, char ch) {
    auto [start, end] = get_char_uv(ch);

    const Glyph &g = glyph[static_cast<int>(ch)];

    float w = (g.atlas_right - g.atlas_left);
    float h = (g.atlas_top - g.atlas_bottom);
    float xoff = g.plane_left * em_size;
    float yoff = std::abs(g.plane_bottom) * em_size;

    x += xoff;
    y += yoff;

    // pos + uv
    return std::vector<float> {
        x, y, start.x, start.y,
        x + w, y,  end.x, start.y,
        x + w, y - h,  end.x, end.y,
        x, y - h, start.x, end.y,
    };
}

VertexBufferPtr FontAtlas::make_text(const std::string &str) {
    float xpos = 0;

    std::vector<float> vertex_uv;
    std::vector<uint32_t> index;

    int vertex_count = 0;

    for (char ch: str) {
        auto v = make_letter(xpos, 0, ch);
        vertex_uv.insert(vertex_uv.end(), v.begin(), v.end());

        const Glyph &g = glyph[static_cast<int>(ch)];
        xpos += g.advance*em_size;

        // quad
        std::vector<uint32_t> idx{0, 1, 2, 0, 2, 3};
        for (auto &i : idx) {
            i += vertex_count;
        }
        index.insert(index.end(), idx.begin(), idx.end());

        vertex_count += 4;
    }

    return make_vertex_buffer(vertex_uv, index);
}

void FontAtlas::draw_letter(float x, float y, char ch) {
    std::vector<float> vert = make_letter(x, y, ch);

    shader->use();

    letter->update_vertex(vert);
    letter->use();

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    int stride = sizeof(float) * 4;
    int uv_offset = sizeof(float) * 2;

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(uv_offset));

    tex->use();
    letter->draw();
}

void FontAtlas::draw_string(float x, float y, const std::string &str) {
    float xpos = x;

    for (char ch: str) {
        draw_letter(xpos, y, ch);
        const Glyph &g = glyph[static_cast<int>(ch)];
        xpos += g.advance*em_size*distance_scale;
    }
}

void FontAtlas::set_scale(float scale) {
    shader->use();
    glUniform1f(shader->get_loc("scale"), scale);
}

void FontAtlas::set_trans(const glm::vec2 &trans) {
    shader->use();
    glUniform2fv(shader->get_loc("trans"), 1, &trans[0]);
}

void FontAtlas::set_target_width(float pixel) {
    shader->use();
    float scale = pixel / grid_width;
    glUniform1f(shader->get_loc("scale"), scale);
    glUniform1f(shader->get_loc("distance_scale"), scale);
    this->distance_scale = scale;
}

void FontAtlas::set_fg(const glm::vec4 &color) const {
    shader->use();
    glUniform4fv(shader->get_loc("fg_color"), 1, &color[0]);
}

void FontAtlas::set_bg(const glm::vec4 &color) const {
    shader->use();
    glUniform4fv(shader->get_loc("bg_color"), 1, &color[0]);
}

void FontAtlas::set_outline(const glm::vec4 &color) const {
    shader->use();
    glUniform4fv(shader->get_loc("outline_color"), 1, &color[0]);
}

void FontAtlas::set_outline_factor(float factor) const {
    shader->use();
    glUniform1f(shader->get_loc("outline_factor"), factor);
}
