#include <vector>
#include "shader.hpp"
#include "log.hpp"

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

