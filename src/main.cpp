#define SDL_MAIN_USE_CALLBACKS // use the callbacks instead of main() 
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengles2.h>
#include <SDL3/SDL_timer.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp> 
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
#include <random>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "geometry.hpp"

constexpr int NUM_SHAPES = 5;
constexpr float ASPECT_RATIO = 4.0/3.0;
const glm::vec4 LINE_COLOR{1.f, 1.f, 1.f, 1.f};
const glm::vec4 BG_COLOR{0.3f, 0.3f, 0.3f, 1.f};
constexpr float SHAPE_ROTATION_SPEED = M_PI_2;

std::map<std::string, glm::vec4> tableau10_palette() {
   return {
        {"blue", {0x57/255.f, 0x78/255.f, 0xa4/255.f, 1.f}},
        {"orange", {0xe4/255.f, 0x94/255.f, 0x44/255.f, 1.f}},
        {"red", {0xd1/255.f, 0x61/255.f, 0x5d/255.f, 1.f}},
        {"teal", {0x85/255.f, 0xb6/255.f, 0xb2/255.f, 1.f}},
        {"green", {0x6a/255.f, 0x9f/255.f, 0x58/255.f, 1.f}},
        {"yellow", {0xe7/255.f, 0xca/255.f, 0x60/255.f, 1.f}},
        {"purple", {0xa8/255.f, 0x7c/255.f, 0x9f/255.f, 1.f}},
        {"pink", {0xf1/255.f, 0xa2/255.f, 0xa9/255.f, 1.f}},
        {"brown", {0x96/255.f, 0x76/255.f, 0x62/255.f, 1.f}},
        {"grey", {0xb8/255.f, 0xb0/255.f, 0xac/255.f, 1.f}}
    };
}

struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;

    bool shader_init = false;
    GLuint v_shader;
    GLuint f_shader;
    GLuint program;
    GLuint vao;

    bool init = false;

    // drawing area within the window
    float xoff, yoff;
    float w, h;     
    float xdiv, ydiv;

    GLPrimitive bg;

    std::vector<Shape> shape_set;
    std::array<Shape, NUM_SHAPES> shape;
    std::array<int, NUM_SHAPES> shape_dst;
    std::array<bool, NUM_SHAPES> shape_done;
    int selected_shape = -1;

    uint64_t last_tick = 0;
};

static const char *vertex_shader = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec2 position;
uniform float scale;
uniform float theta;
uniform vec2 trans;
uniform vec4 color;
uniform mat4 projection_matrix;
out vec4 v_color;

void main() {
    float c = cos(theta);
    float s = sin(theta);
    mat2 R = mat2(c, s, -s, c);

    gl_Position = projection_matrix * vec4(R*position*scale + trans, 0.0, 1.0);
    v_color = color;
})";

static const char *fragment_shader = R"(#version 300 es
precision mediump float;

in vec4 v_color;
out vec4 o_color;

void main() {
    o_color = v_color;
})";

// void GLAPIENTRY gl_debug_callback( 
//     GLenum source,
//     GLenum type,
//     GLuint id,
//     GLenum severity,
//     GLsizei length,
//     const GLchar* message,
//     const void* userParam) {
//
//     fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
//            ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
//             type, severity, message );
// }

void update_gl_primitives(AppState &as) {
    float scale = as.xdiv * 0.4f;

    for (auto &s: as.shape) {
        s.line.scale = scale;
        s.fill.scale = scale;
    }
}

