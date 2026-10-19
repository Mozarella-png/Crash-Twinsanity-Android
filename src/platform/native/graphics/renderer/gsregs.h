#pragma once

// The GS registers' layouts the renderer builds its packets with (the GS User's Manual's), under the names the PS2 side gets
// from PS2SDK's libgs.h, which the native build doesn't have

#include "common.h"

struct GS_PRIM
{
    u64 prim_type : 3;
    u64 iip : 1;
    u64 tme : 1;
    u64 fge : 1;
    u64 abe : 1;
    u64 aa1 : 1;
    u64 fst : 1;
    u64 ctxt : 1;
    u64 fix : 1;
    u64 pad1 : 53;
};

struct GS_PRMODE
{
    u64 pad1 : 3;
    u64 iip : 1;
    u64 tme : 1;
    u64 fge : 1;
    u64 abe : 1;
    u64 aa1 : 1;
    u64 fst : 1;
    u64 ctxt : 1;
    u64 fix : 1;
    u64 pad2 : 53;
};

struct GS_RGBAQ
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    float q;
};

struct GS_ST
{
    float s;
    float t;
};

struct GS_UV
{
    u64 u : 14;
    u64 pad1 : 2;
    u64 v : 14;
    u64 pad2 : 34;
};

struct GS_XYZ
{
    u16 x;
    u16 y;
    u32 z;
};

struct GS_XYZF
{
    u16 x;
    u16 y;
    u32 z : 24;
    u32 f : 8;
};

struct GS_TEX0
{
    u64 tb_addr : 14;
    u64 tb_width : 6;
    u64 psm : 6;
    u64 tex_width : 4;
    u64 tex_height : 4;
    u64 tex_cc : 1;
    u64 tex_funtion : 2;
    u64 cb_addr : 14;
    u64 clut_pixmode : 4;
    u64 clut_smode : 1;
    u64 clut_offset : 5;
    u64 clut_loadmode : 3;
};

struct GS_TEX1
{
    u64 lcm : 1;
    u64 pad1 : 1;
    u64 mxl : 3;
    u64 mmag : 1;
    u64 mmin : 3;
    u64 mtba : 1;
    u64 pad2 : 9;
    u64 l : 2;
    u64 pad3 : 11;
    u64 k : 12;
    u64 pad4 : 20;
};

struct GS_CLAMP
{
    u64 wrap_mode_s : 2;
    u64 wrap_mode_t : 2;
    u64 min_clamp_u : 10;
    u64 max_clamp_u : 10;
    u64 min_clamp_v : 10;
    u64 max_clamp_v : 10;
    u64 pad0 : 20;
};

struct GS_TEXA
{
    u64 alpha_0 : 8;
    u64 pad1 : 7;
    u64 alpha_method : 1;
    u64 pad2 : 16;
    u64 alpha_1 : 8;
    u64 pad3 : 24;
};

struct GS_ALPHA
{
    u64 a : 2;
    u64 b : 2;
    u64 c : 2;
    u64 d : 2;
    u64 pad0 : 24;
    u64 alpha : 8;
    u64 pad1 : 24;
};

struct GS_TEST
{
    u64 atest_enable : 1;
    u64 atest_method : 3;
    u64 atest_reference : 8;
    u64 atest_fail_method : 2;
    u64 datest_enable : 1;
    u64 datest_mode : 1;
    u64 ztest_enable : 1;
    u64 ztest_method : 2;
    u64 pad1 : 45;
};

struct GS_FRAME
{
    u64 fb_addr : 9;
    u64 pad1 : 7;
    u64 fb_width : 6;
    u64 pad2 : 2;
    u64 psm : 6;
    u64 pad3 : 2;
    u64 draw_mask : 32;
};

struct GS_ZBUF
{
    u64 fb_addr : 9;
    u64 pad1 : 15;
    u64 psm : 4;
    u64 pad2 : 4;
    u64 update_mask : 1;
    u64 pad3 : 31;
};

