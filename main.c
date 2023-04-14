/* Adapted from https://github.com/Immediate-Mode-UI/Nuklear/tree/master/demo/sdl_opengles2 */

#define GL_SILENCE_DEPRECATION
#define DEBUG 0
#include "HandmadeMath.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION

#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

/* near */
static hmm_vec4 CSCOORD_LBN, CSCOORD_LTN, CSCOORD_RTN, CSCOORD_RBN,
    CSCOORD_LBF, CSCOORD_LTF, CSCOORD_RTF, CSCOORD_RBF;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define MOVESPEED 0.1f

#define UNUSED(a) (void)a
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])

/* ===============================================================
 *
 *                          Debug
 *
 * ===============================================================*/
static GLenum error;
static void glDebugStuff(void)
{
    if (error != GL_NO_ERROR) {
        printf("OpenGL error: %d\n", error);
        exit(1);
    } else {
        printf("No OpenGL error\n");
    }
}

void printMat4(hmm_mat4 m) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            printf("%.2f ", m.Elements[row][col]);
        }
        printf("\n");
    }
    printf("\n");
}
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
    const GLuint *indices;
    size_t index_len;
};

struct ogl
{
    struct ogl_init *init_data;
    unsigned int program;
    unsigned int matrixID;
    unsigned int VAO;
    unsigned int VBO;
    unsigned int IBO;
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
    hmm_vec3 eye;
    hmm_vec3 center;
    hmm_vec3 up;
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
static void bind_vertices(struct ogl *obj);
static bool init_indices(struct ogl *draw_data, struct ogl_init *init_data);
static void init_color(struct ogl *obj);
static bool init_shader(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_frustum(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_indices(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_grid(struct ogl *draw_data, struct ogl_init *init_data);
static bool init_cube(struct ogl *draw_data, struct ogl_init *init_data);
static void update_frustum_buffer();
static hmm_mat4 perspective(float FOV, float AspectRatio, float Near, float Far);
static hmm_mat4 calc_cube_mvp(struct cam_orientation *ornt, struct cam_perspective *prsp);
static hmm_mat4 calc_grid_mvp(struct cam_orientation *ornt, struct cam_perspective *prsp);
static hmm_mat4 calc_frustum_mvp();
static void set_frustum_verts();
static void reset_cube_transform();
static void reset_proj_cam();
static void draw_triangle_indices(struct ogl *obj, hmm_mat4 mvp);
static void draw_cube(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp);
static void draw_frustum(struct ogl *obj);
static void draw_grid(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp);
static void MainLoop(void *loopArg);

/* ===============================================================
 *
 *                          Variables
 *
 * ===============================================================*/

static const char grid_frag_shader[] =
    "#version 330 core\n"
    "out vec3 color;\n"
    "void main(){\n"
    "	color = vec3(1.0,0.0,0.0);\n"
    "}\n";

static const char vp_vert_shader[] =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * view * vec4(aPos, 1.0);\n"
        "}\n";

static const char color_mvp_vert_shader[] =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec4 aColor;\n"
        "out vec4 vertexColor;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "    vertexColor = aColor;\n"
        "}\n";


static const char mvp_vert_shader[] =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "}\n";


static const char color_vp_vert_shader[] =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec4 aColor;\n"
    "out vec4 vertexColor;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    gl_Position = projection * view * vec4(aPos, 1.0);\n"
    "    vertexColor = aColor;\n"
    "}\n";

static const char color_in_shader[] =
    "#version 330 core\n"
    "in vec4 vertexColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vertexColor;\n"
    "}\n";

static const char cam_frag_shader[] =
    "#version 410\n"
    "in vec4 v_color;"
    "out vec4 frag_color;"
    "void main()"
    "{"
    "frag_color = vec4(0.5,0.5,0.5,1.0);"
    "}";

static GLfloat cube_vertices[] = {
        // Positions        // Colors
        -0.5f, -0.5f, -0.5f, 0.0625f, 0.57421875f, 0.92578125f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0625f, 0.57421875f, 0.92578125f, 1.0f,
        0.5f,  0.5f, -0.5f, 0.0625f, 0.57421875f, 0.92578125f, 1.0f,
        -0.5f,  0.5f, -0.5f, 0.0625f, 0.57421875f, 0.92578125f, 1.0f,
        -0.5f, -0.5f,  0.5f, 0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
        0.5f, -0.5f,  0.5f, 0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
        0.5f,  0.5f,  0.5f, 0.52734375f, 0.76171875f, 0.92578125f, 1.0f,
        -0.5f,  0.5f,  0.5f, 0.52734375f, 0.76171875f, 0.92578125f, 1.0f
};

static GLuint cube_indices[] = {
        0, 1, 2, 2, 3, 0, // Front
        5, 4, 7, 7, 6, 5, // Back
        7, 4, 0, 0, 3, 7, // Left
        1, 5, 6, 6, 2, 1, // Right
        3, 2, 6, 6, 7, 3, // Top
        4, 5, 1, 1, 0, 4, // Bottom
};
static GLfloat frustum_verts[] = {
        -0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        -0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        -0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
        -0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.6f, 0.2f,
};

static GLuint frustum_indices[] = {
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

static GLfloat cam_verts[] = {
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


static GLuint cam_indices[] = {
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

static GLfloat grid_verts[] = {
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

enum cam {OBJECTIVE_CAM = nk_true, PROJECTION_CAM = nk_false};

static struct orientation cube_transform;

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

static hmm_mat4 cube_model, cube_rotate_x, cube_rotate_y, cube_rotate_z,
    cube_scale, cube_translate, cube_view, cube_projection;
static hmm_vec3 cube_center;

static hmm_mat4 cam_rotation, cam_scale;
static hmm_vec3 cam_offset;

static float yaw = -90.0f;
static float pitch = 0;

static int fix_eye_cam_pos = nk_false;
static int show_cam = nk_true;


static enum cam selected_cam = OBJECTIVE_CAM;

static struct ogl_init frustum_init = {
        FRUSTUM,
        color_vp_vert_shader,
        color_in_shader,
        frustum_verts,
        sizeof(frustum_verts),
        frustum_indices,
        sizeof(frustum_indices),
};

static struct ogl_init cube_init = {
        CUBE,
        color_mvp_vert_shader,
        color_in_shader,
        cube_vertices,
        sizeof(cube_vertices),
        cube_indices,
        sizeof(cube_indices),
};

static struct ogl_init cam_init = {
        CAM,
        mvp_vert_shader,
        cam_frag_shader,
        cam_verts,
        sizeof(cam_verts),
        cam_indices,
        sizeof(cam_indices),
};

static struct ogl_init grid_init = {
        GRID,
        vp_vert_shader,
        grid_frag_shader,
        grid_verts,
        sizeof(grid_verts),
        NULL,
        0,
};

static hmm_vec4 corners[8];
static GLenum error;

/* ===============================================================
 *
 *                          Math Functions
 *
 * ===============================================================*/

hmm_mat4
hmm_mat4_inv(hmm_mat4 mat, hmm_mat4 dest) {
    float t[6];
    float det;
    float a = mat.Elements[0][0], b = mat.Elements[0][1], c = mat.Elements[0][2], d = mat.Elements[0][3],
        e = mat.Elements[1][0], f = mat.Elements[1][1], g = mat.Elements[1][2], h = mat.Elements[1][3],
        i = mat.Elements[2][0], j = mat.Elements[2][1], k = mat.Elements[2][2], l = mat.Elements[2][3],
        m = mat.Elements[3][0], n = mat.Elements[3][1], o = mat.Elements[3][2], p = mat.Elements[3][3];

    t[0] = k * p - o * l; t[1] = j * p - n * l; t[2] = j * o - n * k;
    t[3] = i * p - m * l; t[4] = i * o - m * k; t[5] = i * n - m * j;

    dest.Elements[0][0] =  f * t[0] - g * t[1] + h * t[2];
    dest.Elements[1][0] =-(e * t[0] - g * t[3] + h * t[4]);
    dest.Elements[2][0] =  e * t[1] - f * t[3] + h * t[5];
    dest.Elements[3][0] =-(e * t[2] - f * t[4] + g * t[5]);

    dest.Elements[0][1] =-(b * t[0] - c * t[1] + d * t[2]);
    dest.Elements[1][1] =  a * t[0] - c * t[3] + d * t[4];
    dest.Elements[2][1] =-(a * t[1] - b * t[3] + d * t[5]);
    dest.Elements[3][1] =  a * t[2] - b * t[4] + c * t[5];

    t[0] = g * p - o * h; t[1] = f * p - n * h; t[2] = f * o - n * g;
    t[3] = e * p - m * h; t[4] = e * o - m * g; t[5] = e * n - m * f;

    dest.Elements[0][2] =  b * t[0] - c * t[1] + d * t[2];
    dest.Elements[1][2] =-(a * t[0] - c * t[3] + d * t[4]);
    dest.Elements[2][2] =  a * t[1] - b * t[3] + d * t[5];
    dest.Elements[3][2] =-(a * t[2] - b * t[4] + c * t[5]);

    t[0] = g * l - k * h; t[1] = f * l - j * h; t[2] = f * k - j * g;
    t[3] = e * l - i * h; t[4] = e * k - i * g; t[5] = e * j - i * f;

    dest.Elements[0][3] =-(b * t[0] - c * t[1] + d * t[2]);
    dest.Elements[1][3] =  a * t[0] - c * t[3] + d * t[4];
    dest.Elements[2][3] =-(a * t[1] - b * t[3] + d * t[5]);
    dest.Elements[3][3] =  a * t[2] - b * t[4] + c * t[5];

    det = 1.0f / (a * dest.Elements[0][0] + b * dest.Elements[1][0]
                  + c * dest.Elements[2][0] + d * dest.Elements[3][0]);

    hmm_mat4 result = HMM_MultiplyMat4f(dest, det);
    return result;
}

/*!
 * @brief extracts view frustum corners using clip-space coordinates
 *
 * corners' space:
 *  1- if m = invViewProj: World Space
 *  2- if m = invMVP:      Object Space
 *
 * You probably want to extract corners in world space so use invViewProj
 * Computing invViewProj:
 *   glm_mat4_mul(proj, view, viewProj);
 *   ...
 *   glm_mat4_inv(viewProj, invViewProj);
 *
 * if you have a near coord at i index, you can get it's far coord by i + 4
 *
 * Find center coordinates:
 *   for (j = 0; j < 4; j++) {
 *     glm_vec3_center(corners[i], corners[i + 4], centerCorners[i]);
 *   }
 *
 * @param[in]  invMat matrix (see brief)
 * @param[out] dest   exracted view frustum corners (see brief)
 */
void
hmm_frustum_corners(hmm_mat4 invMat, hmm_vec4 dest[8]) {
    hmm_vec4 c[8];

    /* indexOf(nearCoord) = indexOf(farCoord) + 4 */
    hmm_vec4 csCoords[8] = {
        CSCOORD_LBN,
        CSCOORD_LTN,
        CSCOORD_RTN,
        CSCOORD_RBN,

        CSCOORD_LBF,
        CSCOORD_LTF,
        CSCOORD_RTF,
        CSCOORD_RBF
    };

    c[0] = HMM_MultiplyMat4ByVec4(invMat,csCoords[0]);
    c[1] = HMM_MultiplyMat4ByVec4(invMat,csCoords[1]);
    c[2] = HMM_MultiplyMat4ByVec4(invMat,csCoords[2]);
    c[3] = HMM_MultiplyMat4ByVec4(invMat,csCoords[3]);
    c[4] = HMM_MultiplyMat4ByVec4(invMat,csCoords[4]);
    c[5] = HMM_MultiplyMat4ByVec4(invMat,csCoords[5]);
    c[6] = HMM_MultiplyMat4ByVec4(invMat,csCoords[6]);
    c[7] = HMM_MultiplyMat4ByVec4(invMat,csCoords[7]);

    dest[0] = HMM_MultiplyVec4f(c[0], 1.0f / c[0].W);
    dest[1] = HMM_MultiplyVec4f(c[1], 1.0f / c[1].W);
    dest[2] = HMM_MultiplyVec4f(c[2], 1.0f / c[2].W);
    dest[3] = HMM_MultiplyVec4f(c[3], 1.0f / c[3].W);
    dest[4] = HMM_MultiplyVec4f(c[4], 1.0f / c[4].W);
    dest[5] = HMM_MultiplyVec4f(c[5], 1.0f / c[5].W);
    dest[6] = HMM_MultiplyVec4f(c[6], 1.0f / c[6].W);
    dest[7] = HMM_MultiplyVec4f(c[7], 1.0f / c[7].W);
}

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
    cam_rotation = HMM_Rotate(90, HMM_Vec3(0, 1, 0));
    cam_offset = HMM_Vec3(0.5f, 0.0f, 0.0f);
    cam_scale = HMM_Scale(HMM_Vec3(0.5f, 0.5f, 0.5f));
}

void
init_corners()
{
    CSCOORD_LBN= HMM_Vec4(-1.0f, -1.0f, -1.0f, 1.0f);
    CSCOORD_LTN= HMM_Vec4(-1.0f,  1.0f, -1.0f, 1.0f);
    CSCOORD_RTN= HMM_Vec4(1.0f,  1.0f, -1.0f, 1.0f);
    CSCOORD_RBN= HMM_Vec4(1.0f, -1.0f, -1.0f, 1.0f);
    CSCOORD_LBF= HMM_Vec4(-1.0f, -1.0f,  1.0f, 1.0f);
    CSCOORD_LTF= HMM_Vec4(-1.0f,  1.0f,  1.0f, 1.0f);
    CSCOORD_RTF= HMM_Vec4(1.0f,  1.0f,  1.0f, 1.0f);
    CSCOORD_RBF= HMM_Vec4(1.0f, -1.0f,  1.0f, 1.0f);
}

void
init_objs()
{
    reset_proj_cam();
    reset_cube_transform();
    init_cam();
    init_corners();
}

void
bind_vertices(struct ogl *obj)
{
    glBindBuffer(GL_ARRAY_BUFFER, obj->VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->IBO);

    glVertexAttribPointer(0,
                          3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), 0);
}

bool
init_indices(struct ogl *draw_data, struct ogl_init *init_data)
{
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, init_data->index_len, init_data->indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* TODO: this should be separated out and called where needed */

    return true;
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
    return true;
}

bool
init_frustum(struct ogl *draw_data, struct ogl_init *init_data)
{
    if(!init_shader(draw_data,init_data)) {
        fprintf(stderr, "Shader error\n");
    }

    glGenVertexArrays(1, &(draw_data->VAO));
    glGenBuffers(1, &(draw_data->VBO));
    glGenBuffers(1, &(draw_data->IBO));

    glBindVertexArray(draw_data->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, draw_data->VBO);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, init_data->index_len, init_data->indices, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);

    draw_data->init_data = init_data;
    return true;
}

bool
init_grid(struct ogl *draw_data, struct ogl_init *init_data)
{
    if(!init_shader(draw_data,init_data)) {
        fprintf(stderr, "Shader error\n");
    }
    glGenVertexArrays(1, &(draw_data->VAO));
    glGenBuffers(1, &(draw_data->VBO));

    glBindVertexArray(draw_data->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, draw_data->VBO);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);


    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    draw_data->init_data = init_data;
    return true;
}

bool
init_cam_gl(struct ogl *draw_data, struct ogl_init *init_data)
{
    if(!init_shader(draw_data,init_data)) {
        fprintf(stderr, "Shader error\n");
    }

    glGenVertexArrays(1, &(draw_data->VAO));
    glGenBuffers(1, &(draw_data->VBO));
    glGenBuffers(1, &(draw_data->IBO));

    glBindVertexArray(draw_data->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, draw_data->VBO);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, init_data->index_len, init_data->indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    draw_data->init_data = init_data;
    return true;
}


bool
init_cube(struct ogl *draw_data, struct ogl_init *init_data)
{
    if(!init_shader(draw_data,init_data)) {
        fprintf(stderr, "Shader error\n");
    }

    glGenVertexArrays(1, &(draw_data->VAO));
    glGenBuffers(1, &(draw_data->VBO));
    glGenBuffers(1, &(draw_data->IBO));

    glBindVertexArray(draw_data->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, draw_data->VBO);
    glBufferData(GL_ARRAY_BUFFER, init_data->vert_len, init_data->verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, init_data->index_len, init_data->indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    draw_data->init_data = init_data;
    return true;
}

void
update_frustum_buffer()
{
    glBindBuffer(GL_ARRAY_BUFFER, objs.frustum.VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, frustum_init.vert_len, frustum_init.verts);
}

/*
 * CGLM's perspective function isn't giving me the right numbers
 * TODO: double check why and file a bug if I wasn't doing it wrong
 */
hmm_mat4
perspective(float FOV, float AspectRatio, float Near, float Far)
{
    hmm_mat4 result = HMM_Mat4();
    float Cotangent = 1.0f / tanf(FOV * (HMM_PI / 360.0f));

    result.Elements[0][0] = Cotangent / AspectRatio;
    result.Elements[1][1] = Cotangent;
    result.Elements[2][3] = -1.0f;
    result.Elements[2][2] = (Near + Far) / (Near - Far);
    result.Elements[3][2] = (2.0f * Near * Far) / (Near - Far);
    result.Elements[3][3] = 0.0f;

    return result;
}

hmm_mat4
calc_cam_mvp()
{
    hmm_mat4 translation = HMM_Translate(HMM_AddVec3(proj_cam_ornt.eye, cam_offset));
    hmm_mat4 rsm = HMM_MultiplyMat4(cam_rotation,cam_scale);
    hmm_mat4 model = HMM_MultiplyMat4(translation,rsm);
    hmm_mat4 view = HMM_LookAt(obj_cam_ornt.eye, HMM_AddVec3(obj_cam_ornt.center, obj_cam_ornt.eye), obj_cam_ornt.up);
    hmm_mat4 projection = perspective(obj_cam_prsp.fov, obj_cam_prsp.aspect_ratio,
                                     obj_cam_prsp.near, obj_cam_prsp.far);

    hmm_mat4 vm = HMM_MultiplyMat4(view,model);
    hmm_mat4 mvp = HMM_MultiplyMat4(projection,vm);
    return mvp;
}

hmm_mat4
calc_cube_mvp(struct cam_orientation *ornt,
              struct cam_perspective *prsp)
{
    cube_center = HMM_AddVec3(ornt->center, ornt->eye);
    cube_rotate_x = HMM_Rotate(cube_transform.rx * 30, HMM_Vec3(1.0f, 0.0f, 0.0f));
    cube_rotate_y = HMM_Rotate(cube_transform.ry * 30, HMM_Vec3(0.0f, 1.0f, 0.0f));
    cube_rotate_z = HMM_Rotate(cube_transform.rz * 30, HMM_Vec3(0.0f, 0.0f, 1.0f));

    cube_scale = HMM_Scale(HMM_Vec3(cube_transform.sx,cube_transform.sy,cube_transform.sz));
    cube_translate = HMM_Translate(HMM_Vec3(cube_transform.tx,cube_transform.ty,cube_transform.tz));

    hmm_mat4 rsx = HMM_MultiplyMat4(cube_rotate_x,cube_scale);
    hmm_mat4 rsy = HMM_MultiplyMat4(cube_rotate_y,rsx);
    hmm_mat4 rsz = HMM_MultiplyMat4(cube_rotate_z,rsy);
    cube_model = HMM_MultiplyMat4(cube_translate,rsz);


    cube_view = HMM_LookAt(ornt->eye, cube_center, ornt->up);
    cube_projection = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    hmm_mat4 cube_vp = HMM_MultiplyMat4(cube_projection,cube_view);
    hmm_mat4 cube_mvp = HMM_MultiplyMat4(cube_vp,cube_model);
    return cube_mvp;
}

hmm_mat4
calc_grid_mvp(struct cam_orientation *ornt,
              struct cam_perspective *prsp)
{
    hmm_mat4 view, proj;
    hmm_vec3 center;
    center = HMM_AddVec3(ornt->center, ornt->eye);

    view = HMM_LookAt(ornt->eye, center, ornt->up);
    proj = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    hmm_mat4 ret = HMM_MultiplyMat4(proj,view);
    return ret;
}

hmm_mat4
calc_frustum_mvp()
{
    hmm_mat4 view = HMM_LookAt(obj_cam_ornt.eye , HMM_AddVec3(obj_cam_ornt.center, obj_cam_ornt.eye), obj_cam_ornt.up);
    hmm_mat4 projection = perspective(obj_cam_prsp.fov, obj_cam_prsp.aspect_ratio,
                                      obj_cam_prsp.near, obj_cam_prsp.far);

    hmm_mat4 mvp = HMM_MultiplyMat4(projection,view);
    return mvp;
}


void
set_frustum_verts()
{
    hmm_mat4 view = HMM_LookAt(proj_cam_ornt.eye, HMM_AddVec3(proj_cam_ornt.center, proj_cam_ornt.eye), proj_cam_ornt.up);
    hmm_mat4 proj = perspective(proj_cam_prsp.fov, proj_cam_prsp.aspect_ratio,
                                proj_cam_prsp.near, proj_cam_prsp.far);
    hmm_mat4 viewproj = HMM_MultiplyMat4(proj,view);
    hmm_mat4 inv = HMM_Mat4();
    inv = hmm_mat4_inv(viewproj, inv);

    hmm_frustum_corners(inv,corners);
    int i,index;
    index = 0;

    for(i = 0; i < 8; i++) {
        frustum_verts[index++] = corners[i].X;
        frustum_verts[index++] = corners[i].Y;
        frustum_verts[index++] = corners[i].Z;
        index += 4;
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
draw_triangle_indices(struct ogl *obj, hmm_mat4 mvp)
{
    glUniformMatrix4fv(obj->matrixID, 1, GL_FALSE, mvp.Elements[0]);
    glDrawElements(GL_TRIANGLES, obj->init_data->index_len, GL_UNSIGNED_SHORT, 0);
}

void
draw_cam(struct ogl *obj)
{
    glUseProgram(obj->program);
    hmm_mat4 translation = HMM_Translate(HMM_AddVec3(proj_cam_ornt.eye, cam_offset));
    hmm_mat4 rsm = HMM_MultiplyMat4(cam_rotation,cam_scale);
    hmm_mat4 model = HMM_MultiplyMat4(translation,rsm);
    hmm_mat4 view = HMM_LookAt(obj_cam_ornt.eye, HMM_AddVec3(obj_cam_ornt.center, obj_cam_ornt.eye), obj_cam_ornt.up);
    hmm_mat4 projection = perspective(obj_cam_prsp.fov, obj_cam_prsp.aspect_ratio,
                                   obj_cam_prsp.near, obj_cam_prsp.far);

    glUniformMatrix4fv(glGetUniformLocation(obj->program, "model"), 1, GL_FALSE, &model.Elements[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "view"), 1, GL_FALSE, &view.Elements[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "projection"), 1, GL_FALSE, &projection.Elements[0][0]);

    glBindVertexArray(obj->VAO);

    glDrawElements(GL_TRIANGLES, obj->init_data->index_len, GL_UNSIGNED_INT, 0);
}

void
draw_cube(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp)
{
    glUseProgram(obj->program);

    cube_center = HMM_AddVec3(ornt->center, ornt->eye);
    cube_rotate_x = HMM_Rotate(cube_transform.rx * 30, HMM_Vec3(1.0f, 0.0f, 0.0f));
    cube_rotate_y = HMM_Rotate(cube_transform.ry * 30, HMM_Vec3(0.0f, 1.0f, 0.0f));
    cube_rotate_z = HMM_Rotate(cube_transform.rz * 30, HMM_Vec3(0.0f, 0.0f, 1.0f));

    cube_translate = HMM_Translate(HMM_Vec3(cube_transform.tx,cube_transform.ty,cube_transform.tz));
    cube_scale = HMM_Scale(HMM_Vec3(cube_transform.sx,cube_transform.sy,cube_transform.sz));

    hmm_mat4 rsx = HMM_MultiplyMat4(cube_rotate_x,cube_scale);
    hmm_mat4 rsy = HMM_MultiplyMat4(cube_rotate_y,rsx);
    hmm_mat4 rsz = HMM_MultiplyMat4(cube_rotate_z,rsy);
    cube_model = HMM_MultiplyMat4(cube_translate,rsz);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "model"), 1, GL_FALSE, &cube_model.Elements[0][0]);


    cube_view = HMM_LookAt(ornt->eye, cube_center, ornt->up);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "view"), 1, GL_FALSE, &cube_view.Elements[0][0]);
    cube_projection = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "projection"), 1, GL_FALSE, &cube_projection.Elements[0][0]);

    glBindVertexArray(obj->VAO);

    glDrawElements(GL_TRIANGLES, obj->init_data->index_len, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void
draw_frustum(struct ogl *obj)
{
    struct cam_orientation *ornt = &obj_cam_ornt;
    struct cam_perspective *prsp = &obj_cam_prsp;
    glUseProgram(obj->program);

    cube_view = HMM_LookAt(ornt->eye, cube_center, ornt->up);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "view"), 1, GL_FALSE, &cube_view.Elements[0][0]);
    cube_projection = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "projection"), 1, GL_FALSE, &cube_projection.Elements[0][0]);

    glBindVertexArray(obj->VAO);

    glDrawElements(GL_TRIANGLES, obj->init_data->index_len, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void
draw_grid(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp)
{
    glDisable(GL_BLEND);
    glUseProgram(obj->program);

    hmm_mat4 view, projection;
    hmm_vec3 center;
    center = HMM_AddVec3(ornt->center, ornt->eye);

    view = HMM_LookAt(ornt->eye, center, ornt->up);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "view"), 1, GL_FALSE, &view.Elements[0][0]);
    projection = perspective(prsp->fov, prsp->aspect_ratio,prsp->near, prsp->far);
    glUniformMatrix4fv(glGetUniformLocation(obj->program, "projection"), 1, GL_FALSE, &projection.Elements[0][0]);

    glBindVertexArray(obj->VAO);
    glDrawArrays(GL_LINES, 0, (obj->init_data->vert_len) / 3);
}

