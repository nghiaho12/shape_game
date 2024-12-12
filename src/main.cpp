#include <SDL3/SDL_init.h>
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

#include <vector>
#include <cmath>
#include <cstdio>
#include <array>
#include <algorithm>
#include <random>
#include <map>
#include <optional>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "geometry.hpp"
#include "audio.hpp"
#include "shader.hpp"
#include "log.hpp"

constexpr int NUM_SHAPES = 5;
constexpr float ASPECT_RATIO = 4.0/3.0;
const glm::vec4 LINE_COLOR{1.f, 1.f, 1.f, 1.f};
const glm::vec4 BG_COLOR{0.3f, 0.3f, 0.3f, 1.f};
constexpr float SHAPE_ROTATION_SPEED = M_PI_2;

std::map<std::string, glm::vec4> tableau10_palette() {
    const std::map<std::string, uint32_t> color{
        {"blue", 0x5778a4},
        {"orange", 0xe49444},
        {"red", 0xd1615d},
        {"teal", 0x85b6b2},
        {"green", 0x6a9f58},
        {"yellow", 0xe7ca60},
        {"purple", 0xa87c9f},
        {"pink", 0xf1a2a9},
        {"brown", 0x967662},
        {"grey", 0xb8b0ac},
    };

    std::map<std::string, glm::vec4> ret;

    for (auto it: color) {
        uint32_t c = it.second;
        uint8_t r = c >> 16;
        uint8_t g = c >> 8 & 0xff;
        uint8_t b = c & 0xff;

        ret[it.first] = {r/255.f, g/255.f, b/255.f, 1.0f};
    }

    return ret;
}

struct AppState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_GLContext gl_ctx;

    SDL_AudioDeviceID audio_device = 0;
    Audio bgm;
    Audio sfx_correct;
    Audio sfx_win;

    bool shader_init = false;
    GLuint v_shader = 0;
    GLuint f_shader = 0;
    GLuint program = 0;
    GLuint vao = 0;

    bool init = false;

    // drawing area within the window
    float xoff = 0.0f;
    float yoff = 0.0f;
    float w = 0.f;
    float h = 0.f;     
    float xdiv = 0.f;
    float ydiv = 0.f;

    GLPrimitive bg;

    std::vector<Shape> shape_set;
    std::array<Shape, NUM_SHAPES> shape;
    std::array<int, NUM_SHAPES> shape_dst;
    std::array<bool, NUM_SHAPES> shape_done;
    int selected_shape = -1;
    int highlight_dst = -1;

    uint64_t last_tick = 0;
};

void update_gl_primitives(AppState &as) {
    float scale = as.xdiv * 0.4f;

    for (auto &s: as.shape) {
        s.set_scale(scale);
    }
}

