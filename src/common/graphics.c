
#include "graphics.h"
#include "sprites.h"
#include <psprtc.h>

//just for this project
#include "Game/WORK_SYS.h"
#include "Game/sc_data.h"


extern void ppgResetTextureCache(void);
extern s16 render_mode;

// variables
static unsigned int __attribute__((aligned(64))) list[0x20000];

static void * fbp0;
static void * fbp1;
static void * zBuff;
static void * rttBuf;

static uint32_t bg_color = 0xFF000000;

/* CPS3 clipping bounds — used by decompiled game code (bg.c, DC_Ghost.c, etc.) */
float Min_X = 0.0f;
float Max_X = 384.0f;
float Min_Y = 0.0f;
float Max_Y = 224.0f;

int my_gu_init = 0;
int Full_Screen = 0;
int RTT_Enabled = 1;  /* 0=direct rendering, 1=RTT (no seams) */

/* CPS3 native resolution */
#define CPS3_WIDTH  384
#define CPS3_HEIGHT 224
#define RTT_BUF_WIDTH 512  /* power-of-2 stride for texture */

/* Track back buffer for double-buffering */
static int backBuf = 0;

/* Blit quad coordinates */
static float blit_x0, blit_y0, blit_x1, blit_y1;
static s32 scissor_trim_right = 5;
s32 blit_filter = 0;  /* 0=bilinear, 1=nearest */

/* SCALE_X/SCALE_Y — identity during RTT rendering */
float Scale_Factor_X = 1.0f;
float Scale_Factor_Y = 1.0f;
float Scale_Off_X = 0.0f;
float Scale_Off_Y = 0.0f;

void setupScaling(int mode) {
    switch (mode) {
    case 0: /* Stretch — fill entire PSP screen */
        blit_x0 = 0.0f;
        blit_y0 = 0.0f;
        blit_x1 = 480.0f;
        blit_y1 = 272.0f;
        break;
    case 1: { /* 4:3 — arcade aspect, fill height */
        float w = 272.0f * (4.0f / 3.0f);
        blit_x0 = (480.0f - w) / 2.0f;
        blit_y0 = 0.0f;
        blit_x1 = blit_x0 + w;
        blit_y1 = 272.0f;
        break;
    }
    case 2: { /* 4:3 Native — arcade aspect at native height */
        float w = 224.0f * (4.0f / 3.0f);
        blit_x0 = (480.0f - w) / 2.0f;
        blit_y0 = (272.0f - 224.0f) / 2.0f;
        blit_x1 = blit_x0 + w;
        blit_y1 = blit_y0 + 224.0f;
        break;
    }
    default: /* Native — pixel perfect 384x224 centered */
        blit_x0 = (480.0f - CPS3_WIDTH) / 2.0f;
        blit_y0 = (272.0f - CPS3_HEIGHT) / 2.0f;
        blit_x1 = blit_x0 + CPS3_WIDTH;
        blit_y1 = blit_y0 + CPS3_HEIGHT;
        break;
    }

    //Fade_Pos_tbl[8] = { 0, 0, 640, 0, 0, 448, 640, 448 }
    Fade_Pos_tbl[0] = Min_X * 640 / 384;
    Fade_Pos_tbl[1] = Min_Y * 488 / 224;
    Fade_Pos_tbl[2] = Max_X * 640 / 384;
    Fade_Pos_tbl[3] = Min_X * 640 / 384;
    Fade_Pos_tbl[4] = Min_Y * 480 / 224;
    Fade_Pos_tbl[5] = Max_Y * 480 / 224;
    Fade_Pos_tbl[6] = Max_X * 640 / 384;
    Fade_Pos_tbl[7] = Max_Y * 480 / 224;
}

void enableOffscreenMode() { }

void initGu(){
    sceGuInit();

    fbp0 = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_8888);
    fbp1 = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_8888);
    zBuff = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_4444);
    rttBuf = guGetStaticVramBuffer(RTT_BUF_WIDTH, 256, GU_PSM_8888);  /* 256 = power-of-2 for clean texture sampling */

    backBuf = 0;

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, fbp1, BUFFER_WIDTH);

    sceGuDepthBuffer(zBuff, BUFFER_WIDTH);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_LEQUAL);

    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_CLIP_PLANES);

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0x00, 0xFF);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2) + 10, 2048 - (SCREEN_HEIGHT / 2) + 10);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuFinish();

    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    my_gu_init = 1;
}

