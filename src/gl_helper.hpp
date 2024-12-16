#pragma once

#include <SDL3/SDL_opengles2.h>
#include <memory>
#include <vector>
#include <glm/vec2.hpp>

// Wrapper around common OpenGL types.
// These will clear memory on exit.

struct Shader {
    GLuint program = 0;
    GLuint vertex = 0;
    GLuint fragment = 0;

    void use(); // glUseProgram
    GLint get_loc(const char *name); // glGetUniformLocation
};

using ShaderPtr = std::unique_ptr<Shader, void(*)(Shader*)>;
ShaderPtr make_shader(const char *vertex_code, const char *fragment_code);

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;

    void use();
};

using TexturePtr = std::unique_ptr<Texture, void(*)(Texture*)>;
TexturePtr make_texture(const char *bmp_path);

struct VertexBuffer {
    GLuint vertex = 0;
    GLuint index = 0;
    int index_count = 0;

    void bind();
    void draw();
    void update_vertex(const std::vector<glm::vec2> &v);
    void update_vertex(const std::vector<float> &v);
};

using VertexBufferPtr = std::unique_ptr<VertexBuffer, void(*)(VertexBuffer*)>;
VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec2> &vertex, const std::vector<uint32_t> &index);
VertexBufferPtr make_vertex_buffer(const std::vector<float> &vertex, const std::vector<uint32_t> &index);
VertexBufferPtr make_vertex_buffer(const float *vertex, int vertex_count, const std::vector<uint32_t> &index);

// Misc
void enable_gl_debug_callback();
