/* Adapted from https://github.com/Immediate-Mode-UI/Nuklear/tree/master/demo/sdl_opengles2 */

#define DEBUG 0
#include <cglm/struct.h>
#include <cglm/struct/io.h>
#include <GLES2/gl2.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GLES2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_gles2.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define MOVESPEED 0.1f
#define PI 3.14159265358979323846

#define UNUSED(a) (void)a
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])

/* ===============================================================
 *
 *                          Types
 *
 * ===============================================================*/

enum obj_type
{
    CUBE,
    CAM,
    GRID,
    CAMERA,
    FRUSTUM
};

struct ogl_init
{
    enum obj_type type;
    const char *vert_shader;
    const char *frag_shader;
    const GLfloat *verts;
    size_t vert_len;
    const GLushort *indices;
    size_t index_len;
    const float *colors;
    size_t color_len;
};

struct ogl
{
    struct ogl_init *init_data;
    unsigned int program;
    unsigned int matrixID;
    unsigned int vboID;
    unsigned int idoID;
    unsigned int colorID;
    unsigned int mvpLoc;
    unsigned int vertexLoc;
    unsigned int colorLoc;
};

static struct scene_objects
{
    struct ogl cube;
    struct ogl grid;
    struct ogl frustum;
    struct ogl cam;
} objs;

struct orientation
{
    float tx;
    float ty;
    float tz;
    float sx;
    float sy;
    float sz;
    float rx;
    float ry;
    float rz;
};

struct cam_orientation
{
    vec3s eye;
    vec3s center;
    vec3s up;
};

struct cam_perspective
{
    float fov;
    float aspect_ratio;
    float near;
    float far;
};

/* ===============================================================
 *
 *                          Function declarations
 *
 * ===============================================================*/

