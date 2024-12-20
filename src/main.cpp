#define SDL_MAIN_USE_CALLBACKS  // use the callbacks instead of main()
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengles2.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <map>
#include <optional>
#include <random>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "audio.hpp"
#include "font.hpp"
#include "geometry.hpp"
#include "gl_helper.hpp"
#include "log.hpp"

constexpr int NUM_SHAPES = 5;
constexpr int MAX_SCORE = 100;  // score will wrap
constexpr float ASPECT_RATIO = 4.f / 3.f;
const glm::vec4 LINE_COLOR{1.f, 1.f, 1.f, 1.f};
const glm::vec4 BG_COLOR{0.3f, 0.3f, 0.3f, 1.f};
constexpr float SHAPE_ROTATION_SPEED = static_cast<float>(M_PI_2);
constexpr float SHAPE_WIDTH = (1.f / NUM_SHAPES) * 0.4f;

const glm::vec4 TEXT_FG{231 / 255.0, 202 / 255.0, 96 / 255.0, 1.0};
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

    for (auto it : color) {
        uint32_t c = it.second;
        uint8_t r = static_cast<uint8_t>(c >> 16);
        uint8_t g = (c >> 8) & 0xff;
        uint8_t b = c & 0xff;

        ret[it.first] = {r / 255.f, g / 255.f, b / 255.f, 1.0f};
    }

    return ret;
}

enum class AudioEnum { BGM, CORRECT, WIN };

struct AppState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_GLContext gl_ctx;

    SDL_AudioDeviceID audio_device = 0;
    std::map<AudioEnum, Audio> audio;

    FontAtlas font;
    VertexArrayPtr vao{{}, {}};

    int score = 0;
    bool init = false;

    // drawing area within the window
    glm::vec2 draw_area_offset;
    glm::vec2 draw_area_size;
    glm::vec2 draw_area_grid_size;
    Shape draw_area_bg;

    VertexBufferPtr score_vertex{{}, {}};
    BBox score_vertex_bbox;

    ShapeShader shape_shader;
    std::vector<Shape> shape_set; // all possible shapes
    std::array<Shape *, NUM_SHAPES> shape; // subset of shapes
    std::array<size_t, NUM_SHAPES> shape_src_to_dst_idx; 
    std::array<glm::vec2, NUM_SHAPES> src_pos; // position for src shape, normalized units
    std::array<glm::vec2, NUM_SHAPES> dst_pos; // position for dst shape, normalized units
    std::array<bool, NUM_SHAPES> shape_done;
    std::optional<size_t> selected_shape;
    std::optional<size_t> highlight_dst;

    uint64_t last_tick = 0;
};

void update_scale(AppState &as) {
    float scale = as.draw_area_size.x;
    as.shape_shader.set_screen_scale(scale);

    as.font.set_target_width(as.draw_area_size.x * TEXT_WIDTH);
    as.font.set_trans(glm::vec2{as.draw_area_offset.x + as.draw_area_size.x * TEXT_X,
                                as.draw_area_offset.y + as.draw_area_size.y * TEXT_Y});
}

void init_game(AppState &as) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_real_distribution<float> dice_binary(0, 1);

    // Randomly pick NUM_SHAPE from all the shape set
    std::shuffle(as.shape_set.begin(), as.shape_set.end(), g);

    size_t i = 0;
    for (auto &s : as.shape) {
        s = &as.shape_set[i];
        as.shape_src_to_dst_idx[i] = i;

        if (dice_binary(g) > 0.5) {
            s->rotation_direction = 1;
        } else {
            s->rotation_direction = -1;
        }

        i++;
    }

    // Randomly assign the shape dest position
    std::shuffle(as.shape_src_to_dst_idx.begin(), as.shape_src_to_dst_idx.end(), g);

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
    as.score_vertex->update_vertex(
        glm::value_ptr(vertex[0]), sizeof(decltype(vertex)::value_type) * vertex.size(), index);
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

    if (!SDL_CreateWindowAndRenderer(
            "Shape Game", 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &as->window, &as->renderer)) {
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

    if (!as->shape_shader.init()) {
        return SDL_APP_FAILURE;
    }

    as->vao = make_vertex_array();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // background color for drawing area
    {
        float h = 1.0f/ ASPECT_RATIO;

        std::vector<glm::vec2> vertex{
            {0.f, 0.f},
            {1.f, 0.f}, 
            {1.f, h},
            {0.f, h},
        };

        std::vector<uint32_t> index{0, 1, 2, 0, 2, 3};

        as->draw_area_bg.fill.vertex_buffer = make_vertex_buffer(vertex, index);
        as->draw_area_bg.fill.color = BG_COLOR;
    }

    as->shape_set = make_shape_set(LINE_COLOR, tableau10_palette());

    for (auto &s: as->shape_set) {
        s.scale = SHAPE_WIDTH;
    }

    // position for the src and dst shape
    float div = NUM_SHAPES * 2;
    float norm_h = 1.f / ASPECT_RATIO;

    for (size_t i=0; i < NUM_SHAPES; i++) {
        as->src_pos[i].x = static_cast<float>(i*2 + 1) / div;
        as->src_pos[i].y = norm_h * 1.f/4.f;

        as->dst_pos[i].x = static_cast<float>(i*2 + 1) / div;
        as->dst_pos[i].y = norm_h * 3.f/4.f;
    }

    init_game(*as);

    as->last_tick = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

bool recalc_draw_area(AppState &as) {
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
        as.draw_area_size.y = win_hf;
        as.draw_area_size.x = win_hf * ASPECT_RATIO;
        as.draw_area_offset.x = (win_wf - as.draw_area_size.x) / 2;
        as.draw_area_offset.y = 0;
    } else {
        as.draw_area_size.x = win_wf;
        as.draw_area_size.y = win_wf / ASPECT_RATIO;
        as.draw_area_offset.x = 0;
        as.draw_area_offset.y = (win_hf - as.draw_area_size.y) / 2;
    }


    as.draw_area_grid_size.x = as.draw_area_size.x * 1.f / NUM_SHAPES;
    as.draw_area_grid_size.y = as.draw_area_size.y / 4.f;

    glViewport(0, 0, win_w, win_h);
    glm::mat4 ortho = glm::ortho(0.f, win_wf, win_hf, 0.f);

    as.shape_shader.set_drawing_area_offset(as.draw_area_offset);
    as.shape_shader.set_ortho(ortho);

    as.font.shader->use();
    glUniformMatrix4fv(as.font.shader->get_loc("ortho_matrix"), 1, GL_FALSE, &ortho[0][0]);

    return true;
}

