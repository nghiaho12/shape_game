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
#include <array>

constexpr int NUM_SHAPES = 5;
constexpr float ASPECT_RATIO = 4.0/3.0;

struct Shape {
    int sides;
    glm::vec2 center;
    float radius;
    float line_thickness;
    glm::vec4 line_color;
    glm::vec4 fill_color;

    GLuint vbo_line_vertex;
    GLuint vbo_line_index;
    int line_index_count;

    GLuint vbo_fill_vertex;
    GLuint vbo_fill_index;
    int fill_index_count;
};

struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;

    bool shader_init = false;
    GLuint v_shader;
    GLuint f_shader;
    GLuint program;
    GLuint vao;

    GLuint vbo_bg_vertex;
    GLuint vbo_bg_index;
    GLuint bg_index_count;

    std::array<Shape, NUM_SHAPES> shape;
};

static const char *vertex_shader = R"(
#version 330

layout(location = 0) in vec2 position;
uniform vec2 center;
uniform float scale;
uniform vec4 color;
uniform mat4 projection_matrix;
out vec4 v_color;

void main() {
    gl_Position = projection_matrix * vec4(position*scale + center, 0.0, 1.0);
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

    // triangles to fill in the shape
    std::vector<glm::vec2> fill_pts;
    std::vector<uint32_t> fill_idx;

    fill_pts = vert;
    fill_pts.push_back(glm::vec2{0.0f, 0.0f}); // origin of shape

    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        fill_idx.push_back(i);
        fill_idx.push_back(j);
        fill_idx.push_back(fill_pts.size() - 1);
    }

    glGenBuffers(1, &shape.vbo_fill_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_fill_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*fill_pts.size(), fill_pts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &shape.vbo_fill_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.vbo_fill_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::vec2)*fill_idx.size(), fill_idx.data(), GL_STATIC_DRAW);

    shape.fill_index_count = fill_idx.size();

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

    glGenBuffers(1, &shape.vbo_line_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_line_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*tri_pts.size(), tri_pts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &shape.vbo_line_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.vbo_line_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::vec2)*tri_idx.size(), tri_idx.data(), GL_STATIC_DRAW);

    shape.line_index_count = tri_idx.size();

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
    glDisable(GL_CULL_FACE);

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

    int sides = 3;
    for (auto &s: as->shape) {
        s = create_shape(sides, 1.f, 0.1f);
        s.line_color = glm::vec4{1.f, 1.f, 1.f, 1.f};
        s.fill_color = glm::vec4{0.2f, 0.8f, 0.2f, 1.f};

        sides++;
    }

    glGenBuffers(1, &as->vbo_bg_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, as->vbo_bg_vertex);
    std::array<glm::vec2, 4> empty;
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*4, empty.data(), GL_DYNAMIC_DRAW);

    glGenBuffers(1, &as->vbo_bg_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, as->vbo_bg_index);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::vec2)*index.size(), index.data(), GL_STATIC_DRAW);
    as->bg_index_count = index.size();
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            SDL_Quit();
        }
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

    int win_w, win_h;
    if (!SDL_GetWindowSize(as->window, &win_w, &win_h)) {
        std::cerr << SDL_GetError() << "\n";
        return SDL_APP_FAILURE;
    }

    glViewport(0, 0, win_w, win_h);
    glm::mat4 ortho = glm::ortho(0.0f, win_w*1.0f, win_h*1.0f, 0.0f);

    glUseProgram(as->program);
    glUniformMatrix4fv(glGetUniformLocation(as->program, "projection_matrix"), 1, GL_FALSE, &ortho[0][0]);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(as->vao);

    float cx, cy;
    SDL_GetMouseState(&cx, &cy);

    float w, h, xoff=0, yoff=0;

    if (win_w > win_h) {
        h = win_h;
        w = win_h * ASPECT_RATIO;
        xoff = (win_w - w)/2;
    } else {
        w = win_w;
        h = win_w / ASPECT_RATIO;
        yoff = (win_h - h) /2;
    }

    std::array<glm::vec2, 4> bg;
    bg[0] = glm::vec2{xoff, yoff};
    bg[1] = glm::vec2{xoff + w, yoff};
    bg[2] = glm::vec2{xoff + w, yoff + h};
    bg[3] = glm::vec2{xoff, yoff + h};
   
    glm::vec2 origin{0.f, 0.f};
    glm::vec4 color{0.3f, 0.3f, 0.3f, 1.0f};
    glUniform2fv(glGetUniformLocation(as->program, "center"), 1, &origin[0]);
    glUniform1f(glGetUniformLocation(as->program, "scale"), 1.0f);
    glUniform4fv(glGetUniformLocation(as->program, "color"), 1, &color[0]);

    glBindBuffer(GL_ARRAY_BUFFER, as->vbo_bg_vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec2)*4, bg.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, as->vbo_bg_index);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawElements(GL_TRIANGLES, as->bg_index_count, GL_UNSIGNED_INT, 0);

    float xdiv = w*1.f / (NUM_SHAPES + 1);
    float ydiv = h / 4.0f;
    float scale = xdiv * 0.4f;

    glUniform1f(glGetUniformLocation(as->program, "scale"), scale);

    for (size_t i=0; i < as->shape.size(); i++) {
        auto &s = as->shape[i];

        s.center = glm::vec2{xoff + (i+1)*xdiv, yoff + ydiv};

        glUniform2fv(glGetUniformLocation(as->program, "center"), 1, &s.center[0]);

        glUniform4fv(glGetUniformLocation(as->program, "color"), 1, &s.fill_color[0]);
        glBindBuffer(GL_ARRAY_BUFFER, s.vbo_fill_vertex);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.vbo_fill_index);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawElements(GL_TRIANGLES, s.fill_index_count, GL_UNSIGNED_INT, 0);
  
        glUniform4fv(glGetUniformLocation(as->program, "color"), 1, &s.line_color[0]);
        glBindBuffer(GL_ARRAY_BUFFER, s.vbo_line_vertex);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.vbo_line_index);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawElements(GL_TRIANGLES, s.line_index_count, GL_UNSIGNED_INT, 0);
    }

    SDL_GL_SwapWindow(as->window);

    return SDL_APP_CONTINUE;
}