static void init_cam();
static void init_objs();
static void use_program(struct ogl *obj);
static void bind_vertices(struct ogl *obj);
static bool init_indices(struct ogl *draw_data, struct ogl_init *init_data);
static void init_color(struct ogl *obj);
static bool init_shader(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_frustum(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_indices(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_verts(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_cube(struct ogl *draw_data, struct ogl_init *init_data);
static void update_frustum_buffer();
static mat4s perspective(float FOV, float AspectRatio, float Near, float Far);
static mat4s calc_cube_mvp(struct cam_orientation *ornt, struct cam_perspective *prsp);
static mat4s calc_grid_mvp(struct cam_orientation *ornt, struct cam_perspective *prsp);
static mat4s calc_frustum_mvp();
static void set_frustum_verts();
static void reset_cube_transform();
static void reset_proj_cam();
static void draw_vert_lines(struct ogl *obj, mat4s mvp);
static void draw_triangle_indices(struct ogl *obj, mat4s mvp);
static void draw_cube(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp);
static void draw_frustum(struct ogl *obj);
static void draw_grid(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp);
static void MainLoop(void *loopArg);

/* ===============================================================
 *
 *                          Variables
 *
 * ===============================================================*/

static const char
frustum_vert_shader[] =
    "precision mediump float;"
    "uniform mat4 mvp;"
    "attribute vec4 a_position;"
    "attribute vec4 a_color;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_Position = mvp * a_position;"
    "v_color = a_color;"
    "}";

static const char
frustum_frag_shader[] =
    "precision mediump float;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_FragColor = v_color;"
    "}";

static const char
grid_frag_shader[] =
    "precision mediump float;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_FragColor = vec4(1.0,0.0,0.0,1.0);"
    "}";

static const char
common_vert_shader[] =
    "precision mediump float;"
    "uniform mat4 mvp;"
    "attribute vec4 a_position;"
    "attribute vec4 a_color;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_Position = mvp * a_position;"
    "v_color = a_color;"
    "}";

static const char
cube_frag_shader[] =
    "precision mediump float;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_FragColor = v_color;"
    "}";

// TODO: vary color on verts a bit
static const char
cam_frag_shader[] =
    "precision mediump float;"
    "varying vec4 v_color;"
    "void main()"
    "{"
    "gl_FragColor = vec4(0.5,0.5,0.5,1.0);"
    "}";

static const float
frustum_colors[] =
{
    //front
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    // right
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    // back
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    // left
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    1.0f, 1.0f, 0.6f, 0.2f,
    // top
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    1.0f, 1.0f, 0.9f, 0.2f,
    // bottom
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
    1.0f, 1.0f, 0.8f, 0.2f,
};

static const float
cube_colors[] =
{
    // Front
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    // Right
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    // Back
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    // Left
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    0.0625f, 0.57421875f, 0.92578125f, 1.0f,
    // Top
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    0.29296875f, 0.66796875f, 0.92578125f, 1.0f,
    // Bottom
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
    0.52734375f, 0.76171875f, 0.92578125f, 1.0f};

enum cam {OBJECTIVE_CAM = nk_false, PROJECTION_CAM = nk_true};

static struct orientation cube_transform;

static GLfloat cam_verts[] =
{
    -0.25f, 0.25f, 0.0f,   // 0 -- front
    -0.25f, -0.25f, 0.0f,  // 1
    0.25f, 0.25f, 0.0f,    // 2
    0.25f, -0.25f, 0.0f,   // 3 -- front
    0.5f, 0.5f, -0.5f,     // 4
    0.5f, -0.5f, -0.5f,    // 5
    -0.5f, 0.5f, -0.5f,    // 6
    -0.5f, -0.5f, -0.5f,   // 7
    -0.25f, 0.25f, 0.5f,   // 8 -- front
    -0.25f, -0.25f, 0.5f,  // 9
    0.25f, 0.25f, 0.5f,    // 10
    0.25f, -0.25f, 0.5f,   // 11 -- front
};


static GLushort cam_indices[] =
{
    // Right-Back
    2, 3, 4,
    4, 3, 5,
    // Back
    4, 5, 6,
    6, 5, 7,
    // Left-Back
    6, 7, 0,
    0, 7, 1,
    // Top-Back
    6, 0, 4,
    4, 0, 2,
    // Bottom-Back
    1, 7, 3,
    3, 7, 5,
    // Front
    8, 9, 10,
    10, 9, 11,
    // Right-Front
    10, 11, 2,
    2, 11, 3,
    // Left-Front
    0, 1, 8,
    8, 1, 9,
    // Top-Front
    0, 8, 2,
    2, 8, 10,
    // Bottom-Front
    9, 1, 11,
    11, 1, 3,
};

static GLushort cube_indices[] =
{
    // Front
    0, 1, 2,
    2, 1, 3,
    // Right
    2, 3, 4,
    4, 3, 5,
    // Back
    4, 5, 6,
    6, 5, 7,
    // Left
    6, 7, 0,
    0, 7, 1,
    // Top
    6, 0, 4,
    4, 0, 2,
    // Bottom
    1, 7, 3,
    3, 7, 5,
};

static GLfloat cube_verts[] =
{
    -0.5f, 0.5f, 0.5f,   // 0 -- front
    -0.5f, -0.5f, 0.5f,  // 1
    0.5f, 0.5f, 0.5f,    // 2
    0.5f, -0.5f, 0.5f,   // 3 -- front
    0.5f, 0.5f, -0.5f,   // 4
    0.5f, -0.5f, -0.5f,  // 5
    -0.5f, 0.5f, -0.5f,  // 6
    -0.5f, -0.5f, -0.5f, // 7
};

static GLfloat grid_verts[] =
{
    -5.0f, -0.5f, -5.0f,
    -5.0f, -0.5f, 5.0f,
    -4.5f, -0.5f, -5.0f,
    -4.5f, -0.5f, 5.0f,
    -4.0f, -0.5f, -5.0f,
    -4.0f, -0.5f, 5.0f,
    -3.5f, -0.5f, -5.0f,
    -3.5f, -0.5f, 5.0f,
    -3.0f, -0.5f, -5.0f,
    -3.0f, -0.5f, 5.0f,
    -2.5f, -0.5f, -5.0f,
    -2.5f, -0.5f, 5.0f,
    -2.0f, -0.5f, -5.0f,
    -2.0f, -0.5f, 5.0f,
    -1.5f, -0.5f, -5.0f,
    -1.5f, -0.5f, 5.0f,
    -1.0f, -0.5f, -5.0f,
    -1.0f, -0.5f, 5.0f,
    -0.5f, -0.5f, -5.0f,
    -0.5f, -0.5f, 5.0f,
    -0.0f, -0.5f, -5.0f,
    -0.0f, -0.5f, 5.0f,
    0.5f, -0.5f, -5.0f,
    0.5f, -0.5f, 5.0f,
    1.0f, -0.5f, -5.0f,
    1.0f, -0.5f, 5.0f,
    1.5f, -0.5f, -5.0f,
    1.5f, -0.5f, 5.0f,
    2.0f, -0.5f, -5.0f,
    2.0f, -0.5f, 5.0f,
    2.5f, -0.5f, -5.0f,
    2.5f, -0.5f, 5.0f,
    3.0f, -0.5f, -5.0f,
    3.0f, -0.5f, 5.0f,
    3.5f, -0.5f, -5.0f,
    3.5f, -0.5f, 5.0f,
    4.0f, -0.5f, -5.0f,
    4.0f, -0.5f, 5.0f,
    4.5f, -0.5f, -5.0f,
    4.5f, -0.5f, 5.0f,
    5.0f, -0.5f, -5.0f,
    5.0f, -0.5f, 5.0f,
    -5.0f, -0.5f, -5.0f,
    5.0f, -0.5f, -5.0f,
    -5.0f, -0.5f, -4.5f,
    5.0f, -0.5f, -4.5f,
    -5.0f, -0.5f, -4.0f,
    5.0f, -0.5f, -4.0f,
    -5.0f, -0.5f, -3.5f,
    5.0f, -0.5f, -3.5f,
    -5.0f, -0.5f, -3.0f,
    5.0f, -0.5f, -3.0f,
    -5.0f, -0.5f, -2.5f,
    5.0f, -0.5f, -2.5f,
    -5.0f, -0.5f, -2.0f,
    5.0f, -0.5f, -2.0f,
    -5.0f, -0.5f, -1.5f,
    5.0f, -0.5f, -1.5f,
    -5.0f, -0.5f, -1.0f,
    5.0f, -0.5f, -1.0f,
    -5.0f, -0.5f, -0.5f,
    5.0f, -0.5f, -0.5f,
    -5.0f, -0.5f, 0.0f,
    5.0f, -0.5f, 0.0f,
    -5.0f, -0.5f, 0.5f,
    5.0f, -0.5f, 0.5f,
    -5.0f, -0.5f, 1.0f,
    5.0f, -0.5f, 1.0f,
    -5.0f, -0.5f, 1.5f,
    5.0f, -0.5f, 1.5f,
    -5.0f, -0.5f, 2.0f,
    5.0f, -0.5f, 2.0f,
    -5.0f, -0.5f, 2.5f,
    5.0f, -0.5f, 2.5f,
    -5.0f, -0.5f, 3.0f,
    5.0f, -0.5f, 3.0f,
    -5.0f, -0.5f, 3.5f,
    5.0f, -0.5f, 3.5f,
    -5.0f, -0.5f, 4.0f,
    5.0f, -0.5f, 4.0f,
    -5.0f, -0.5f, 4.5f,
    5.0f, -0.5f, 4.5f,
    -5.0f, -0.5f, 5.0f,
    5.0f, -0.5f, 5.0f,
};

static struct orientation cube_transform_init = {
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    0.0f,
    0.0f
};

static struct cam_orientation obj_cam_ornt_init;
static struct cam_orientation proj_cam_ornt_init;
static struct cam_orientation obj_cam_ornt;
static struct cam_orientation proj_cam_ornt;

static struct cam_perspective proj_cam_prsp_init = {
    45.0f,
    4.0f / 3.0f,
    0.1f,
    10.0f};

static struct cam_perspective proj_cam_prsp;
static struct cam_perspective obj_cam_prsp = {
    90.0f,
    1920.0f / 1080.0f,
    0.1f,
    100.0f};

static mat4s cube_model, cube_rotate_x, cube_rotate_y, cube_rotate_z,
             cube_scale, cube_translate, cube_view, cube_projection, cube_mvp;
static vec3s cube_center;

static mat4s cam_rotation, cam_scale;
static vec3s cam_offset;

static float yaw = -90.0f;
static float pitch = 0;

static int fix_eye_cam_pos = nk_false;
static int show_cam = nk_true;

static GLfloat frustum_verts[24];
static GLushort frustum_indices[] =
{
    // Front
    0, 1, 2,
    0, 2, 3,
    1, 5, 6,
    1, 2, 6,
    1, 5, 4,
    1, 0, 4,
    3, 0, 4,
    3, 7, 4,
    2, 6, 7,
    2, 3, 7,
    4, 5, 6,
    4, 6, 7
};

static enum cam selected_cam;

static struct ogl_init frustum_init = {
    FRUSTUM,
    frustum_vert_shader,
    frustum_frag_shader,
    frustum_verts,
    sizeof(frustum_verts),
    frustum_indices,
    sizeof(frustum_indices),
    frustum_colors,
    sizeof(frustum_colors)
};

static struct ogl_init cube_init = {
    CUBE,
    common_vert_shader,
    cube_frag_shader,
    cube_verts,
    sizeof(cube_verts),
    cube_indices,
    sizeof(cube_indices),
    cube_colors,
    sizeof(cube_colors),
};

static struct ogl_init cam_init = {
    CAM,
    common_vert_shader,
    cam_frag_shader,
    cam_verts,
    sizeof(cam_verts),
    cam_indices,
    sizeof(cam_indices),
    cube_colors,
    sizeof(cube_colors),
};

static struct ogl_init grid_init = {
    GRID,
    common_vert_shader,
    grid_frag_shader,
    grid_verts,
    sizeof(grid_verts),
    NULL,
    0,
    NULL,
    0
};

static vec4s corners[8];

/* ===============================================================
 *
 *                          Functions
 *
 * ===============================================================*/

GLuint
LoadShader(GLenum type, const char *shaderSrc)
{
    GLuint shader;
    GLint compiled;
    shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    glShaderSource(shader, 1, &shaderSrc, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char *infoLog = malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            printf("Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void
init_cam()
{
    cam_rotation = glms_rotate_make(glm_rad(90), (vec3s){{0.0f, 1.0f, 0.0f}});
    cam_offset = (vec3s){{0.5f, 0.0f, 0.0f}};
    cam_scale = glms_scale_make((vec3s){{0.5f, 0.5f, 0.5f}});
}

void
init_objs()
{
    reset_proj_cam();
    reset_cube_transform();
    init_cam();
}

void
use_program(struct ogl *obj)
{
    obj->matrixID = glGetUniformLocation(obj->program, "mvp");
    glUseProgram(obj->program);
}

void
bind_vertices(struct ogl *obj)
{
    glEnableVertexAttribArray(obj->vertexLoc);
    glBindBuffer(GL_ARRAY_BUFFER, obj->vboID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->idoID);

    glVertexAttribPointer(obj->vertexLoc,
                          3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), 0);
}

bool
init_indices(struct ogl *draw_data, struct ogl_init *init_data)
{
    glGenBuffers(1, &(draw_data->idoID));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data->idoID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, init_data->index_len, init_data->indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* TODO: this should be separated out and called where needed */
    const float *colors = init_data->colors;
    if (colors == NULL)
    {
        fprintf(stderr, "Object was not provided color values\n");
        return false;
    }

    glGenBuffers(1, &draw_data->colorID);
    glBindBuffer(GL_ARRAY_BUFFER, draw_data->colorID);
    glBufferData(GL_ARRAY_BUFFER, init_data->color_len, colors, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void
init_color(struct ogl *obj)
{
    glEnableVertexAttribArray(obj->colorLoc);
    glBindBuffer(GL_ARRAY_BUFFER, obj->colorID);
    glVertexAttribPointer(obj->colorLoc,
                          4, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), 0);
}

bool
init_shader(struct ogl *draw_data, struct ogl_init *init_data)
{
    unsigned int vtx_shader;
    unsigned int frag_shader;

    vtx_shader = LoadShader(GL_VERTEX_SHADER, init_data->vert_shader);
    frag_shader = LoadShader(GL_FRAGMENT_SHADER, init_data->frag_shader);
    draw_data->program = glCreateProgram();
    if (draw_data->program == 0)
        return 0;
    glAttachShader(draw_data->program, vtx_shader);
    glAttachShader(draw_data->program, frag_shader);
    glBindAttribLocation(draw_data->program, 0, "vPosition");
    glLinkProgram(draw_data->program);
    GLint linked;
    glGetProgramiv(draw_data->program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(draw_data->program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char *infoLog = malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(draw_data->program, infoLen, NULL, infoLog);
            fprintf(stderr, "Error linking program:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteProgram(draw_data->program);
        return false;
    }
    draw_data->mvpLoc = glGetUniformLocation(draw_data->program, "mvp");
    return true;
}

bool
init_frustum(struct ogl *draw_data, struct ogl_init *init_data)
{
    init_shader(draw_data,init_data);
    draw_data->colorLoc = glGetAttribLocation(draw_data->program, "a_color");
    glGenBuffers(1, &(draw_data->vboID));
    glBindBuffer(GL_ARRAY_BUFFER, draw_data->vboID);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    init_indices(draw_data,init_data);
    draw_data->init_data = init_data;
    return true;
}

bool
init_verts(struct ogl *draw_data, struct ogl_init *init_data)
{
    init_shader(draw_data,init_data);
    draw_data->vertexLoc = glGetAttribLocation(draw_data->program, "a_position");
    glGenBuffers(1, &(draw_data->vboID));
    glBindBuffer(GL_ARRAY_BUFFER, draw_data->vboID);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    draw_data->init_data = init_data;
    return true;
}


bool
init_cube(struct ogl *draw_data, struct ogl_init *init_data)
{
    init_shader(draw_data,init_data);
    draw_data->vertexLoc = glGetAttribLocation(draw_data->program, "a_position");
    draw_data->colorLoc = glGetAttribLocation(draw_data->program, "a_color");
    glGenBuffers(1, &(draw_data->vboID));
    glBindBuffer(GL_ARRAY_BUFFER, draw_data->vboID);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    init_indices(draw_data,init_data);
    draw_data->init_data = init_data;
    return true;
}

void
update_frustum_buffer()
{
    glBindBuffer(GL_ARRAY_BUFFER, objs.frustum.vboID);
    glBufferData(GL_ARRAY_BUFFER, frustum_init.vert_len, frustum_init.verts, GL_STATIC_DRAW);
}

/*
 * CGLM's perspective function isn't giving me the right numbers
 * TODO: double check why and file a bug if I wasn't doing it wrong
 */
mat4s
perspective(float FOV, float AspectRatio, float Near, float Far)
{
    mat4s result = GLMS_MAT4_ZERO;
    float Cotangent = 1.0f / tanf(FOV * (GLM_PI / 360.0f));

    result.raw[0][0] = Cotangent / AspectRatio;
    result.raw[1][1] = Cotangent;
    result.raw[2][3] = -1.0f;
    result.raw[2][2] = (Near + Far) / (Near - Far);
    result.raw[3][2] = (2.0f * Near * Far) / (Near - Far);
    result.raw[3][3] = 0.0f;

    return result;
}

mat4s
calc_cam_mvp()
{
    mat4s model = GLMS_MAT4_IDENTITY;
    mat4s translation = glms_translate_make(glms_vec3_add(proj_cam_ornt.eye, cam_offset));
    model = glms_mat4_mulN((mat4s *[]){&translation, &cam_rotation, &cam_scale, &model},3);
    mat4s view = glms_lookat(obj_cam_ornt.eye, glms_vec3_add(obj_cam_ornt.center, obj_cam_ornt.eye), obj_cam_ornt.up);
    mat4s projection = perspective(obj_cam_prsp.fov, obj_cam_prsp.aspect_ratio,
                                   obj_cam_prsp.near, obj_cam_prsp.far);

    mat4s mvp = glms_mat4_mulN((mat4s *[]){&projection, &view, &model},3);
    return mvp;
}

mat4s
calc_cube_mvp(struct cam_orientation *ornt,
              struct cam_perspective *prsp)
{
    cube_center = glms_vec3_add(ornt->center, ornt->eye);
    cube_rotate_x = glms_rotate_make(cube_transform.rx * 30, (vec3s){{1.0f, 0.0f, 0.0f}});
    cube_rotate_y = glms_rotate_make(cube_transform.ry * 30, (vec3s){{0.0f, 1.0f, 0.0f}});
    cube_rotate_z = glms_rotate_make(cube_transform.rz * 30, (vec3s){{0.0f, 0.0f, 1.0f}});

    cube_translate = glms_translate_make((vec3s){{cube_transform.tx,cube_transform.ty,cube_transform.tz}});

    cube_model = glms_mat4_mulN((mat4s *[]){&cube_translate, &cube_rotate_z, &cube_rotate_y, &cube_rotate_x, &cube_scale}, 5);

    cube_scale = glms_scale_make((vec3s){{cube_transform.sx,cube_transform.sy,cube_transform.sz}});

    cube_view = glms_lookat(ornt->eye,cube_center,(vec3s){{0,1,0}});
    cube_projection = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    cube_mvp = glms_mat4_mulN((mat4s *[]){&cube_projection, &cube_view, &cube_model }, 3);
    return cube_mvp;
}

mat4s
calc_grid_mvp(struct cam_orientation *ornt,
              struct cam_perspective *prsp)
{
    mat4s view, proj;
    vec3s center;
    center = glms_vec3_add(ornt->center, ornt->eye);

    view = glms_lookat(ornt->eye,center,(vec3s){{0,1,0}});
    proj = glms_perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    return glms_mat4_mulN((mat4s *[]){&proj, &view}, 2);
}

mat4s
calc_frustum_mvp()
{
    mat4s model = GLMS_MAT4_IDENTITY;
    mat4s view = glms_lookat(obj_cam_ornt.eye, glms_vec3_add(obj_cam_ornt.center, obj_cam_ornt.eye), obj_cam_ornt.up);
    mat4s projection = perspective(obj_cam_prsp.fov, obj_cam_prsp.aspect_ratio,
                                   obj_cam_prsp.near, obj_cam_prsp.far);

    mat4s mvp = glms_mat4_mulN((mat4s *[]){&projection, &view, &model},3);
    return mvp;
}


void
set_frustum_verts()
{
    mat4s view = glms_lookat(proj_cam_ornt.eye, glms_vec3_add(proj_cam_ornt.center, proj_cam_ornt.eye), proj_cam_ornt.up);
    mat4s proj = perspective(proj_cam_prsp.fov, proj_cam_prsp.aspect_ratio,
                             proj_cam_prsp.near, proj_cam_prsp.far);
    mat4s viewproj = glms_mat4_mul(proj, view);
    mat4s inv = glms_mat4_inv(viewproj);
    glms_frustum_corners(inv,corners);
    int i,j,index;
    index = 0;

    for(i = 0; i < 8; i++) {
        for(j = 0; j < 3; j++) {
            frustum_verts[index] = corners[i].raw[j];
            index++;
        }
    }
}

void
reset_cube_transform()
{
    cube_transform.rx = cube_transform_init.rx;
    cube_transform.ry = cube_transform_init.ry;
    cube_transform.rz = cube_transform_init.rz;
    cube_transform.tx = cube_transform_init.tx;
    cube_transform.ty = cube_transform_init.ty;
    cube_transform.tz = cube_transform_init.tz;
    cube_transform.sx = cube_transform_init.sx;
    cube_transform.sy = cube_transform_init.sy;
    cube_transform.sz = cube_transform_init.sz;
}

void
reset_proj_cam()
{
    proj_cam_prsp.aspect_ratio = proj_cam_prsp_init.aspect_ratio;
    proj_cam_prsp.fov = proj_cam_prsp_init.fov;
    proj_cam_prsp.near = proj_cam_prsp_init.near;
    proj_cam_prsp.far = proj_cam_prsp_init.far;
    proj_cam_ornt.center = proj_cam_ornt_init.center;
    proj_cam_ornt.eye = proj_cam_ornt_init.eye;
    proj_cam_ornt.up = proj_cam_ornt_init.up;
    fix_eye_cam_pos = nk_false;
    show_cam = nk_true;
}


void
draw_vert_lines(struct ogl *obj, mat4s mvp)
{
    glUniformMatrix4fv(obj->matrixID, 1, GL_FALSE, mvp.raw[0]);
    glDrawArrays(GL_LINES, 0, (obj->init_data->vert_len) / 3);
}

void
draw_triangle_indices(struct ogl *obj, mat4s mvp)
{
    glUniformMatrix4fv(obj->matrixID, 1, GL_FALSE, mvp.raw[0]);
    glDrawElements(GL_TRIANGLES, obj->init_data->index_len, GL_UNSIGNED_SHORT, 0);
}

void
draw_cam(struct ogl *obj)
{
    use_program(obj);
    bind_vertices(obj);
    init_color(obj);
    mat4s mvp = calc_cam_mvp();
    draw_triangle_indices(obj,mvp);
}

void
draw_cube(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp)
{
    use_program(obj);
    bind_vertices(obj);
    init_color(obj);
    mat4s mvp = calc_cube_mvp(ornt, prsp);
    draw_triangle_indices(obj,mvp);
}

void
draw_frustum(struct ogl *obj)
{
    use_program(obj);
    bind_vertices(obj);
    init_color(obj);
    mat4s mvp = calc_frustum_mvp();
    draw_triangle_indices(obj,mvp);
}

void
draw_grid(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp)
{
    glDisable(GL_BLEND);
    use_program(obj);
    bind_vertices(obj);
    mat4s mvp;
    mvp = calc_grid_mvp( ornt,prsp );
    draw_vert_lines(obj,mvp);
}

/* ===============================================================
 *
 *                          DEMO
 *
 * ===============================================================*/

/* Platform */
SDL_Window *win;
int running = nk_true;
bool lc_down = false;

void
MainLoop(void *loopArg)
{
    set_frustum_verts();
    update_frustum_buffer();
    struct nk_context *ctx = (struct nk_context *)loopArg;
    int x, y;
    int lastx, lasty;

    /* Input */
    SDL_Event evt;
    nk_input_begin(ctx);

    while (SDL_PollEvent(&evt))
    {
        switch (evt.type)
        {
        case SDL_QUIT:
            running = nk_false;
            break;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
        {
            switch (evt.key.keysym.sym)
            {
            case SDLK_e:
                printf("Error: %s\n", SDL_GetError());
                break;
            case SDLK_q:
                running = nk_false;
                break;
                //TODO: make WASD movement additive
            case SDLK_w:
            {
                vec3s delta, eye;
                delta = glms_vec3_scale(obj_cam_ornt.center,MOVESPEED);
                eye = glms_vec3_add(obj_cam_ornt.eye, delta);
                obj_cam_ornt.eye = eye;
                break;
            }
            case SDLK_a:
            {
                vec3s left = glms_normalize(glms_cross(obj_cam_ornt.center, obj_cam_ornt.up));
                vec3s delta = glms_vec3_scale(left,MOVESPEED);
                obj_cam_ornt.eye = glms_vec3_sub(obj_cam_ornt.eye, delta);
                break;
            }
            case SDLK_s:
            {
                vec3s delta = glms_vec3_scale(obj_cam_ornt.center,MOVESPEED);
                obj_cam_ornt.eye = glms_vec3_sub(obj_cam_ornt.eye, delta);
                break;
            }
            case SDLK_d:
            {
                vec3s left = glms_normalize(glms_cross(obj_cam_ornt.center, obj_cam_ornt.up));
                vec3s delta = glms_vec3_scale(left,MOVESPEED);
                obj_cam_ornt.eye = glms_vec3_add(obj_cam_ornt.eye, delta);
                break;
            }
            }
        }
        case SDL_MOUSEBUTTONDOWN:
        {
            if (!nk_window_is_any_hovered(ctx) && !lc_down && evt.button.button == SDL_BUTTON_LEFT)
            {
                SDL_GetMouseState(&x, &y);
                lc_down = true;
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:
            if (lc_down && evt.button.button == SDL_BUTTON_LEFT)
                lc_down = false;
            break;
        case SDL_MOUSEMOTION:
            if (lc_down == true)
            {
                lastx = x;
                lasty = y;
                SDL_GetMouseState(&x, &y);
                int xoffset = x - lastx;
                int yoffset = lasty - y;
                yaw += (float)xoffset;
                pitch += (float)yoffset;
                obj_cam_ornt.center.x = cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
                obj_cam_ornt.center.y = sinf(glm_rad(pitch));
                obj_cam_ornt.center.z = sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));
                obj_cam_ornt.center = glms_normalize(obj_cam_ornt.center);
            }
            break;
        }
        nk_sdl_handle_event(&evt);
    }
    nk_input_end(ctx);

    /* GUI */
    /* TODO: all controls go here */
    int window_flags = 0;
    window_flags |= NK_WINDOW_BORDER;
    window_flags |= NK_WINDOW_SCALABLE;
    window_flags |= NK_WINDOW_MOVABLE;
    window_flags |= NK_WINDOW_SCALE_LEFT;
    window_flags |= NK_WINDOW_MINIMIZABLE;

    if (nk_begin(ctx, "Controls", nk_rect(0, 0, 250, 830), window_flags))
    {
        {
            if (nk_tree_push(ctx, NK_TREE_TAB, "Cube", NK_MAXIMIZED)) {
                if (nk_button_label(ctx, "Reset Values"))
                    reset_cube_transform();
                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Translate x", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.tx, 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Translate y", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.ty, 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Translate z", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.tz, 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Scale x", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, 0, &cube_transform.sx, 5.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Scale y", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, 0.0f, &cube_transform.sy, 5.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Scale z", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, 0.0f, &cube_transform.sz, 5.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Rotate x", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.rx, 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Rotate y", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.ry, 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Rotate z", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                nk_slider_float(ctx, -10.0f, &cube_transform.rz, 10.0f, 0.01f);
                nk_tree_pop(ctx);
            }

            if (nk_tree_push(ctx, NK_TREE_TAB, "Scene Camera", NK_MAXIMIZED))
            {
                if (nk_button_label(ctx, "Reset Values")) {
                    fprintf(stdout, "button pressed\n");
                    reset_proj_cam();
                }
                nk_checkbox_label(ctx, "Fix Eye to Scene Camera", (nk_bool*)&selected_cam);
                nk_checkbox_label(ctx, "Show Scene Camera", &show_cam);

                nk_layout_row_static(ctx, 30, 80, 2);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos x", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.x), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos y", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.y), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos z", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.z), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "fov", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, 30.0f, &(proj_cam_prsp.fov), 120.0f, 1.0f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Aspect Ratio", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, 1.0f, &(proj_cam_prsp.aspect_ratio), 2.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "near", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, 0.1f, &(proj_cam_prsp.near), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "far", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, 0.0f, &proj_cam_prsp.far, 15.0f, 0.1f);
                nk_tree_pop(ctx);
            }
        }
    }
    nk_end(ctx);

    if (nk_begin(ctx, "Matrices", nk_rect(0, 830, 1920, 250),
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE |
                     NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MINIMIZABLE))
    {
        nk_layout_row_dynamic(ctx, 250, 3);

        nk_group_begin(ctx, "Model", NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR);
        nk_layout_row_static(ctx, 30, 50, 4);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m00);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m10);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m20);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m30);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m01);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m11);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m21);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m31);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m02);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m12);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m22);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m32);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m03);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m13);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m23);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.m33);
        nk_group_end(ctx);

        nk_group_begin(ctx, "View", NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR);
        nk_layout_row_static(ctx, 30, 50, 4);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m00);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m10);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m20);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m30);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m01);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m11);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m21);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m31);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m02);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m12);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m22);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m32);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m03);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m13);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m23);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.m33);
        nk_group_end(ctx);

        nk_group_begin(ctx, "Projection", NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR);
        nk_layout_row_static(ctx, 30, 50, 4);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m00);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m10);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m20);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m30);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m01);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m11);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m21);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m31);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m02);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m12);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m22);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m32);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m03);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m13);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m23);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.m33);
        nk_group_end(ctx);
    }
    nk_end(ctx);

    /* Draw */
    {
        float bg[4];
        int win_width, win_height;
        /* nk_color_fv(bg, nk_rgb(28, 48, 62)); */
        nk_color_fv(bg, nk_rgb(0, 0, 0));
        SDL_GetWindowSize(win, &win_width, &win_height);
        glViewport(0, 0, win_width, win_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg[0], bg[1], bg[2], bg[3]);

        if (selected_cam == OBJECTIVE_CAM)
        {
            draw_grid(&(objs.grid), &obj_cam_ornt, &obj_cam_prsp);
            calc_frustum_mvp();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            draw_cube(&(objs.cube), &obj_cam_ornt, &obj_cam_prsp);
            if (show_cam)
            {
                draw_cam(&(objs.cam));
                draw_frustum(&(objs.frustum));
            }
            glFlush();
        }
        else if (selected_cam == PROJECTION_CAM)
        {
            // TODO: Fix rendering bug with FOV
            /*  draw_grid(&(objs.grid), &proj_cam_ornt, &proj_cam_prsp); */
            draw_cube(&(objs.cube), &proj_cam_ornt, &proj_cam_prsp);
        }
        /* IMPORTANT: `nk_sdl_render` modifies some global OpenGL state
         * with blending, scissor, face culling, depth test and viewport and
         * defaults everything back into a default state.
         * Make sure to either a.) save and restore or b.) reset your own state after
         * rendering the UI. */
        nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
        SDL_GL_SwapWindow(win);
    }
}

