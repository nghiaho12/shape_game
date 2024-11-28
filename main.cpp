#define SDL_MAIN_USE_CALLBACKS 1 // use the callbacks instead of main() 
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp> 
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <cmath>

struct Shape {
    int sides;
    glm::vec2 center;
    float radius;
    float line_thickness;
    glm::vec4 line_color;
    glm::vec4 fill_color;

    GLuint vbo_vertex;
    std::vector<uint32_t> vertex_index;
};

struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;

    bool shader_init = false;
    GLuint v_shader;
    GLuint f_shader;
    GLuint program;
    GLuint vao;

    std::vector<Shape> shape;
};

static const char *vertex_shader = R"(
#version 330

layout(location = 0) in vec2 position;
uniform vec2 center;
uniform vec4 color;
uniform mat4 projection_matrix;
out vec4 v_color;

void main() {
    gl_Position = projection_matrix * vec4(position + center, 0.0, 1.0);
    v_color = color;
})";

static const char *fragment_shader = R"(
#version 330

in vec4 v_color;
out vec4 o_color;

void main() {
    o_color = v_color;
})";

void GLAPIENTRY gl_debug_callback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

Shape create_shape(int sides, float radius, float line_thickness) {
    Shape shape;

    shape.sides = sides;
    shape.radius = radius;
    shape.line_thickness = line_thickness;

    std::vector<glm::vec2> vert;

    for (int i=0; i < sides; i++) {
        float theta = i * 2*M_PI / sides;
        float x = radius*std::cos(theta);
        float y = radius*std::sin(theta);

        vert.push_back(glm::vec2{x, y});
    }

    // quads for each line
    std::vector<glm::vec2> tri_pts;
    std::vector<uint32_t> tri_idx;
    std::vector<glm::vec2> inner, outer;

    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        glm::vec2 v = vert[j] - vert[i];
        v /= glm::length(v);

        // normal to the line
        glm::vec2 n{-v[1], v[0]};

        tri_idx.push_back(tri_pts.size() + 0);
        tri_idx.push_back(tri_pts.size() + 1);
        tri_idx.push_back(tri_pts.size() + 2);

        tri_idx.push_back(tri_pts.size() + 0);
        tri_idx.push_back(tri_pts.size() + 2);
        tri_idx.push_back(tri_pts.size() + 3);

        tri_pts.push_back(vert[i] + n*line_thickness*0.5f);
        tri_pts.push_back(vert[j] + n*line_thickness*0.5f);
        tri_pts.push_back(vert[j] - n*line_thickness*0.5f);
        tri_pts.push_back(vert[i] - n*line_thickness*0.5f);

        outer.push_back(vert[i] + n*line_thickness*0.5f);
        outer.push_back(vert[j] + n*line_thickness*0.5f);
        inner.push_back(vert[j] - n*line_thickness*0.5f);
        inner.push_back(vert[i] - n*line_thickness*0.5f);
    }

     // miter
    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        for (const auto &pts: {inner, outer}) {
            glm::vec2 p0 = pts[i*2];
            glm::vec2 p1 = pts[i*2 + 1];
            glm::vec2 p2 = pts[j*2];
            glm::vec2 p3 = pts[j*2 + 1];

            glm::vec2 v0 = p1 - p0;
            glm::vec2 v1 = p3 - p2;

            // find intersection of 2 lines
            // Ax = b
            // A = [v0 | -v1]
            // b = p2 - p0
            // use Cramer's rule
           
            glm::vec2 b = p2 - p0;

            double det = v0[0]*(-v1[1]) - (-v1[0]*v0[1]);
            double c0 =  b[0]*(-v1[1]) - (-v1[0]*b[1]);
            float t0 = c0 / det;

            glm::vec2 pp = p0 + v0*t0;

            tri_idx.push_back(tri_pts.size() + 0);
            tri_idx.push_back(tri_pts.size() + 1);
            tri_idx.push_back(tri_pts.size() + 2);

            tri_idx.push_back(tri_pts.size() + 0);
            tri_idx.push_back(tri_pts.size() + 1);
            tri_idx.push_back(tri_pts.size() + 3);

            tri_pts.push_back(vert[j]);
            tri_pts.push_back(pp);
            tri_pts.push_back(p1);
            tri_pts.push_back(p2);
        }
    }

    glGenBuffers(1, &shape.vbo_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*tri_pts.size(), tri_pts.data(), GL_STATIC_DRAW);

    shape.vertex_index = tri_idx;

    return shape;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    AppState *as = new AppState();
    
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("shape", 640, 480, SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
        return SDL_APP_FAILURE;
    }    

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(gl_debug_callback, 0);

    as->v_shader = glCreateShader(GL_VERTEX_SHADER);
    as->f_shader = glCreateShader(GL_FRAGMENT_SHADER);

    int length = strlen(vertex_shader);
    glShaderSource(as->v_shader, 1, static_cast<const GLchar**>(&vertex_shader), &length );
    glCompileShader(as->v_shader);

    GLint status;
    glGetShaderiv(as->v_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        std::cerr << "vertex shader compilation failed\n";
        return SDL_APP_FAILURE;
    }

    length = strlen(fragment_shader);
    glShaderSource(as->f_shader, 1, static_cast<const GLchar**>(&fragment_shader), &length);
    glCompileShader(as->f_shader);

    glGetShaderiv(as->f_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        std::cerr << "fragment shader compilation failed\n";
        return SDL_APP_FAILURE;
    }

    as->program = glCreateProgram();
    glAttachShader(as->program, as->v_shader);
    glAttachShader(as->program, as->f_shader);
    glLinkProgram(as->program);

    glGenVertexArrays(1, &as->vao);
    glBindVertexArray(as->vao);

    Shape s = create_shape(5, 100.f, 10.f);
    s.center = glm::vec2{320.f, 240.f};
    s.line_color = glm::vec4{1.f, 1.f, 1.f, 1.f};

    as->shape.push_back(s);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
    }
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate) {
        AppState *as = static_cast<AppState*>(appstate);
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);

        delete as;
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as = static_cast<AppState*>(appstate);

    int w, h;
    if (!SDL_GetWindowSize(as->window, &w, &h)) {
        std::cerr << SDL_GetError() << "\n";
        return SDL_APP_FAILURE;
    }

    glViewport(0, 0, w, h);
    glm::mat4 ortho = glm::ortho(0.0f, w*1.0f, h*1.0f, 0.0f);

    glUseProgram(as->program);
    glUniformMatrix4fv(glGetUniformLocation(as->program, "projection_matrix"), 1, GL_FALSE, &ortho[0][0]);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.2, 0.2, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(as->vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    for (const auto &s: as->shape) {
        glUniform4fv(glGetUniformLocation(as->program, "color"), 1, &s.line_color[0]);
        glUniform2fv(glGetUniformLocation(as->program, "center"), 1, &s.center[0]);

        glBindBuffer(GL_ARRAY_BUFFER, s.vbo_vertex);
        glDrawElements(GL_TRIANGLES, s.vertex_index.size(), GL_UNSIGNED_INT, s.vertex_index.data());
    }

    SDL_GL_SwapWindow(as->window);

    return SDL_APP_CONTINUE;
}