void endGu(){
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    my_gu_init = 0;
}

/* Vertex for the final blit quad */
typedef struct { float u, v; float x, y, z; } BlitVertex;

void startFrame(){
    setupScaling(render_mode);

    sceGuStart(GU_DIRECT, list);

    if (RTT_Enabled) {
        sceGuDrawBufferList(GU_PSM_8888, rttBuf, RTT_BUF_WIDTH);
        ppgResetTextureCache();
        sceGuOffset(2048 - (CPS3_WIDTH / 2) + 10, 2048 - (CPS3_HEIGHT / 2) + 10);
        sceGuViewport(2048, 2048, CPS3_WIDTH, CPS3_HEIGHT);
        Scale_Factor_X = 1.0f; Scale_Factor_Y = 1.0f;
        Scale_Off_X = 0.0f; Scale_Off_Y = 0.0f;
    } else {
        setupScaling(render_mode);
    }

    sceGuClearColor(bg_color);
    sceGuClearDepth(0xFFFF);
    if (RTT_Enabled) {
        /* Scissor BEFORE clear — prevents clear from overflowing
           RTT buffer into adjacent VRAM texture cache */
        sceGuScissor(0, 0, CPS3_WIDTH, CPS3_HEIGHT);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    } else {
        sceGuDisable(GU_SCISSOR_TEST);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    }

    if (RTT_Enabled) {
        sceGuScissor(0, 0, CPS3_WIDTH, CPS3_HEIGHT);
    } else {
        s32 sx = (s32)SCALE_X(0);
        s32 sy = (s32)SCALE_Y(0);
        s32 sw = (s32)(SCALE_X(384) - 5);
        s32 sh = (s32)SCALE_Y(224);
        sceGuScissor(sx, sy, sw, sh);
    }
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

void endFrame(){
    if (!RTT_Enabled) {
        /* Direct path — just finish and swap */
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
        return;
    }

    /* RTT path — switch to screen, blit */
    void *curBack = backBuf ? fbp1 : fbp0;
    sceGuDrawBufferList(GU_PSM_8888, curBack, BUFFER_WIDTH);
    ppgResetTextureCache();

    sceGuOffset(2048 - (SCREEN_WIDTH / 2) + 10, 2048 - (SCREEN_HEIGHT / 2) + 10);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);

    sceGuDisable(GU_SCISSOR_TEST);
    if (blit_x0 > 0.5f || blit_y0 > 0.5f) {
        sceGuClearColor(0xFF000000);
        sceGuClear(GU_COLOR_BUFFER_BIT);
    }

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    /* Force opaque blit — use fixed blend factors to ignore any alpha */
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFFFF, 0x00000000);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    void *rttAbs = (void*)((u32)sceGeEdramGetAddr() + (u32)rttBuf);
    sceGuTexImage(0, RTT_BUF_WIDTH, 256, RTT_BUF_WIDTH, rttAbs);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(blit_filter ? GU_NEAREST : GU_LINEAR,
                   blit_filter ? GU_NEAREST : GU_LINEAR);
    sceGuEnable(GU_TEXTURE_2D);

    BlitVertex *v = (BlitVertex*)sceGuGetMemory(2 * sizeof(BlitVertex));
    v[0].u = 0.5f;                     v[0].v = 0.5f;
    v[0].x = blit_x0;                  v[0].y = blit_y0;       v[0].z = 0.0f;
    v[1].u = (float)CPS3_WIDTH - 0.5f; v[1].v = (float)CPS3_HEIGHT - 0.5f;
    v[1].x = blit_x1 - scissor_trim_right;
    v[1].y = blit_y1;                  v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES,
        GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);


    /* Restore state for next frame */
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    backBuf ^= 1;
}

void endFrameDebug(){
    /* Restore screen draw buffer on debug/skip frames */
    void *curBack = backBuf ? fbp1 : fbp0;
    sceGuDrawBufferList(GU_PSM_8888, curBack, BUFFER_WIDTH);
    sceGuOffset(2048 - (SCREEN_WIDTH / 2) + 10, 2048 - (SCREEN_HEIGHT / 2) + 10);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
}

uint32_t getBgColor(){
    return bg_color;
}

void setBgColor(uint32_t color){
    bg_color = color;
}

int getGuInit(){
    return my_gu_init;
}
