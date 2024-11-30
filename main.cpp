#define SDL_MAIN_USE_CALLBACKS // use the callbacks instead of main() 
#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengles2.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp> 
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
#include <random>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

constexpr int NUM_SHAPES = 5;
constexpr float ASPECT_RATIO = 4.0/3.0;
const glm::vec4 LINE_COLOR{1.f, 1.f, 1.f, 1.f};
const glm::vec4 FILL_COLOR{0.2f, 0.8f, 0.2f, 1.f};
const glm::vec4 BG_COLOR{0.3f, 0.3f, 0.3f, 1.f};

// Wrapper for GL_TRIANGLES
struct GLPrimitive {
    GLuint vbo_vertex;
    GLuint vbo_index;
    int index_count;
    glm::vec4 color;
    glm::vec2 trans{0.0f, 0.0f};
    float scale = 1.0f;
};

struct VertexIndex {
    std::vector<glm::vec2> vertex;
    std::vector<uint32_t> index;
};


struct Shape {
    int sides;
    glm::vec2 center;
    float radius;
    float line_thickness;

    GLPrimitive line;
    GLPrimitive fill;
};

struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;

    bool shader_init = false;
    GLuint v_shader;
    GLuint f_shader;
    GLuint program;
    GLuint vao;

    bool init = false;

    // actual drawing area
    float xoff, yoff;
    float w, h;     
    float xdiv, ydiv;

    GLPrimitive bg;

    std::vector<Shape> all_shape;
    int selected_shape = -1;
    std::array<Shape, NUM_SHAPES> shape;
    std::array<int, NUM_SHAPES> shape_dst;
    std::array<bool, NUM_SHAPES> shape_done;
};

static const char *vertex_shader = R"(#version 300 es
precision mediump float;

layout(location = 0) in vec2 position;
uniform vec2 trans;
uniform float scale;
uniform vec4 color;
uniform mat4 projection_matrix;
out vec4 v_color;

void main() {
    gl_Position = projection_matrix * vec4(position*scale + trans, 0.0, 1.0);
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

GLPrimitive make_gl_primitive(const VertexIndex &vi, const glm::vec4 &color) {
    GLPrimitive ret;

    glGenBuffers(1, &ret.vbo_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, ret.vbo_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*vi.vertex.size(), vi.vertex.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ret.vbo_index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret.vbo_index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::vec2)*vi.index.size(), vi.index.data(), GL_STATIC_DRAW);

    ret.index_count = vi.index.size();
    ret.color = color;

    return ret;
}

void free_gl_primitive(GLPrimitive &p) {
    if (p.vbo_vertex) {
        glDeleteBuffers(1, &p.vbo_vertex);
        p.vbo_vertex = 0;
    }

    if (p.vbo_index) {
        glDeleteBuffers(1, &p.vbo_index);
        p.vbo_index= 0;
    }
}

void draw_gl_primitive(GLuint program, const GLPrimitive &p) {
    glUniform1f(glGetUniformLocation(program, "scale"), p.scale);
    glUniform2fv(glGetUniformLocation(program, "trans"), 1, &p.trans[0]);
    glUniform4fv(glGetUniformLocation(program, "color"), 1, &p.color[0]);

    glBindBuffer(GL_ARRAY_BUFFER, p.vbo_vertex);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.vbo_index);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawElements(GL_TRIANGLES, p.index_count, GL_UNSIGNED_INT, 0);
}
  
std::vector<glm::vec2> make_polygon(int sides, const std::vector<float> &radius, float theta_offset=0.0f) {
    std::vector<glm::vec2> vert;

    for (int i=0; i < sides; i++) {
        float theta = i * 2*M_PI / sides + theta_offset;

        float r = radius[(i % radius.size())];
        float x = r*std::cos(theta);
        float y = r*std::sin(theta);

        vert.push_back(glm::vec2{x, y});
    }

    return vert;
}