void init_game(AppState &as) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> dice_binary(-1, 1);

    // Randomly pick NUM_SHAPE from all the shape set
    std::shuffle(as.shape_set.begin(), as.shape_set.end(), g);

    for (int i=0; i < NUM_SHAPES; i++) {
        as.shape[i] = as.shape_set[i];
        as.shape_dst[i] = i;

        if (dice_binary(g) > 0) {
            as.shape[i].rotation_direction = 1;
        } else {
            as.shape[i].rotation_direction = -1;
        }
        }

    // Randomly assign the shape dest position
    std::shuffle(as.shape_dst.begin(), as.shape_dst.end(), g);

    for (auto &b : as.shape_done) {
        b = false;
    }

    update_gl_primitives(as);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); 
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

    AppState *as = new AppState();
    
    if (!as) {
        std::cerr << "can't alloc memory for AppState\n";
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("shape", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
        std::cerr << "SDL_CreateWindowAndRenderer failed\n";
    }    

    // glEnable(GL_DEBUG_OUTPUT);
    // glDebugMessageCallback(gl_debug_callback, 0);

    as->v_shader = glCreateShader(GL_VERTEX_SHADER);
    as->f_shader = glCreateShader(GL_FRAGMENT_SHADER);

    int length = strlen(vertex_shader);
    glShaderSource(as->v_shader, 1, static_cast<const GLchar**>(&vertex_shader), &length );
    glCompileShader(as->v_shader);

    GLint status;
    glGetShaderiv(as->v_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(as->v_shader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(as->v_shader, maxLength, &maxLength, &errorLog[0]);

        std::cout << std::string(errorLog.data()) << "\n";
        std::cerr << "vertex shader compilation failed\n";
        return SDL_APP_FAILURE;
    }

    length = strlen(fragment_shader);
    glShaderSource(as->f_shader, 1, static_cast<const GLchar**>(&fragment_shader), &length);
    glCompileShader(as->f_shader);

    glGetShaderiv(as->f_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(as->f_shader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(as->f_shader, maxLength, &maxLength, &errorLog[0]);

        std::cout << std::string(errorLog.data()) << "\n";
        std::cerr << "fragment shader compilation failed\n";
        return SDL_APP_FAILURE;
    }

    as->program = glCreateProgram();
    glAttachShader(as->program, as->v_shader);
    glAttachShader(as->program, as->f_shader);
    glLinkProgram(as->program);

    glGenVertexArraysOES(1, &as->vao);
    glBindVertexArrayOES(as->vao);

    // background
    std::vector<glm::vec2> empty(4);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};
    as->bg = make_gl_primitive({empty, index}, BG_COLOR);

    as->shape_set = make_shape_set(LINE_COLOR, tableau10_palette());
    init_game(*as);

    as->last_tick = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

bool recalc_drawing_area(AppState &as) {
    int win_w, win_h;

    if (!SDL_GetWindowSize(as.window, &win_w, &win_h)) {
        std::cerr << SDL_GetError() << "\n";
        return false;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_canvas_element_size("#canvas", win_w, win_h);
#endif

    if (win_w > win_h) {
        as.h = win_h;
        as.w = win_h * ASPECT_RATIO;
        as.xoff = (win_w - as.w)/2;
        as.yoff = 0;
    } else {
        as.w = win_w;
        as.h = win_w / ASPECT_RATIO;
        as.xoff = 0;
        as.yoff = (win_h - as.h) /2;
    }

    as.xdiv = as.w*1.f / NUM_SHAPES;
    as.ydiv = as.h / 4.0f;

    glViewport(0, 0, win_w, win_h);
    glm::mat4 ortho = glm::ortho(0.0f, win_w*1.0f, win_h*1.0f, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(as.program, "projection_matrix"), 1, GL_FALSE, &ortho[0][0]);

    return true;
}

void update_background(const AppState &as) {
    std::array<glm::vec2, 4> bg;

    bg[0] = glm::vec2{as.xoff, as.yoff};
    bg[1] = glm::vec2{as.xoff + as.w, as.yoff};
    bg[2] = glm::vec2{as.xoff + as.w, as.yoff + as.h};
    bg[3] = glm::vec2{as.xoff, as.yoff + as.h};
   
    glBindBuffer(GL_ARRAY_BUFFER, as.bg.vbo_vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec2)*4, bg.data());
}

glm::vec2 shape_index_to_src_pos(const AppState &as, int idx) {
    return glm::vec2{as.xoff + (idx+1)*as.xdiv - as.xdiv*0.5, as.yoff + as.ydiv};
}

glm::vec2 shape_index_to_dst_pos(const AppState &as, int idx) {
    return glm::vec2{as.xoff + (idx+1)*as.xdiv - as.xdiv*0.5, as.yoff + as.ydiv*3};
}

int find_selected_shape(const AppState &as, bool dst) {
    float cx, cy;
    SDL_GetMouseState(&cx, &cy);

    int selected_shape = -1;

    for (int i=0; i < NUM_SHAPES; i++) {
        glm::vec2 pos;

        if (dst) {
            pos = shape_index_to_dst_pos(as, i); 
        } else {
            if (as.shape_done[i]) {
                continue;
            }
            pos = shape_index_to_src_pos(as, i); 
        }

        const auto &s = as.shape[i];
        float dx = std::abs(pos[0] - cx);
        float dy = std::abs(pos[1] - cy);
        float r = s.radius * s.line.scale;

        if (dx < r && dy < r) {
            selected_shape = i;
            break;
        }
    }

    return selected_shape;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState &as = *static_cast<AppState*>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                SDL_Quit();
                return SDL_APP_SUCCESS;
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            recalc_drawing_area(as);
            update_background(as);
            update_gl_primitives(as);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            as.selected_shape = find_selected_shape(as, false);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (as.selected_shape != -1) {
                int dst_idx = find_selected_shape(as, true);

                if (as.shape_dst[as.selected_shape] == dst_idx) {
                    as.shape_done[as.selected_shape] = true;
                }
            }

            as.selected_shape = -1;

            int done = 0;
            for (auto a: as.shape_done) {
                if (a) {
                    done++;
                }
            }

            if (done == NUM_SHAPES) {
                init_game(as);
            }

            break;
    }
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate) {
        AppState &as = *static_cast<AppState*>(appstate);
        SDL_DestroyRenderer(as.renderer);
        SDL_DestroyWindow(as.window);

        for (auto &s: as.shape_set) {
            free_gl_primitive(s.line);
            free_gl_primitive(s.fill);
        }

        glDeleteVertexArraysOES(1, &as.vao);

        delete &as;
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState &as = *static_cast<AppState*>(appstate);

    glUseProgram(as.program);

    if (!as.init) {
        recalc_drawing_area(as);
        update_background(as);
        update_gl_primitives(as);
        as.init = true;
    }

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArrayOES(as.vao);

    float cx, cy;
    SDL_GetMouseState(&cx, &cy);

    draw_gl_primitive(as.program, as.bg);

    float dt = (SDL_GetTicks() - as.last_tick) * 1e-3f;
    as.last_tick = SDL_GetTicks();

    for (size_t i=0; i < as.shape.size(); i++) {
        auto &s = as.shape[i];
        int idx = as.shape_dst[i];

        if (as.shape_done[i]) {
            s.line.trans = shape_index_to_dst_pos(as, idx);
            s.fill.trans = s.line.trans;
            
            draw_gl_primitive(as.program, s.fill);
            draw_gl_primitive(as.program, s.line);
        } else {
            if (i == as.selected_shape) {
                s.fill.trans = glm::vec2{cx, cy};
            } else {
                s.fill.trans = shape_index_to_src_pos(as, i);
            }

            s.line.trans = s.fill.trans;

            float theta = s.line.theta + SHAPE_ROTATION_SPEED * s.rotation_direction * dt;
            if (theta < 0) {
                theta = 2*M_PI;
            } else if (theta > 2*M_PI) {
                theta = 0.f;
            }

            s.line.theta += SHAPE_ROTATION_SPEED * s.rotation_direction * dt;
            s.fill.theta += SHAPE_ROTATION_SPEED * s.rotation_direction * dt;

            draw_gl_primitive(as.program, s.fill);
            draw_gl_primitive(as.program, s.line);

            s.line.trans = shape_index_to_dst_pos(as, idx);
            
            draw_gl_primitive(as.program, s.line);
        }
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
