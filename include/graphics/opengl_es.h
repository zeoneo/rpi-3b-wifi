#ifndef _OPEN_GL_ES_H_
#define _OPEN_GL_ES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define GL_FRAGMENT_SHADER 35632
#define GL_VERTEX_SHADER   35633

/* primitive typo\e in the GL pipline */
typedef enum {
    PRIM_POINT          = 0,
    PRIM_LINE           = 1,
    PRIM_LINE_LOOP      = 2,
    PRIM_LINE_STRIP     = 3,
    PRIM_TRIANGLE       = 4,
    PRIM_TRIANGLE_STRIP = 5,
    PRIM_TRIANGLE_FAN   = 6,
} PRIMITIVE;

/* These come from the blob information header .. they start at   KHRN_HW_INSTR_HALT */
/* https://github.com/raspberrypi/userland/blob/a1b89e91f393c7134b4cdc36431f863bb3333163/middleware/khronos/common/2708/khrn_prod_4.h
 */
/* GL pipe control commands */
typedef enum {
    GL_HALT                           = 0,
    GL_NOP                            = 1,
    GL_FLUSH                          = 4,
    GL_FLUSH_ALL_STATE                = 5,
    GL_START_TILE_BINNING             = 6,
    GL_INCREMENT_SEMAPHORE            = 7,
    GL_WAIT_ON_SEMAPHORE              = 8,
    GL_BRANCH                         = 16,
    GL_BRANCH_TO_SUBLIST              = 17,
    GL_RETURN_FROM_SUBLIST            = 18,
    GL_STORE_MULTISAMPLE              = 24,
    GL_STORE_MULTISAMPLE_END          = 25,
    GL_STORE_FULL_TILE_BUFFER         = 26,
    GL_RELOAD_FULL_TILE_BUFFER        = 27,
    GL_STORE_TILE_BUFFER              = 28,
    GL_LOAD_TILE_BUFFER               = 29,
    GL_INDEXED_PRIMITIVE_LIST         = 32,
    GL_VERTEX_ARRAY_PRIMITIVES        = 33,
    GL_VG_COORDINATE_ARRAY_PRIMITIVES = 41,
    GL_COMPRESSED_PRIMITIVE_LIST      = 48,
    GL_CLIP_COMPRESSD_PRIMITIVE_LIST  = 49,
    GL_PRIMITIVE_LIST_FORMAT          = 56,
    GL_SHADER_STATE                   = 64,
    GL_NV_SHADER_STATE                = 65,
    GL_VG_SHADER_STATE                = 66,
    GL_VG_INLINE_SHADER_RECORD        = 67,
    GL_CONFIG_STATE                   = 96,
    GL_FLAT_SHADE_FLAGS               = 97,
    GL_POINTS_SIZE                    = 98,
    GL_LINE_WIDTH                     = 99,
    GL_RHT_X_BOUNDARY                 = 100,
    GL_DEPTH_OFFSET                   = 101,
    GL_CLIP_WINDOW                    = 102,
    GL_VIEWPORT_OFFSET                = 103,
    GL_Z_CLIPPING_PLANES              = 104,
    GL_CLIPPER_XY_SCALING             = 105,
    GL_CLIPPER_Z_ZSCALE_OFFSET        = 106,
    GL_TILE_BINNING_CONFIG            = 112,
    GL_TILE_RENDER_CONFIG             = 113,
    GL_CLEAR_COLORS                   = 114,
    GL_TILE_COORDINATES               = 115
} GL_CONTROL;

// typedef int (*printhandler) (const char *fmt, ...);

// int32_t load_shader(int32_t shaderType);

void do_rotate(float delta);
// Render a single triangle to memory.
void test_triangle(uint16_t renderWth, uint16_t renderHt, uint32_t renderBufferAddr);

#ifdef __cplusplus
}
#endif

#endif