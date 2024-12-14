#include <SDL3/SDL_surface.h>

#include "font.hpp"
#include "log.hpp"
#include "shader.hpp"

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
    v_shader = glCreateShader(GL_VERTEX_SHADER);
    f_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(v_shader, font_vertex_shader)) {
        LOG("failed to compile font vertex shader");
        return false;
    }

    if (!compile_shader(f_shader, font_fragment_shader)) {
        LOG("failed to compile font fragment shader");
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, v_shader);
    glAttachShader(program, f_shader);
    glLinkProgram(program);

    SDL_Surface *bmp = SDL_LoadBMP(bmp_path);
    if (!bmp) {
        LOG("Failed to loat font atlas: %s", bmp_path);
        return false;
    }

    tex_w = bmp->w;
    tex_h = bmp->h;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmp->w, bmp->h, 0, GL_RGB, GL_UNSIGNED_BYTE, bmp->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_DestroySurface(bmp);

    std::vector<float> empty_vert(16);
    std::vector<uint32_t> vbo_index{0, 1, 2, 0, 2, 3};
    letter.index_count = vbo_index.size();

    glGenBuffers(1, &letter.vbo_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, letter.vbo_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*empty_vert.size(), empty_vert.data(), GL_DYNAMIC_DRAW);

    glGenBuffers(1, &letter.vbo_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, letter.vbo_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*vbo_index.size(), vbo_index.data(), GL_STATIC_DRAW);

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
    glm::vec2 end = start + glm::vec2{1.f*grid_w/tex_w, 1.f*grid_h/tex_h};

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

    glUseProgram(program);
    glUniform4fv(glGetUniformLocation(program, "bgColor"), 1, &bg[0]);
    glUniform4fv(glGetUniformLocation(program, "fgColor"), 1, &fg[0]);
    glUniform1i(glGetUniformLocation(program, "msdf"), 0); // set it manually
    glUniform1f(glGetUniformLocation(program, "screenPxRange"), distance_range * scale); // set it manually

    glBindBuffer(GL_ARRAY_BUFFER, letter.vbo_vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*vert.size(), vert.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, letter.vbo_index);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawElements(GL_TRIANGLES, letter.index_count, GL_UNSIGNED_INT, 0);
}


