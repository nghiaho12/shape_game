#define SDL_MAIN_USE_CALLBACKS // use the callbacks instead of main() 
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL_init.h>
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

#include "log.hpp"
#include "geometry.hpp"
#include "audio.hpp"
#include "font.hpp"
#include "gl_helper.hpp"

constexpr int NUM_SHAPES = 5;
constexpr int MAX_SCORE = 10; // score will wrap
constexpr float ASPECT_RATIO = 4.f/3.f;
const glm::vec4 LINE_COLOR{1.f, 1.f, 1.f, 1.f};
const glm::vec4 BG_COLOR{0.3f, 0.3f, 0.3f, 1.f};
constexpr float SHAPE_ROTATION_SPEED = static_cast<float>(M_PI_2);

const glm::vec4 TEXT_FG{231/255.0, 202/255.0, 96/255.0, 1.0};
const glm::vec4 TEXT_BG{0, 0, 0, 0};
const glm::vec4 TEXT_OUTLINE{1, 1, 1, 1};
constexpr float TEXT_OUTLINE_FACTOR = 0.1f;
// units as percentage of drawing width
constexpr float TEXT_WIDTH = 0.2f;
constexpr float TEXT_X = 0.f;
constexpr float TEXT_Y = 0.98f;

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
        uint8_t r = static_cast<uint8_t>(c >> 16);
        uint8_t g = (c >> 8) & 0xff;
        uint8_t b = c & 0xff;

        ret[it.first] = {r/255.f, g/255.f, b/255.f, 1.0f};
    }

    return ret;
}

enum class AudioEnum {BGM, CORRECT, WIN};

struct AppState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_GLContext gl_ctx;

    SDL_AudioDeviceID audio_device = 0;
    std::map<AudioEnum, Audio> audio;

    FontAtlas font;
    GLPrimitive letter;
    GLuint vao = 0;

    int score = 0;
    bool init = false;

    // drawing area within the window
    float xoff = 0.0f;
    float yoff = 0.0f;
    float w = 0.f;
    float h = 0.f;     
    float xdiv = 0.f;
    float ydiv = 0.f;

    GLPrimitive bg;
    VertexBufferPtr score_vertex{{}, {}};
    std::pair<glm::vec2, glm::vec2> score_vertex_bbox;

    ShaderPtr shape_shader{{}, {}};
    std::vector<Shape> shape_set;
    std::array<Shape*, NUM_SHAPES> shape;
    std::array<size_t, NUM_SHAPES> shape_dst;
    std::array<bool, NUM_SHAPES> shape_done;
    std::optional<size_t> selected_shape;
    std::optional<size_t> highlight_dst;

    uint64_t last_tick = 0;
};

void update_scale(AppState &as) {
    float scale = as.xdiv * 0.4f;

    for (auto &s: as.shape) {
        s->set_scale(scale);
    }

    as.font.set_target_width(as.w * TEXT_WIDTH);
    as.font.set_trans(glm::vec2{as.xoff + as.w*TEXT_X, as.yoff + as.h*TEXT_Y});
}

void init_game(AppState &as) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_real_distribution<float> dice_binary(0, 1);

    // Randomly pick NUM_SHAPE from all the shape set
    std::shuffle(as.shape_set.begin(), as.shape_set.end(), g);

    size_t i = 0;
    for (auto &s: as.shape) {
        s = &as.shape_set[i];
        as.shape_dst[i] = i;

        if (dice_binary(g) > 0.5) {
            s->rotation_direction = 1;
        } else {
            s->rotation_direction = -1;
        }

        i++;
    }

    // Randomly assign the shape dest position
    std::shuffle(as.shape_dst.begin(), as.shape_dst.end(), g);

    for (auto &b : as.shape_done) {
        b = false;
    }

    as.selected_shape.reset();
    as.highlight_dst.reset();

    update_scale(as);
}

bool init_audio(AppState &as, const std::string &base_path) {
    as.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (as.audio_device == 0) {
        LOG("Couldn't open audio device: %s", SDL_GetError());
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "bgm.ogg").c_str(), 0.1f)) {
        as.audio[AudioEnum::BGM] = *w;
    } else {
        return false;
    }

    if (auto w = load_ogg(as.audio_device, (base_path + "win.ogg").c_str())) {
        as.audio[AudioEnum::WIN] = *w;
    } else {
        return false;
    }

    if (auto w = load_wav(as.audio_device, (base_path + "ding.wav").c_str())) {
        as.audio[AudioEnum::CORRECT] = *w;
    } else {
        return false;
    }

    return true;
}

