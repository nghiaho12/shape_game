#define GL_GLEXT_PROTOTYPES
#include "gl_helper.hpp"
#include "log.hpp"
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_opengles2.h>
#include <memory>
#include <vector>

namespace {
bool compile_shader(GLuint s, const char *shader) {
    int length = strlen(shader);
    glShaderSource(s, 1, static_cast<const GLchar**>(&shader), &length);
    glCompileShader(s);

    GLint status;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);

        std::vector<GLchar> error(len);
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
    assert(ret >= 0);
    return ret;
}

ShaderPtr make_shader(const char *vertex_code, const char *fragment_code) {
    auto cleanup = [](Shader *s) {
        LOG("deleting shader");
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
        LOG("deleting texture");
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
    return make_vertex_buffer(&vertex[0].x, 2*vertex.size(), index);
}

VertexBufferPtr make_vertex_buffer(const std::vector<float> &vertex, const std::vector<uint32_t> &index) {
    return make_vertex_buffer(vertex.data(), vertex.size(), index);
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*vertex_count, vertex, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &v->index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, v->index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*index.size(), index.data(), GL_STATIC_DRAW);

    v->index_count = index.size();

    return v;
}

void VertexBuffer::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index);
}

void VertexBuffer::draw() const {
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
}

void VertexBuffer::update_vertex(const std::vector<glm::vec2> &v) const {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec2)*v.size(), v.data());
}

void VertexBuffer::update_vertex(const std::vector<float> &v) const {
    glBindBuffer(GL_ARRAY_BUFFER, vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*v.size(), v.data());
}
