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

// Native res centered — offset only, no scaling, no seams
#define SCALE_X(x) ((x) + 48.0f)
#define SCALE_Y(y) ((y) + 24.0f)

// c++ guard
#ifdef __cplusplus
extern "C" {
#endif

// variables
extern int Full_Screen;

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
