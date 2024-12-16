#pragma once

#include <SDL3/SDL_opengles2.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include "gl_helper.hpp"

struct FontAtlas {
    ShaderPtr shader{{},{}};
    TexturePtr tex{{}, {}};
    VertexBufferPtr letter{{}, {}};

    int distance_range;
    float em_size;
    int grid_width;
    int grid_height;
    int grid_cols;
    int grid_rows;
    float advance[256];

    bool load(const std::string &atlas_path, const std::string &atlas_txt);

    std::pair<glm::vec2, glm::vec2> get_char_uv(char ch);
    void draw_letter(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, const glm::vec4 &outline, char ch);
    void draw_string(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, const glm::vec4 &outline, const std::string &str);
};

