#pragma once

#include <SDL3/SDL_opengles2.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include "gl_helper.hpp"

struct FontAtlas {
    FontAtlas() : 
        shader({}, {}),
        tex({}, {}),
        letter({}, {}) {
    }

    ShaderPtr shader;
    TexturePtr tex;
    VertexBufferPtr letter;

    // hardcoded values from running:
    // wine msdf-atlas-gen.exe -font /usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf -type msdf -fontname ubuntu_mono -uniformcols 10 -imageout atlas.bmp
    static const int cols = 10;
    static const int rows = 10;
    static const int grid_w = 21;
    static const int grid_h = 35;
    static constexpr float distance_range = 2.0f;
    static constexpr float glyph_size = 32.875f;

    bool load(const char *bmp_path);
    std::pair<glm::vec2, glm::vec2> get_char_uv(char ch);
    void draw_letter(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, char ch);
    void draw_string(float x, float y, float scale, const glm::vec4 &fg, const glm::vec4 &bg, const std::string &str);
};

