#pragma once

#include <SDL3/SDL_opengles2.h>
#include <glm/ext/vector_float4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include <map>
#include "gl_helper.hpp"

// How to render the Glyph
// Plane is offset relative to cursor pos
// Atlas is bounding box in the texture atlas
struct Glyph {
    float advance; // em, x increment
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
    int grid_width;
    int grid_height;
    std::map<int, Glyph> glyph;
    float distance_scale; // scales up distance_range

    bool load(const std::string &atlas_path, const std::string &atlas_txt);

    void set_target_width(float pixel);
    void set_trans(const glm::vec2 &trans); // vertex shader
    void set_fg(const glm::vec4 &color) const;
    void set_bg(const glm::vec4 &color) const;
    void set_outline(const glm::vec4 &color) const;
    void set_outline_factor(float factor) const;

    std::pair<glm::vec2, glm::vec2> get_char_uv(char ch);

    // for static text
    std::vector<glm::vec4> make_letter(float x, float y, char ch);
    std::pair<std::vector<glm::vec4>, std::vector<uint32_t>> make_text_vertex(const std::string &str);
    VertexBufferPtr make_text(const std::string &str);

    // For drawing text on the fly
    void draw_letter(float x, float y, char ch);
    void draw_string(float x, float y, const std::string &str);
};

