
#include "graphics.h"
#include "sprites.h"

//just for this project
#include "Game/WORK_SYS.h"
#include "Game/sc_data.h"

// variables
static unsigned int __attribute__((aligned(64))) list[0x20000];

static void * fbp0;
static void * fbp1;
static void * zBuff;

static uint32_t bg_color = 0xFF000000;

int my_gu_init = 0;

int Screen_Mode = 0;

/* Scaling: default = fullscreen (aspect-preserving)
   CPS3 = 384x224, PSP = 480x272
   Scale = 272/224 = 1.2143, width = 384*1.2143 = 466, offset = (480-466)/2 = 7 */
float Scale_Factor_X = 1.2143f;
float Scale_Factor_Y = 1.2143f;
float Scale_Off_X = 7.0f;
float Scale_Off_Y = 0.0f;

float Min_X = 0.0f;
float Max_X = 384.0f;
float Min_Y = 0.0f;
float Max_Y = 224.0f;

void setupScaling() {
    switch(Screen_Mode){
        case SCREEN_MODE_FULLSCREEN:
            Scale_Factor_X = 272.0f / 224.0f;
            Scale_Factor_Y = 272.0f / 224.0f;
            Scale_Off_X = (480.0f - 384.0f * Scale_Factor_X) / 2.0f;
            Scale_Off_Y = 0.0f;

            Min_X = 0.0f;
            Max_X = 384.0f;
            Min_Y = 0.0f;
            Max_Y = 224.0f;
            break;

        case SCREEN_MODE_ORIGINAL:
            /* Native resolution centered */
            Scale_Factor_X = 1.0f;
            Scale_Factor_Y = 1.0f;
            Scale_Off_X = 48.0f;
            Scale_Off_Y = 24.0f;

            Min_X = 0.0f;
            Max_X = 384.0f;
            Min_Y = 0.0f;
            Max_Y = 224.0f;
            break;
        case SCREEN_MODE_VERTICAL:
            /* Native resolution centered */
            Scale_Factor_X = 1.0f;
            Scale_Factor_Y = 1.0f;
            Scale_Off_X = 48.0f;
            Scale_Off_Y = 24.0f;

            Min_X = 0.0f;
            Max_X = 384.0f;
            Min_Y = -16.0f;
            Max_Y = 230.0f;
            break;
            
        case SCREEN_MODE_EXTENDED:
            /* Native resolution centered */
            Scale_Factor_X = 1.0f;
            Scale_Factor_Y = 1.0f;
            Scale_Off_X = 48.0f;
            Scale_Off_Y = 24.0f;

            Min_X = -48.0f;
            Max_X = 480.0f;
            Min_Y = -24.0f;
            Max_Y = 272.0f;
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
    setupScaling();

    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(bg_color);
    sceGuClearDepth(0xFFFF);
    sceGuDisable(GU_SCISSOR_TEST);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    /* Clip to scaled game area — trim overscan on right edge only */
    s32 sx = (s32)SCALE_X(Min_X);
    s32 sy = (s32)SCALE_Y(Min_Y);
    s32 sw = (s32)(Max_X - Min_X) * Scale_Factor_X;
    s32 sh = (s32)(Max_Y - Min_Y) * Scale_Factor_Y;

    sceGuScissor(sx, sy, sw, sh);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

void endFrame(){
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
