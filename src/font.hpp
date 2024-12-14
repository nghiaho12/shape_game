#pragma once

#include <SDL3/SDL_opengles2.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <utility>
#include "geometry.hpp"

struct FontAtlas {
    GLuint program;
    GLuint v_shader;
    GLuint f_shader;

    GLuint tex;
    int tex_w;
    int tex_h;

    GLPrimitive letter;

    // hardcoded values from running:
    // wine msdf-atlas-gen.exe -font /usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf -type msdf -fontname ubuntu_mono -uniformcols 10 -imageout atlas.bmp
    static const int cols = 10;
    static const int rows = 10;
    static const int grid_w = 21;
    static const int grid_h = 35;
    static constexpr float distance_range = 2.0f;
    static constexpr float glyph_size = 32.875f;

    bool load_font(const char *bmp_path);
};

std::pair<glm::vec2, glm::vec2> get_char_uv(const FontAtlas &font, char ch);
void draw_letter(const FontAtlas &font, float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, char ch);
