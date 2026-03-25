#include <pspuser.h>
#include <psppower.h>

#include "common/audio.h"
#include "common/graphics.h"
#include "psp/adx.h"
#include "psp/afs.h"
#include <pspaudiolib.h>

#include "Game/main.h"

// PSP_MODULE_INFO is required
PSP_MODULE_INFO("3rd-strike", 0, 5, 1);
//PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-1024);
PSP_HEAP_THRESHOLD_SIZE_KB(1024);

// global variables
volatile int g_request_pause = 0;

extern void adxSuspend(void);
extern void adxResume(void);

int power_callback(int unknown, int powerInfo, void *common) {
    if (powerInfo & PSP_POWER_CB_SUSPENDING) {
        /* Silence audio callback during sleep */
        adxSuspend();
        /* Close AFS fds — prevents stale fd reads */
        afsSuspend();
        /* Trigger in-game pause on resume */
        g_request_pause = 1;
    }
    if (powerInfo & PSP_POWER_CB_RESUME_COMPLETE) {
        /* Proactively reopen AFS fds — sceIoRead on stale fd may hang
           rather than return error on some firmware */
        afsReopen();
        /* Restore 333MHz — OS may reset clock after sleep */
        scePowerSetClockFrequency(333, 333, 166);
        /* Resume audio playback */
        adxResume();
    }
    return 0;
}

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize args, void *argp) {
    int exit_cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(exit_cbid);

    int power_cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
    scePowerRegisterCallback(0, power_cbid);

    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks(void) {
    int thid = sceKernelCreateThread("callback_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

int main(void)  {
    // Use above functions to make exiting possible
    setup_callbacks();

    scePowerSetClockFrequency(333, 333, 166);

    initGu();

    pspAudioInit();  // Init pspaudiolib with NULL callbacks (silence)
    // SPU_Init sets channel 0 callback for SFX
    // ADX uses sceAudioChReserve for BGM on a separate channel

    AcrMain();

    return 0;
}
