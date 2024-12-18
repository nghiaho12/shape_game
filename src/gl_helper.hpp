#pragma once

#include <SDL3/SDL_opengles2.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// Light wrapper around common OpenGL types.

struct Shader {
    GLuint program = 0;
    GLuint vertex = 0;
    GLuint fragment = 0;

    void use() const; // glUseProgram
    GLint get_loc(const char *name) const; // glGetUniformLocation
};

using ShaderPtr = std::unique_ptr<Shader, void(*)(Shader*)>;
ShaderPtr make_shader(const char *vertex_code, const char *fragment_code);

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;

    void use() const;
};

using TexturePtr = std::unique_ptr<Texture, void(*)(Texture*)>;
TexturePtr make_texture(const std::string &bmp_path);

struct VertexBuffer {
    GLuint vertex = 0;
    GLuint index = 0;
    int index_count = 0;

    void use() const;
    void update_vertex(const std::vector<glm::vec2> &v) const;
    void update_vertex(const std::vector<glm::vec4> &v, const std::vector<uint32_t> &optional_index={}); // pos + texture uv
};

using VertexBufferPtr = std::unique_ptr<VertexBuffer, void(*)(VertexBuffer*)>;

VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec2> &vertex, const std::vector<uint32_t> &index);
VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec4> &vertex, const std::vector<uint32_t> &index); // pos + texture uv
VertexBufferPtr make_vertex_buffer(const float *vertex, int vertex_count, const std::vector<uint32_t> &index); 

void draw_with_texture(const ShaderPtr &shader, const TexturePtr &tex, const VertexBufferPtr &v);
std::pair<glm::vec2, glm::vec2> bbox(const std::vector<glm::vec4> &vertex);   
void enable_gl_debug_callback();