int
main(int argc, char *argv[])
{
    init_objs();
    proj_cam_ornt = proj_cam_ornt_init = (struct cam_orientation){
        (vec3s){{3.5f, 0.0f, 0.0f}},
        (vec3s){{-1.0f, 0.0f, 0.0f}},
        (vec3s){{0.0f, 1.0f, 0.0f}},
    };
    obj_cam_ornt = obj_cam_ornt_init = (struct cam_orientation){
        (vec3s){{5.75f, 2.0f, 4.0f}},
        (vec3s){{-5.0f, -2.0f, -4.0f}},
        (vec3s){{0.0f, 1.0f, 0.0f}},
    };

    /* GUI */
    struct nk_context *ctx;
    SDL_GLContext glContext;

    NK_UNUSED(argc);
    NK_UNUSED(argv);

    /* SDL setup */
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("Demo",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           WINDOW_WIDTH, WINDOW_HEIGHT,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    glContext = SDL_GL_CreateContext(win);

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    init_cube(&(objs.cube), &cube_init);
    init_cube(&(objs.cam), &cam_init);
    init_verts(&(objs.grid), &grid_init);
    set_frustum_verts();
    init_frustum(&(objs.frustum), &frustum_init);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    ctx = nk_sdl_init(win);
    /* Load Fonts: if none of these are loaded a default font will be used  */
    /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    {
        struct nk_font_atlas *atlas;
        nk_sdl_font_stash_begin(&atlas);
        nk_sdl_font_stash_end();
        ;
    }

// Currently broken
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
    emscripten_set_main_loop_arg(MainLoop, (void *)ctx, 0, nk_true);
#else
    while (running)
        MainLoop((void *)ctx);
#endif

    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
