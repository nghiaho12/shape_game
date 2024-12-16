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

    int distance_range; // signed distance field range in pixels
    float em_size; // pixels per em unit
    int grid_width; // pixels
    int grid_height; // pixels
    int grid_cols;
    int grid_rows;
    float advance[256]; // font x advance in em units
    float scale;

    bool load(const std::string &atlas_path, const std::string &atlas_txt);
    void set_scale(float scale);
    void set_fg(const glm::vec4 &color) const;
    void set_bg(const glm::vec4 &color) const;
    void set_outline(const glm::vec4 &color) const;
    void set_outline_factor(float factor) const;

    std::pair<glm::vec2, glm::vec2> get_char_uv(char ch);
    void draw_letter(float x, float y, char ch);
    void draw_string(float x, float y, const std::string &str);
};

