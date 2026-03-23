#include "graphics.h"
#include "sprites.h"

//just for this project
#include "Game/WORK_SYS.h"

// variables
static unsigned int __attribute__((aligned(64))) list[0x20000];

static void * fbp0;
static void * fbp1;
static void * zBuff;

static uint32_t bg_color = 0xFF000000;

int my_gu_init = 0;

int Full_Screen = 0;

// per-vertex SCALE_X/SCALE_Y handles fullscreen

void enableOffscreenMode() { }

void initGu(){
    sceGuInit();

    fbp0 = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_8888);
    fbp1 = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_8888);
    zBuff = guGetStaticVramBuffer(BUFFER_WIDTH, SCREEN_HEIGHT, GU_PSM_4444);

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

void startFrame(){
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(bg_color);
    sceGuClearDepth(0xFFFF);
    sceGuDisable(GU_SCISSOR_TEST);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

void endFrame(){
    if(Full_Screen){
        // Clip to visible game area — 336x200 centered
        sceGuScissor(48, 24, 48 + 336, 24 + 200);
    }
    else{
        // extend vertically game area
        sceGuScissor(48, 4, 384, 248);
    }
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void endFrameDebug(){
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