VertexIndex make_fill(const std::vector<glm::vec2> &vert) {
    std::vector<glm::vec2> fill_vert;
    std::vector<uint32_t> fill_idx;

    fill_vert = vert;
    fill_vert.push_back(glm::vec2{0.0f, 0.0f}); // cener of shape

    for (int i=0; i < static_cast<int>(vert.size()); i++) {
        int j = (i + 1) % vert.size();

        fill_idx.push_back(i);
        fill_idx.push_back(j);
        fill_idx.push_back(fill_vert.size() - 1);
    }

    return {fill_vert, fill_idx};
}

VertexIndex make_line(const std::vector<glm::vec2> &vert, float thickness) {
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

        tri_pts.push_back(vert[i] + n*thickness*0.5f);
        tri_pts.push_back(vert[j] + n*thickness*0.5f);
        tri_pts.push_back(vert[j] - n*thickness*0.5f);
        tri_pts.push_back(vert[i] - n*thickness*0.5f);

        outer.push_back(vert[i] + n*thickness*0.5f);
        outer.push_back(vert[j] + n*thickness*0.5f);
        inner.push_back(vert[j] - n*thickness*0.5f);
        inner.push_back(vert[i] - n*thickness*0.5f);
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

    return {tri_pts, tri_idx};
}

Shape create_shape( int sides, const std::vector<float> &radius, float line_thickness, const glm::vec4 &line_color, const glm::vec4 &fill_color, float theta_offset=0.0f) {
    Shape shape;

    shape.radius = *std::max_element(radius.begin(), radius.end());

    std::vector<glm::vec2> vert = make_polygon(sides, radius, theta_offset);

    shape.fill = make_gl_primitive(make_fill(vert), fill_color);
    shape.line = make_gl_primitive(make_line(vert, line_thickness), line_color);

    return shape;
}

std::vector<Shape> create_shape_set() {
    std::vector<Shape> ret;

    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_real_distribution<double> dice(0.0, 2*M_PI);

    for (int sides=3; sides <= 6; sides++) {
        Shape s = create_shape(sides, {1.f}, 0.1f, LINE_COLOR, FILL_COLOR, dice(g));
        ret.push_back(s);
    }

    Shape circle = create_shape(36, {1.f}, 0.1f, LINE_COLOR, FILL_COLOR);
    ret.push_back(circle);

    Shape star = create_shape(10, {1.0f, 0.5f}, 0.1f, LINE_COLOR, FILL_COLOR, dice(g));
    ret.push_back(star);

    Shape rhombus = create_shape(4, {1.0f, 0.5f}, 0.1f, LINE_COLOR, FILL_COLOR, dice(g));
    ret.push_back(rhombus);

    return ret;
}

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

    for (auto &s: as.all_shape) {
        free_gl_primitive(s.line);
        free_gl_primitive(s.fill);
    }

    as.all_shape = create_shape_set();
    std::shuffle(as.all_shape.begin(), as.all_shape.end(), g);

    for (int i=0; i < NUM_SHAPES; i++) {
        as.shape[i] = as.all_shape[i];
        as.shape_dst[i] = i;
    }

    std::shuffle(as.shape_dst.begin(), as.shape_dst.end(), g);

    for (auto &b : as.shape_done) {
        b = false;
    }

    update_gl_primitives(as);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    // SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    // SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    // SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    // SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); 

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

    init_game(*as);

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

    as.xdiv = as.w*1.f / (NUM_SHAPES + 1);
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
    return glm::vec2{as.xoff + (idx+1)*as.xdiv, as.yoff + as.ydiv};
}

glm::vec2 shape_index_to_dst_pos(const AppState &as, int idx) {
    return glm::vec2{as.xoff + (idx+1)*as.xdiv, as.yoff + as.ydiv*3};
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
        if (glm::length(pos - glm::vec2{cx, cy}) < s.radius * s.line.scale) {
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
        AppState *as = static_cast<AppState*>(appstate);
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);

        delete as;
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

            draw_gl_primitive(as.program, s.fill);
            draw_gl_primitive(as.program, s.line);

            s.line.trans = shape_index_to_dst_pos(as, idx);
            
            draw_gl_primitive(as.program, s.line);
        }
    }

    SDL_GL_SwapWindow(as.window);

    return SDL_APP_CONTINUE;
}