void init_game(AppState &as) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_real_distribution<float> dice_binary(0, 1);

    // Randomly pick NUM_SHAPE from all the shape set
    std::shuffle(as.shape_set.begin(), as.shape_set.end(), g);

    for (int i=0; i < NUM_SHAPES; i++) {
        as.shape[i] = as.shape_set[i];
        as.shape_dst[i] = i;

        if (dice_binary(g) > 0.5) {
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

    as.selected_shape = -1;
    as.highlight_dst = -1;

    update_gl_primitives(as);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOG("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = new AppState();
    
    if (!as) {
        LOG("can't alloc memory for AppState");
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    std::string base_path = "assets/";
#ifdef __ANDROID__
    base_path = "";
#endif

    if (auto w = load_ogg((base_path + "bgm.ogg").c_str(), 0.1)) {
        as->bgm = *w;
    } else {
        return SDL_APP_FAILURE;
    }

    if (auto w = load_ogg((base_path + "win.ogg").c_str())) {
        as->sfx_win = *w;
    } else {
        return SDL_APP_FAILURE;
    }

    if (auto w = load_wav((base_path + "ding.wav").c_str())) {
        as->sfx_correct = *w;
    } else {
        return SDL_APP_FAILURE;
    }

    SDL_ResumeAudioStreamDevice(as->bgm.stream);

    as->audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (as->audio_device == 0) {
        LOG("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); 
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

    if (!SDL_CreateWindowAndRenderer("shape", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
        LOG("SDL_CreateWindowAndRenderer failed");
        return SDL_APP_FAILURE;
    }    

    if (!SDL_SetRenderVSync(as->renderer, 1)) {
        LOG("SDL_SetRenderVSync failed");
        return SDL_APP_FAILURE;
    }

#ifndef __EMSCRIPTEN__
    as->gl_ctx = SDL_GL_CreateContext(as->window);
    SDL_GL_MakeCurrent(as->window, as->gl_ctx);
#endif

    as->v_shader = glCreateShader(GL_VERTEX_SHADER);
    as->f_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(as->v_shader, vertex_shader)) {
        return SDL_APP_FAILURE;
    }

    if (!compile_shader(as->f_shader, fragment_shader)) {
        return SDL_APP_FAILURE;
    }

    as->program = glCreateProgram();
    glAttachShader(as->program, as->v_shader);
    glAttachShader(as->program, as->f_shader);
    glLinkProgram(as->program);

    glGenVertexArraysOES(1, &as->vao);
    glBindVertexArrayOES(as->vao);

    // background color for drawing area
    // the area size is determined later
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
        LOG("%s", SDL_GetError());
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
#ifndef __EMSCRIPTEN__
            if (event->key.key == SDLK_ESCAPE) {
                SDL_Quit();
                return SDL_APP_SUCCESS;
            }
#endif
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            recalc_drawing_area(as);
            update_background(as);
            update_gl_primitives(as);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            as.selected_shape = find_selected_shape(as, false);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            as.highlight_dst = -1;

            if (as.selected_shape != -1) {
                as.highlight_dst = find_selected_shape(as, true);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            as.highlight_dst = -1;

            if (as.selected_shape != -1) {
                int dst_idx = find_selected_shape(as, true);

                if (as.shape_dst[as.selected_shape] == dst_idx) {
                    as.shape_done[as.selected_shape] = true;

                    SDL_PutAudioStreamData(as.sfx_correct.stream, as.sfx_correct.data.data(), as.sfx_correct.data.size());
                    SDL_ResumeAudioStreamDevice(as.sfx_correct.stream);
                }
            }

            as.selected_shape = -1;

            // check if we won
            auto is_true = [](bool b) { return b; };
            if (std::all_of(as.shape_done.begin(), as.shape_done.end(), is_true)) {
                SDL_PutAudioStreamData(as.sfx_win.stream, as.sfx_win.data.data(), as.sfx_win.data.size());
                SDL_ResumeAudioStreamDevice(as.sfx_win.stream);

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

    if (SDL_GetAudioStreamAvailable(as.bgm.stream) < static_cast<int>(as.bgm.data.size())) {
        SDL_PutAudioStreamData(as.bgm.stream, as.bgm.data.data(), as.bgm.data.size());
    }

#ifndef __EMSCRIPTEN__
    SDL_GL_MakeCurrent(as.window, as.gl_ctx);
#endif

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

    float cx=0, cy=0;
    SDL_GetMouseState(&cx, &cy);

    draw_gl_primitive(as.program, as.bg);

    float dt = (SDL_GetTicksNS() - as.last_tick) * 1e-9f;
    as.last_tick = SDL_GetTicksNS();

    for (int i=0; i < static_cast<int>(as.shape.size()); i++) {
        auto &s = as.shape[i];
        int dst_idx = as.shape_dst[i];

        if (as.shape_done[i]) {
            s.set_trans(shape_index_to_dst_pos(as, dst_idx));
            
            draw_gl_primitive(as.program, s.fill);
            draw_gl_primitive(as.program, s.line);
        } else {
            if (i == as.selected_shape) {
                s.set_trans(glm::vec2{cx, cy});
            } else {
                s.set_trans(shape_index_to_src_pos(as, i));
            }

            float theta = s.line.theta + SHAPE_ROTATION_SPEED * s.rotation_direction * dt;
            if (theta < 0) {
                theta = 2*M_PI;
            } else if (theta > 2*M_PI) {
                theta = 0.f;
            }

            s.set_theta(theta);

            draw_gl_primitive(as.program, s.fill);
            draw_gl_primitive(as.program, s.line);

            // destination shape
            if (as.highlight_dst == dst_idx) {
                s.line_highlight.trans = shape_index_to_dst_pos(as, dst_idx);
                draw_gl_primitive(as.program, s.line_highlight);
            } else {
                s.line.trans = shape_index_to_dst_pos(as, dst_idx);
                draw_gl_primitive(as.program, s.line);
            }
        }
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