void
just_draw_it(struct ogl *obj, struct cam_orientation *ornt, struct cam_perspective *prsp)
{
    glUseProgram(obj->program);
    hmm_mat4 mvp;
    mvp = calc_grid_mvp( ornt,prsp );
    glUniformMatrix4fv(obj->matrixID, 1, GL_FALSE, mvp.Elements[0]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, obj->VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->IBO);

    glVertexAttribPointer(0,
                          3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(float), 0);
    glDrawArrays(GL_LINES, 0, (obj->init_data->vert_len) / 3);
    glDisableVertexAttribArray(0);
}


/* ===============================================================
 *
 *                          Main Program
 *
 * ===============================================================*/

/* Platform */
SDL_Window *win;
int running = nk_true;
bool lc_down = false;

static int foo = 0;

void
MainLoop(void *loopArg)
{
    set_frustum_verts();
    update_frustum_buffer();
    //glDebugStuff();
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
                hmm_vec3 delta, eye;
                delta = HMM_MultiplyVec3f(obj_cam_ornt.center,MOVESPEED);
                eye = HMM_AddVec3(obj_cam_ornt.eye, delta);
                obj_cam_ornt.eye = eye;
                break;
            }
            case SDLK_a:
            {
                hmm_vec3 left = HMM_NormalizeVec3(HMM_Cross(obj_cam_ornt.center, obj_cam_ornt.up));
                hmm_vec3 delta = HMM_MultiplyVec3f(left,MOVESPEED);
                obj_cam_ornt.eye = HMM_SubtractVec3(obj_cam_ornt.eye, delta);
                break;
            }
            case SDLK_s:
            {
                hmm_vec3 delta = HMM_MultiplyVec3f(obj_cam_ornt.center,MOVESPEED);
                obj_cam_ornt.eye = HMM_SubtractVec3(obj_cam_ornt.eye, delta);
                break;
            }
            case SDLK_d:
            {
                hmm_vec3 left = HMM_NormalizeVec3(HMM_Cross(obj_cam_ornt.center, obj_cam_ornt.up));
                hmm_vec3 delta = HMM_MultiplyVec3f(left,MOVESPEED);
                obj_cam_ornt.eye = HMM_AddVec3(obj_cam_ornt.eye, delta);
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
                obj_cam_ornt.center.X = HMM_COSF(HMM_ToRadians(yaw)) * cosf(HMM_ToRadians(pitch));
                obj_cam_ornt.center.Y = HMM_SINF(HMM_ToRadians(pitch));
                obj_cam_ornt.center.Z = HMM_SINF(HMM_ToRadians(yaw)) * HMM_COSF(HMM_ToRadians(pitch));
                obj_cam_ornt.center = HMM_NormalizeVec3(obj_cam_ornt.center);
            }
            break;
        }
        nk_sdl_handle_event(&evt);
    }
    nk_input_end(ctx);

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
                if (nk_button_label(ctx, "Reset Values"))
                    reset_proj_cam();

                nk_checkbox_label(ctx, "Fix Eye to Scene Camera", (nk_bool*)&selected_cam);
                nk_checkbox_label(ctx, "Show Scene Camera", &show_cam);

                nk_layout_row_static(ctx, 30, 80, 2);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos x", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.X), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos y", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.Y), 10.0f, 0.01f);

                nk_layout_row_dynamic(ctx, 32, 2);
                nk_label(ctx, "Cam pos z", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
                nk_slider_float(ctx, -10.0f, &(proj_cam_ornt.eye.Z), 10.0f, 0.01f);

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
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[0][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[1][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[2][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[3][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[0][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[1][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[2][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[3][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[0][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[1][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[2][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[3][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[0][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[1][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[2][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_model.Elements[3][3]);
        nk_group_end(ctx);

        nk_group_begin(ctx, "View", NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR);
        nk_layout_row_static(ctx, 30, 50, 4);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[0][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[1][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[2][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[3][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[0][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[1][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[2][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[3][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[0][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[1][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[2][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[3][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[0][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[1][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[2][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_view.Elements[3][3]);
        nk_group_end(ctx);

        nk_group_begin(ctx, "Projection", NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR);
        nk_layout_row_static(ctx, 30, 50, 4);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[0][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[1][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[2][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[3][0]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[0][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[1][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[2][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[3][1]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[0][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[1][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[2][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[3][2]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[0][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[1][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[2][3]);
        nk_labelf(ctx, NK_TEXT_LEFT, "%0.2f", cube_projection.Elements[3][3]);
        nk_group_end(ctx);
    }
    nk_end(ctx);

    /* Draw */
    {
        float bg[4];
        int win_width, win_height;
        nk_color_fv(bg, nk_rgb(0, 0, 0));
        SDL_GetWindowSize(win, &win_width, &win_height);
        glViewport(0, 0, win_width, win_height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg[0], bg[1], bg[2], bg[3]);

        if (selected_cam == OBJECTIVE_CAM)
        {
            draw_grid(&(objs.grid), &obj_cam_ornt, &obj_cam_prsp);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            draw_cube(&(objs.cube), &obj_cam_ornt, &obj_cam_prsp);

            if (show_cam)
            {
                draw_frustum(&(objs.frustum));
                draw_cam(&(objs.cam));
            }
            glFlush();
        }
        else if (selected_cam == PROJECTION_CAM)
        {
            draw_grid(&(objs.grid), &proj_cam_ornt, &proj_cam_prsp);
            draw_cube(&(objs.cube), &proj_cam_ornt, &proj_cam_prsp);
        }
        glFlush();

        nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
        SDL_GL_SwapWindow(win);
    }

}

int
main(int argc, char *argv[])
{
    init_objs();
    proj_cam_ornt = proj_cam_ornt_init = (struct cam_orientation){
        HMM_Vec3(3.5f, 0.0f, 0.0f),
        HMM_Vec3(-1.0f, 0.0f, 0.0f),
        HMM_Vec3(0.0f, 1.0f, 0.0f),
    };
    obj_cam_ornt = obj_cam_ornt_init = (struct cam_orientation){
        HMM_Vec3(5.75f, 2.0f, 4.0f),
        HMM_Vec3(-5.0f, -2.0f, -4.0f),
        HMM_Vec3(0.0f, 1.0f, 0.0f),
    };

    struct nk_context *ctx;
    SDL_GLContext glContext;

    NK_UNUSED(argc);
    NK_UNUSED(argv);

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);
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

    glewExperimental = 1;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to setup GLEW\n");
        exit(1);
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    init_cube(&(objs.cube), &cube_init);
    init_cam_gl(&(objs.cam), &cam_init);
    init_grid(&(objs.grid), &grid_init);
    init_frustum(&(objs.frustum), &frustum_init);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    ctx = nk_sdl_init(win);
    {
        struct nk_font_atlas *atlas;
        nk_sdl_font_stash_begin(&atlas);
        nk_sdl_font_stash_end();
        ;
    }

    while (running) {
        MainLoop((void *)ctx);
    }
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