std::optional<size_t> find_selected_shape(const AppState &as, bool dst) {
    float cx, cy;
    SDL_GetMouseState(&cx, &cy);

    std::optional<size_t> selected_shape;

    for (size_t i = 0; i < NUM_SHAPES; i++) {
        glm::vec2 pos;

        if (dst) {
            pos = as.dst_pos[i];
        } else {
            if (as.shape_done[i]) {
                continue;
            }
            pos = as.src_pos[i];
        }

        BBox bbox = shape_bbox_to_screen_units(as.shape_shader, *as.shape[i]);

        if ((cx > bbox.start.x) && (cx < bbox.end.x) && (cy > bbox.start.y) && (cy < bbox.end.y)) {
            selected_shape = i;
            break;
        }
    }

    return selected_shape;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState &as = *static_cast<AppState *>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
#ifndef __EMSCRIPTEN__
            if (event->key.key == SDLK_ESCAPE) {
                SDL_Quit();
                return SDL_APP_SUCCESS;
            }
#endif
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            recalc_draw_area(as);
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

                if (as.shape_src_to_dst_idx[*as.selected_shape] == dst_idx) {
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
        AppState &as = *static_cast<AppState *>(appstate);
        SDL_DestroyRenderer(as.renderer);
        SDL_DestroyWindow(as.window);

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

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState &as = *static_cast<AppState *>(appstate);

    float dt = static_cast<float>(SDL_GetTicksNS() - as.last_tick) * 1e-9f;
    as.last_tick = SDL_GetTicksNS();

    auto &bgm = as.audio[AudioEnum::BGM];
    if (SDL_GetAudioStreamAvailable(bgm.stream) < static_cast<int>(bgm.data.size())) {
        bgm.play();
    }

#ifndef __EMSCRIPTEN__
    SDL_GL_MakeCurrent(as.window, as.gl_ctx);
#endif

    as.shape_shader.shader->use();

    if (!as.init) {
        recalc_draw_area(as);
        update_scale(as);
        as.init = true;
    }

    // glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    as.vao->use();

    float cx = 0, cy = 0;
    SDL_GetMouseState(&cx, &cy);

    draw_shape(as.shape_shader, as.draw_area_bg);

    for (size_t i = 0; i < as.shape.size(); i++) {
        auto &s = *as.shape[i];
        size_t dst_idx = as.shape_src_to_dst_idx[i];

        if (as.shape_done[i]) {
            s.trans = as.dst_pos[dst_idx];
            draw_shape(as.shape_shader, s, true, true);
        } else {
            if (i == as.selected_shape) {
                glm::vec2 pos = screen_pos_to_normalize_pos(as.shape_shader, glm::vec2{cx, cy});
                s.trans = pos;
            } else {
                s.trans = as.src_pos[i];
            }

            float theta = s.theta + SHAPE_ROTATION_SPEED * s.rotation_direction * dt;
            if (theta < 0) {
                theta = static_cast<float>(2 * M_PI);
            } else if (theta > 2 * M_PI) {
                theta = 0.f;
            }

            s.theta = theta;
            draw_shape(as.shape_shader, s, true, true);

            // destination shape
            s.trans = as.dst_pos[dst_idx];
            if (as.highlight_dst == dst_idx) {
                draw_shape(as.shape_shader, s, false, false, true);
            } else {
                draw_shape(as.shape_shader, s, false, true, false);
            }
        }
    }

    if (as.score > 0) {
        // draw the score in the middle of the drawing area
        const BBox &bbox = as.score_vertex_bbox;
        const glm::vec2 &offset = as.draw_area_offset;
        const glm::vec2 &size = as.draw_area_size;

        glm::vec2 text_center = (bbox.start + bbox.end) * 0.5f * as.font.distance_scale;
        glm::vec2 center= glm::vec2{offset.x + 0.5f * size.x, offset.y + 0.5f * size.y} - text_center;

        as.font.set_trans(center);
        draw_vertex_buffer(as.font.shader, as.score_vertex, as.font.tex);
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
