#pragma once

#include <SDL3/SDL_opengles2.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include "gl_helper.hpp"

struct Glyph {
    float advance; // em
    float plane_left; // em
    float plane_bottom;
    float plane_right;
    float plane_top;
    float atlas_left; // pixel 
    float atlas_bottom;
    float atlas_right;
    float atlas_top;
};

struct FontAtlas {
    ShaderPtr shader{{},{}};
    TexturePtr tex{{}, {}};
    VertexBufferPtr letter{{}, {}};

    int distance_range; // signed distance field range in pixels
    float em_size; // pixels per em unit
    Glyph glyph[256] = {}; 
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