bool init_font(AppState &as, const std::string &base_path) {
    if (!as.font.load(base_path + "atlas.bmp", base_path + "atlas.txt")) {
        return false;
    }

    as.font.set_fg(TEXT_FG);
    as.font.set_bg(TEXT_BG);
    as.font.set_outline(TEXT_OUTLINE);
    as.font.set_outline_factor(TEXT_OUTLINE_FACTOR);

    return true;
}

void update_score_text(AppState &as) {
    auto [vertex, index] = as.font.make_text_vertex(std::to_string(as.score));
    as.score_vertex_bbox = bbox(vertex);
    as.score_vertex->update_vertex(vertex, index);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    // Unused
    (void)argc;
    (void)argv;

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

    if (!init_audio(*as, base_path)) {
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); 
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

    if (!SDL_CreateWindowAndRenderer("Shape Game", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
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
    enable_gl_debug_callback();
#endif

    if (!init_font(*as, base_path)) {
        return SDL_APP_FAILURE;
    }

    // pre-allocate all vertex we need
    // number of space needs to be >= MAX_SCORE string
    as->score_vertex = as->font.make_text("    "); 
    update_score_text(*as);

    as->shape_shader = make_shape_shader();
        
    glEnable(GL_BLEND);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glGenVertexArraysOES(1, &as->vao);
    glBindVertexArrayOES(as->vao);


    // background color for drawing area
    // the area size is determined later
    std::vector<glm::vec2> empty(4);
    std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};
    as->bg.vertex_buffer = make_vertex_buffer(empty, index);
    as->bg.color = BG_COLOR;

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

    float win_wf = static_cast<float>(win_w);
    float win_hf = static_cast<float>(win_h);

    if (win_w > win_h) {
        as.h = win_hf;
        as.w = win_hf * ASPECT_RATIO;
        as.xoff = (win_wf - as.w)/2;
        as.yoff = 0;
    } else {
        as.w = win_wf;
        as.h = win_wf / ASPECT_RATIO;
        as.xoff = 0;
        as.yoff = (win_hf - as.h) /2;
    }

    as.xdiv = as.w*1.f / NUM_SHAPES;
    as.ydiv = as.h / 4.f;

    glViewport(0, 0, win_w, win_h);
    glm::mat4 ortho = glm::ortho(0.f, win_wf, win_hf, 0.f);
   
    as.shape_shader->use();
    glUniformMatrix4fv(as.shape_shader->get_loc("projection_matrix"), 1, GL_FALSE, &ortho[0][0]);

    as.font.shader->use();
    glUniformMatrix4fv(as.font.shader->get_loc("projection_matrix"), 1, GL_FALSE, &ortho[0][0]);

    return true;
}

void update_background(const AppState &as) {
    std::vector<glm::vec2> bg(4);

    bg[0] = glm::vec2{as.xoff, as.yoff};
    bg[1] = glm::vec2{as.xoff + as.w, as.yoff};
    bg[2] = glm::vec2{as.xoff + as.w, as.yoff + as.h};
    bg[3] = glm::vec2{as.xoff, as.yoff + as.h};
  
    as.bg.vertex_buffer->update_vertex(bg);
}

glm::vec2 shape_index_to_src_pos(const AppState &as, size_t idx) {
    return glm::vec2{as.xoff + static_cast<float>(idx+1)*as.xdiv - as.xdiv*0.5, as.yoff + as.ydiv};
}

glm::vec2 shape_index_to_dst_pos(const AppState &as, size_t idx) {
    return glm::vec2{as.xoff + static_cast<float>(idx+1)*as.xdiv - as.xdiv*0.5, as.yoff + as.ydiv*3};
}

std::optional<size_t> find_selected_shape(const AppState &as, bool dst) {
    float cx, cy;
    SDL_GetMouseState(&cx, &cy);

    std::optional<size_t> selected_shape;

    for (size_t i=0; i < NUM_SHAPES; i++) {
        glm::vec2 pos;

        if (dst) {
            pos = shape_index_to_dst_pos(as, i); 
        } else {
            if (as.shape_done[i]) {
                continue;
            }
            pos = shape_index_to_src_pos(as, i); 
        }

        const auto &s = *as.shape[i];
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
            update_scale(as);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            as.selected_shape = find_selected_shape(as, false);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            as.highlight_dst.reset();

            if (as.selected_shape) {
                as.highlight_dst = find_selected_shape(as, true);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            as.highlight_dst.reset();

            if (as.selected_shape) {
                std::optional<size_t> dst_idx = find_selected_shape(as, true);

                if (as.shape_dst[*as.selected_shape] == dst_idx) {
                    as.shape_done[*as.selected_shape] = true;
                    as.audio[AudioEnum::CORRECT].play();
                }
            }

            as.selected_shape.reset();

            // check if we won
            auto is_true = [](bool b) { return b; };
            if (std::all_of(as.shape_done.begin(), as.shape_done.end(), is_true)) {
                as.audio[AudioEnum::WIN].play();
                as.score++;

                if (as.score > MAX_SCORE) {
                    as.score = 1;
                }

                init_game(as);
                update_score_text(as);
            }

            break;
    }
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    // Unused
    (void)result;

    if (appstate) {
        AppState &as = *static_cast<AppState*>(appstate);
        SDL_DestroyRenderer(as.renderer);
        SDL_DestroyWindow(as.window);

        glDeleteVertexArraysOES(1, &as.vao);

        SDL_CloseAudioDevice(as.audio_device);

        // TODO: This code causes a crash as of libSDL preview-3.1.6
        // for (auto &a: as.audio) {
        //     if (a.second.stream) {
        //         SDL_DestroyAudioStream(a.second.stream);
        //     }
        // }

        delete &as;
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState &as = *static_cast<AppState*>(appstate);

    float dt = static_cast<float>(SDL_GetTicksNS() - as.last_tick) * 1e-9f;
    as.last_tick = SDL_GetTicksNS();

    auto &bgm = as.audio[AudioEnum::BGM];
    if (SDL_GetAudioStreamAvailable(bgm.stream) < static_cast<int>(bgm.data.size())) {
        bgm.play();
    }

#ifndef __EMSCRIPTEN__
    SDL_GL_MakeCurrent(as.window, as.gl_ctx);
#endif

    glUseProgram(as.shape_shader->program);

    if (!as.init) {
        recalc_drawing_area(as);
        update_background(as);
        update_scale(as);
        as.init = true;
    }

    // glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArrayOES(as.vao);

    float cx=0, cy=0;
    SDL_GetMouseState(&cx, &cy);

    as.bg.draw(as.shape_shader);

    for (size_t i=0; i < as.shape.size(); i++) {
        auto &s = *as.shape[i];
        size_t dst_idx = as.shape_dst[i];

        if (as.shape_done[i]) {
            s.set_trans(shape_index_to_dst_pos(as, dst_idx));
           
            s.fill.draw(as.shape_shader);
            s.line.draw(as.shape_shader);
        } else {
            if (i == as.selected_shape) {
                s.set_trans(glm::vec2{cx, cy});
            } else {
                s.set_trans(shape_index_to_src_pos(as, i));
            }

            float theta = s.line.theta + SHAPE_ROTATION_SPEED * s.rotation_direction * dt;
            if (theta < 0) {
                theta = static_cast<float>(2*M_PI);
            } else if (theta > 2*M_PI) {
                theta = 0.f;
            }

            s.set_theta(theta);

            s.fill.draw(as.shape_shader);
            s.line.draw(as.shape_shader);

            // destination shape
            if (as.highlight_dst == dst_idx) {
                s.line_highlight.trans = shape_index_to_dst_pos(as, dst_idx);
                s.line_highlight.draw(as.shape_shader);
            } else {
                s.line.trans = shape_index_to_dst_pos(as, dst_idx);
                s.line.draw(as.shape_shader);
            }
        }
    }

    if (as.score > 0) {
        glm::vec2 text_center = (as.score_vertex_bbox.first + as.score_vertex_bbox.second)*0.5f * as.font.distance_scale;
        glm::vec2 center{as.xoff + 0.5*as.w - text_center.x, as.yoff + 0.5f*as.h - text_center.y};

        as.font.set_trans(center);
        draw_with_texture(as.font.shader, as.font.tex, as.score_vertex);
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
