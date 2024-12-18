#define GL_GLEXT_PROTOTYPES
#include "gl_helper.hpp"
#include "log.hpp"
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_opengles2.h>
#include <memory>
#include <vector>

namespace {
bool compile_shader(GLuint s, const char *shader) {
    GLint length = static_cast<GLint>(strlen(shader));
    glShaderSource(s, 1, static_cast<const GLchar**>(&shader), &length);
    glCompileShader(s);

    GLint status = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);

        std::vector<GLchar> error(static_cast<size_t>(len));
        glGetShaderInfoLog(s, len, &len, error.data());

        if (len > 0) {
            LOG("compile_shader error: %s", error.data());
        }

        return false;
    }

    return true;
}

void debug_callback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam) {
    LOG("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR_KHR ? "** GL ERROR **" : "" ),
            type, severity, message );

    // Unused
    (void)(source);
    (void)(id);
    (void)(length);
    (void)(userParam);
}

} // namespace

void enable_gl_debug_callback() {
    glEnable(GL_DEBUG_OUTPUT_KHR);
    glDebugMessageCallbackKHR(debug_callback, 0);
}

void Shader::use() const {
    glUseProgram(program);
}

GLint Shader::get_loc(const char *name) const {
    GLint ret = glGetUniformLocation(program, name);
    // assert(ret >= 0);
    return ret;
}

ShaderPtr make_shader(const char *vertex_code, const char *fragment_code) {
    auto cleanup = [](Shader *s) {
        LOG("deleting shader: %d %d %d", s->program, s->vertex, s->fragment);
        glDeleteShader(s->vertex);
        glDeleteShader(s->fragment);
        glDeleteProgram(s->program);
    };

    ShaderPtr s(new Shader, cleanup);

    s->program = glCreateProgram();
    s->vertex = glCreateShader(GL_VERTEX_SHADER);
    s->fragment = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(s->vertex, vertex_code)) {
        LOG("failed to compile vertex shader");
        return {{}, cleanup};
    }

    if (!compile_shader(s->fragment, fragment_code)) {
        LOG("failed to compile fragment shader");
        return {{}, cleanup};
    }

    glAttachShader(s->program, s->vertex);
    glAttachShader(s->program, s->fragment);
    glLinkProgram(s->program);
    
    return s;
}

TexturePtr make_texture(const std::string &bmp_path) {
    SDL_Surface *bmp = SDL_LoadBMP(bmp_path.c_str());
    if (!bmp) {
        LOG("Failed to loat font atlas: %s", bmp_path.c_str());
        return {{}, {}};
    }

    auto cleanup = [](Texture *t) {
        LOG("deleting texture: %d", t->id);
        glDeleteTextures(1, &t->id);
    };
                   
    TexturePtr t(new Texture, cleanup);

    t->width = bmp->w;
    t->height = bmp->h;

    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmp->w, bmp->h, 0, GL_RGB, GL_UNSIGNED_BYTE, bmp->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_DestroySurface(bmp);

    return t;
}

void Texture::use() const {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
}

VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec2> &vertex, const std::vector<uint32_t> &index) {
    return make_vertex_buffer(&vertex[0].x, static_cast<int>(2*vertex.size()), index);
}

VertexBufferPtr make_vertex_buffer(const std::vector<glm::vec4> &vertex, const std::vector<uint32_t> &index) {
    return make_vertex_buffer(&vertex[0].x, static_cast<int>(4*vertex.size()), index);
}

VertexBufferPtr make_vertex_buffer(const float *vertex, int vertex_count, const std::vector<uint32_t> &index) {
    auto cleanup = [](VertexBuffer *v) {
        LOG("deleting vertex and index buffer: %d %d", v->vertex, v->index);
        glDeleteBuffers(1, &v->vertex);
        glDeleteBuffers(1, &v->index);
    };

    VertexBufferPtr v(new VertexBuffer, cleanup);

    glGenBuffers(1, &v->vertex);
    glBindBuffer(GL_ARRAY_BUFFER, v->vertex);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float)*static_cast<uint32_t>(vertex_count)), vertex, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &v->index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, v->index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(uint32_t)*index.size()), index.data(), GL_STATIC_DRAW);

    v->index_count = static_cast<int>(index.size());

    return v;
}

void VertexBuffer::use() const {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index);
}

void VertexBuffer::update_vertex(const std::vector<glm::vec2> &v) const {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec2)*v.size()), v.data());
}

void VertexBuffer::update_vertex(const std::vector<glm::vec4> &v, const std::vector<uint32_t> &optional_idx) {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(glm::vec4)*v.size()), v.data());

    if (!optional_idx.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, index);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(optional_idx.size()), optional_idx.data());
        index_count = static_cast<int>(optional_idx.size());
    }
}

void draw_with_texture(const ShaderPtr &shader, const TexturePtr &tex, const VertexBufferPtr &v) {
    shader->use();
    tex->use();
    v->use();

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    int stride = sizeof(float) * 4;
    int uv_offset = sizeof(float) * 2;

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(uv_offset));

    glDrawElements(GL_TRIANGLES, v->index_count, GL_UNSIGNED_INT, 0);
}

std::pair<glm::vec2, glm::vec2> bbox(const std::vector<glm::vec4> &vertex) {
    float x0 = vertex[0].x;
    float x1 = vertex[0].x;
    float y0 = vertex[0].y;
    float y1 = vertex[0].y;

    for (const auto &v: vertex) {
        x0 = std::min(x0, v.x);
        x1 = std::max(x1, v.x);

        y0 = std::min(y0, v.y);
        y1 = std::max(y1, v.y);
    }

    return {glm::vec2{x0, y0}, glm::vec2{x1, y1}};
}
