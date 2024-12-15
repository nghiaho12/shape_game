#include <SDL3/SDL_surface.h>

#include "font.hpp"
#include "gl_helper.hpp"

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
uniform vec4 bgColor;
uniform vec4 fgColor;
uniform float screenPxRange;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(msdf, texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    color = mix(bgColor, fgColor, opacity);
})";
}

bool FontAtlas::load(const char *bmp_path) {
    shader = make_shader(font_vertex_shader, font_fragment_shader);

    if (!shader) {
        return false;
    }

    tex = make_texture(bmp_path);

    if (!tex) {
        return false;
    }

    // quad
    std::vector<float> empty_vert(16);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};
    letter = make_vertex_buffer(empty_vert, index);

    return true;
}

std::pair<glm::vec2, glm::vec2> FontAtlas::get_char_uv(char ch) {
    // Assume character set from ascii 33 to 126
    int idx = ch - 33;

    int row = idx / cols;
    int col = idx % cols;

    float u = 1.f*col / cols;
    float v = 1.f*row / rows;

    glm::vec2 start{u, v};
    glm::vec2 end = start + glm::vec2{1.f*grid_w/tex->width, 1.f*grid_h/tex->height};

    return {start, end};
}

void FontAtlas::draw_letter(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, char ch) {
    auto [start, end] = get_char_uv(ch);

    float w = grid_w * scale;
    float h = grid_h * scale;

    // pos + uv
    std::vector<float> vert{
        x, y, start.x, start.y,
        x + w, y,  end.x, start.y,
        x + w, y + h,  end.x, end.y,
        x, y + h, start.x, end.y,
    };

    shader->use();
    glUniform4fv(shader->get_loc("bgColor"), 1, &bg[0]);
    glUniform4fv(shader->get_loc("fgColor"), 1, &fg[0]);
    glUniform1i(shader->get_loc("msdf"), 0); 
    glUniform1f(shader->get_loc("screenPxRange"), distance_range * scale); 

    letter->update_vertex(vert);
    letter->bind();

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2 * sizeof(float)));

    tex->use();
    letter->draw();
}


