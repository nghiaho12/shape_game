#pragma once

#include <SDL3/SDL_opengles2.h>
#include <memory>
#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// Light wrapper around common OpenGL types.
// The unique_ptr will delete the OpenGL object automatically.

struct VertexArray {
    GLuint vao;
    void use();
};

using VertexArrayPtr = std::unique_ptr<VertexArray, void(*)(VertexArray*)>;
VertexArrayPtr make_vertex_array();

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

// This is general enough to represent all the drawing combos we need.
// - vertex only
// - vertex + texture uv
// - vertex + color 
struct VertexBuffer {
    GLuint vertex = 0;
    GLuint index = 0;

    size_t vertex_bytes = 0;
    size_t index_count = 0;

    void use() const;
    void update_vertex(const float *v, size_t v_bytes, const std::vector<uint32_t> &optional_index={}); // pos + texture uv
};

using VertexBufferPtr = std::unique_ptr<VertexBuffer, void(*)(VertexBuffer*)>;

VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec2> &vertex, const std::vector<uint32_t> &index);
VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec4> &vertex, const std::vector<uint32_t> &index); // pos + texture uv
VertexBufferPtr make_vertex_buffer(const float *vertex, size_t vertex_bytes, const std::vector<uint32_t> &index); 

void draw_vertex_buffer(const ShaderPtr &shader, const VertexBufferPtr &v, const TexturePtr &optional_tex={{}, {}});

// returns xy bounding box of vertex
std::pair<glm::vec2, glm::vec2> bbox(const std::vector<glm::vec4> &vertex);   

void enable_gl_debug_callback();
