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
// uniform vec2 trans;

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
uniform float screen_px_range;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    const float outline_factor = 0.1;

    vec3 msd = texture(msdf, texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
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
    in >> label; assert(label == "grid_cols");
    in >> grid_cols;
    in >> label; assert(label == "grid_rows");
    in >> grid_rows;
    in >> label; assert(label == "advance");

    for (int i=0; i < 256; i++) {
        in >> advance[i];
    }

    return true;
}

std::pair<glm::vec2, glm::vec2> FontAtlas::get_char_uv(char ch) {
    // Assume character set from ascii 33 to 126
    int idx = ch - 33;

    int row = idx / grid_cols;
    int col = idx % grid_cols;

    float u = 1.f*col / grid_cols;
    float v = 1.f*row / grid_rows;

    glm::vec2 start{u, v};
    glm::vec2 end = start + glm::vec2{1.f*(grid_width-1)/tex->width, 1.f*(grid_height-1)/tex->height};

    return {start, end};
}

void FontAtlas::draw_letter(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, const glm::vec4 &outline, char ch) {
    auto [start, end] = get_char_uv(ch);

    float w = grid_width * scale;
    float h = grid_height * scale;

    // pos + uv
    std::vector<float> vert{
        x, y, start.x, start.y,
        x + w, y,  end.x, start.y,
        x + w, y + h,  end.x, end.y,
        x, y + h, start.x, end.y,
    };

    shader->use();
    glUniform4fv(shader->get_loc("bg_color"), 1, &bg[0]);
    glUniform4fv(shader->get_loc("fg_color"), 1, &fg[0]);
    glUniform4fv(shader->get_loc("outline_color"), 1, &outline[0]);
    glUniform1i(shader->get_loc("msdf"), 0); 
    glUniform1f(shader->get_loc("screen_px_range"), distance_range * scale); 

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

void FontAtlas::draw_string(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, const glm::vec4 &outline, const std::string &str) {
    float xpos = x;

    for (char ch: str) {
        draw_letter(xpos, y, scale, fg, bg, outline, ch);
        xpos += advance[static_cast<int>(ch)]*em_size*scale;
    }
}
