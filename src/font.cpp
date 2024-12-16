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
out vec2 texCoord;

void main() {
    gl_Position = projection_matrix * vec4(position, 0.0, 1.0);
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
uniform float scale;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(msdf, texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    float screen_px_range = distance_range * scale;
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

    return true;
}

std::pair<glm::vec2, glm::vec2> FontAtlas::get_char_uv(char ch) {
    const Glyph &g = glyph[static_cast<int>(ch)];

    glm::vec2 start{g.atlas_left/tex->width, 1- g.atlas_bottom/tex->height};
    glm::vec2 end{g.atlas_right/tex->width, 1- g.atlas_top/tex->height};

    return {start, end};
}

void FontAtlas::draw_letter(float x, float y, char ch) {
    auto [start, end] = get_char_uv(ch);

    const Glyph &g = glyph[static_cast<int>(ch)];

    float w = (g.atlas_right - g.atlas_left)* scale;
    float h = (g.atlas_top - g.atlas_bottom)* scale;
    float xoff = g.plane_left * em_size * scale;
    float yoff = std::abs(g.plane_bottom) * em_size * scale;

    x += xoff;
    y += yoff;

    // pos + uv
    std::vector<float> vert{
        x, y, start.x, start.y,
        x + w, y,  end.x, start.y,
        x + w, y - h,  end.x, end.y,
        x, y - h, start.x, end.y,
    };

    shader->use();

    letter->update_vertex(vert);
    letter->bind();

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
        xpos += g.advance*em_size*scale;
    }
}

void FontAtlas::set_scale(float scale) {
    shader->use();
    glUniform1f(shader->get_loc("scale"), scale);
    this->scale = scale;
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