struct GS_SCISSOR
{
    u64 clip_x0 : 11;
    u64 pad1 : 5;
    u64 clip_x1 : 11;
    u64 pad2 : 5;
    u64 clip_y0 : 11;
    u64 pad3 : 5;
    u64 clip_y1 : 11;
    u64 pad4 : 5;
};

struct GS_FBA
{
    u64 alpha : 1;
    u64 pad0 : 63;
};

struct GS_DTHE
{
    u64 enable : 1;
    u64 pad01 : 63;
};

struct GS_COLCLAMP
{
    u64 clamp : 1;
    u64 pad01 : 63;
};

struct GS_BITBLTBUF
{
    u64 src_addr : 14;
    u64 pad1 : 2;
    u64 src_width : 6;
    u64 pad2 : 2;
    u64 src_pixmode : 6;
    u64 pad3 : 2;
    u64 dest_addr : 14;
    u64 pad4 : 2;
    u64 dest_width : 6;
    u64 pad5 : 2;
    u64 dest_pixmode : 6;
    u64 pad6 : 2;
};

struct GS_TRXPOS
{
    u64 src_x : 11;
    u64 pad1 : 5;
    u64 src_y : 11;
    u64 pad2 : 5;
    u64 dest_x : 11;
    u64 pad3 : 5;
    u64 dest_y : 11;
    u64 direction : 2;
    u64 pad4 : 3;
};

struct GS_TRXREG
{
    u64 trans_w : 12;
    u64 pad1 : 20;
    u64 trans_h : 12;
    u64 pad2 : 20;
};

struct GS_TRXDIR
{
    u64 trans_dir : 2;
    u64 pad1 : 62;
};

enum GsPrimType
{
    GS_PRIM_POINT = 0,
    GS_PRIM_LINE,
    GS_PRIM_LINE_STRIP,
    GS_PRIM_TRI,
    GS_PRIM_TRI_STRIP,
    GS_PRIM_TRI_FAN,
    GS_PRIM_SPRITE
};

#define GS_PIXMODE_32 0
#define GS_PIXMODE_24 1
#define GS_PIXMODE_16 2
#define GS_PIXMODE_16S 10
#define GS_TEX_32 0
#define GS_TEX_24 1
#define GS_TEX_16 2
#define GS_TEX_16S 10
#define GS_TEX_8 19
#define GS_TEX_4 20
#define GS_TEX_8H 27
#define GS_TEX_4HL 36
#define GS_TEX_4HH 44
#define GS_ZBUFF_32 48
#define GS_ZBUFF_24 49
#define GS_ZBUFF_16 50
#define GS_ZBUFF_16S 58

enum GsAlphaTestMethods
{
    GS_ALPHA_NEVER = 0,
    GS_ALPHA_ALWAYS,
    GS_ALPHA_LESS,
    GS_ALPHA_LEQUAL,
    GS_ALPHA_EQUAL,
    GS_ALPHA_GEQUAL,
    GS_ALPHA_GREATER,
    GS_ALPHA_NOTEQUAL,
};

enum GsATestFailedUpdateMethods
{
    GS_ALPHA_NO_UPDATE = 0,
    GS_ALPHA_FB_ONLY,
    GS_ALPHA_ZB_ONLY,
    GS_ALPHA_RGB_ONLY
};

enum GsZTestMethodTypes
{
    GS_ZBUFF_NEVER = 0,
    GS_ZBUFF_ALWAYS,
    GS_ZBUFF_GEQUAL,
    GS_ZBUFF_GREATER
};

enum GsTexFilters
{
    GS_TEX_NEAREST = 0,
    GS_TEX_LINEAR,
    GS_TEX_NEAREST_MIPMAP_NEAREST,
    GS_TEX_NEAREST_MIPMAP_LINEAR,
    GS_TEX_LINEAR_MIPMAP_NEAREST,
    GS_TEX_LINEAR_MIPMAP_LINEAR
};

enum GsTexFunctions
{
    GS_TEX_MODULATE = 0,
    GS_TEX_DECAL,
    GS_TEX_HIGHLIHGT1,
    GS_TEX_HIGHLIHGT2
};
