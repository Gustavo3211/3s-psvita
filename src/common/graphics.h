#ifndef GRAPHICS_H_ //  include guard
#define GRAPHICS_H_ 1

// libraries
#include <memory.h>
#include <pspdisplay.h>
#include <pspgu.h>

// custom
#include "sprites.h"

// constants
#define BUFFER_WIDTH 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// Scaling macros — switch between fullscreen and native
extern float Scale_Factor_X;
extern float Scale_Factor_Y;
extern float Scale_Off_X;
extern float Scale_Off_Y;

#define SCALE_X(x) ((x) * Scale_Factor_X + Scale_Off_X)
#define SCALE_Y(y) ((y) * Scale_Factor_Y + Scale_Off_Y)

// c++ guard
#ifdef __cplusplus
extern "C" {
#endif

// variables
extern int Screen_Mode;

#define SCREEN_MODE_FULLSCREEN 0
#define SCREEN_MODE_ORIGINAL 1
#define SCREEN_MODE_VERTICAL 2

// functions
void initGu();
void endGu();

void startFrame();
void endFrame();
void endFrameDebug();
void enableOffscreenMode();

uint32_t getBgColor();
void setBgColor(uint32_t color);
int getGuInit();

// end c++ guard
#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_H_
