/**
 * CRI ADX decoder for PSP
 * Based on working feat/fullscreen-scaling implementation
 * Uses sceAudioChReserve + sceAudioOutputBlocking (proven working)
 */
#include "psp/adx.h"
#include "psp/afs.h"

#include <pspaudio.h>
#include <pspaudiolib.h>
#include <pspthreadman.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PSP_SAMPLES 1024
#define MAX_CH 2

typedef struct { s32 p1, p2; } ChSt;

static struct {
    volatile AdxStat stat;
    volatile s32 volume;
    volatile s32 running;

    u8 *buf;
    s32 buf_size;
    s32 data_offset;
    s32 channels;
    s32 sample_rate;
    s32 block_size;
    s32 spb;
    s32 frame_size;
    s32 coeff1, coeff2;
    ChSt ch[MAX_CH];
    s32 pos;

    s32 loop_enabled;
    s32 loop_start_byte;
    s32 loop_end_byte;
} P;

static u16 rb16(const u8 *p) { return (p[0]<<8)|p[1]; }
static u32 rb32(const u8 *p) { return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; }

static void dec_block(const u8 *blk, ChSt *ch, s16 *out, s32 stride, s32 spb, s32 c1, s32 c2) {
    s32 scale = rb16(blk);
    const u8 *d = blk + 2;
    s32 p1 = ch->p1, p2 = ch->p2;
    s32 i;
    for (i = 0; i < spb/2; i++) {
        s32 b = d[i];
        s32 n1 = (b >> 4) & 0xF; if (n1 & 8) n1 -= 16;
        s32 s1 = n1 * scale + ((c1 * p1 + c2 * p2) >> 12);
        if (s1 > 32767) s1 = 32767; if (s1 < -32768) s1 = -32768;
        *out = (s16)s1; out += stride; p2 = p1; p1 = s1;

        s32 n2 = b & 0xF; if (n2 & 8) n2 -= 16;
        s32 s2 = n2 * scale + ((c1 * p1 + c2 * p2) >> 12);
        if (s2 > 32767) s2 = 32767; if (s2 < -32768) s2 = -32768;
        *out = (s16)s2; out += stride; p2 = p1; p1 = s2;
    }
    ch->p1 = p1; ch->p2 = p2;
}

static s32 parse_header(const u8 *h, s32 size) {
    if (size < 20 || h[0] != 0x80) return -1;
    P.data_offset = (s32)rb16(h + 2) + 4;
    P.channels = h[7];
    if (P.channels < 1 || P.channels > MAX_CH) return -1;
    P.sample_rate = (s32)rb32(h + 8);
    P.block_size = h[5];
    if (P.block_size < 3) return -1;
    P.spb = (P.block_size - 2) * 2;
    P.frame_size = P.block_size * P.channels;

    double w = 2.0 * M_PI * 500.0 / (double)P.sample_rate;
    double x = sqrt(2.0) - cos(w);
    double y = sqrt(2.0) - 1.0;
    double z = (x - sqrt((x+y)*(x-y))) / y;
    P.coeff1 = (s32)(z * 8192.0);
    P.coeff2 = (s32)(z * z * -4096.0);
    memset(P.ch, 0, sizeof(P.ch));

    P.loop_enabled = 0;
    u8 ver = h[0x12];
    if (ver == 3 && size > 0x28) {
        if (rb16(h + 0x16) == 1) {
            P.loop_enabled = 1;
            P.loop_start_byte = (s32)rb32(h + 0x1C) - P.data_offset;
            P.loop_end_byte = (s32)rb32(h + 0x24) - P.data_offset;
            if (P.loop_start_byte < 0) P.loop_start_byte = 0;
            if (P.loop_end_byte < 0) P.loop_end_byte = 0;
        }
    }
    return 0;
}

// Decode one source sample (called by resampler)
static s16 blk_buf[MAX_CH][64];
static s32 blk_pos = 0, blk_avail = 0;
static s16 last_l = 0, last_r = 0;
static u32 resample_frac = 0;
static s32 adx_paused = 0;

static void decode_next_sample(void) {
    if (P.stat != ADX_STAT_PLAYING || P.buf == NULL) {
        last_l = last_r = 0;
        return;
    }

    if (blk_pos >= blk_avail) {
        s32 audio_size = P.buf_size - P.data_offset;
        if (P.pos + P.frame_size > audio_size) {
            if (P.loop_enabled && P.loop_start_byte >= 0) {
                P.pos = P.loop_start_byte;
                memset(P.ch, 0, sizeof(P.ch));
            } else {
                P.stat = ADX_STAT_PLAYEND;
                last_l = last_r = 0;
                return;
            }
        }
        if (P.loop_enabled && P.loop_end_byte > 0 && P.pos >= P.loop_end_byte) {
            P.pos = P.loop_start_byte;
            memset(P.ch, 0, sizeof(P.ch));
        }

        const u8 *src = P.buf + P.data_offset + P.pos;
        s32 c;
        for (c = 0; c < P.channels; c++)
            dec_block(src + c * P.block_size, &P.ch[c], blk_buf[c], 1, P.spb, P.coeff1, P.coeff2);
        P.pos += P.frame_size;
        blk_pos = 0;
        blk_avail = P.spb;
    }

    s32 vol = P.volume;
    last_l = (s16)((blk_buf[0][blk_pos] * vol) >> 7);
    last_r = (P.channels == 2) ? (s16)((blk_buf[1][blk_pos] * vol) >> 7) : last_l;
    blk_pos++;
}

// pspaudiolib callback on channel 1 — resamples from source rate to 44100Hz
#define PSP_OUTPUT_RATE 44100

static void adx_psp_callback(void *buf, unsigned int reqn, void *userdata) {
    s16 *out = (s16 *)buf;

    if (P.stat != ADX_STAT_PLAYING || P.buf == NULL || adx_paused) {
        memset(buf, 0, reqn * 4);
        return;
    }

    u32 step = ((u32)P.sample_rate << 16) / PSP_OUTPUT_RATE;

    for (u32 i = 0; i < reqn; i++) {
        resample_frac += step;
        while (resample_frac >= 0x10000) {
            decode_next_sample();
            resample_frac -= 0x10000;
        }
        out[i * 2]     = last_l;
        out[i * 2 + 1] = last_r;
    }
}

void adxInit(void) {
    memset(&P, 0, sizeof(P));
    P.stat = ADX_STAT_STOP;
    P.volume = 127;
    // Use pspaudiolib callback on channel 1 (channel 0 = SPU/SFX)
    pspAudioSetChannelCallback(1, adx_psp_callback, NULL);
}

void adxShutdown(void) {
    pspAudioSetChannelCallback(1, NULL, NULL);
    if (P.buf) free(P.buf);
}

void adxPlay(u16 fnum) {
    // Stop current
    P.stat = ADX_STAT_STOP;
    if (P.buf) { free(P.buf); P.buf = NULL; }
    P.pos = 0;

    u32 file_size = afsGetFileSize(fnum);
    if (file_size == 0) return;

    u8 *buf = (u8 *)memalign(16, file_size);
    if (!buf) return;

    if (!afsReadSync(fnum, buf, file_size)) {
        free(buf);
        return;
    }

    if (parse_header(buf, file_size) < 0) {
        free(buf);
        return;
    }
    P.buf = buf;
    P.buf_size = file_size;
    P.pos = 0;
    P.stat = ADX_STAT_PLAYING;
}

void adxPlayLoop(u16 fnum, u32 ls, u32 le) { adxPlay(fnum); }

void adxStop(void) {
    P.stat = ADX_STAT_STOP;
    if (P.buf) { free(P.buf); P.buf = NULL; }
    P.pos = 0;
}

void adxSetVolume(s32 vol) {
    if (vol < 0) vol = 0;
    if (vol > 127) vol = 127;
    P.volume = vol;
}

AdxStat adxGetStat(void) { return P.stat; }
void adxUpdate(void) { }

// ── Compatibility wrappers for the new sound system ──────────────
// Bridge the branch's ADX_* API to our existing adx* functions.
// Supports seamless multi-track, memory playback, and AFS streaming.

static s32 adx_entry_queue[16];
static s32 adx_entry_count = 0;

void ADX_Init(void) {
    adxInit();
    adx_paused = 0;
    adx_entry_count = 0;
}

void ADX_Exit(void) {
    adxShutdown();
}

void ADX_Stop(void) {
    adxStop();
}

void ADX_StartAfs(s32 fnum) {
    adxPlay((u16)fnum);
    adx_paused = 0;
}

void ADX_EntryAfs(s32 fnum) {
    /* Queue a file for seamless playback.
       For now just store it; ADX_StartSeamless will play the first one. */
    if (adx_entry_count < 16) {
        adx_entry_queue[adx_entry_count++] = fnum;
    }
}

void ADX_StartSeamless(void) {
    /* Start playing the first queued entry and unpause */
    if (adx_entry_count > 0) {
        adxPlay((u16)adx_entry_queue[0]);
        for (s32 i = 1; i < adx_entry_count; i++) {
            adx_entry_queue[i - 1] = adx_entry_queue[i];
        }
        adx_entry_count--;
    }
    adx_paused = 0;
}

void ADX_StartMem(void* data, u32 size) {
    if (!data || size == 0) return;

    /* Stop current */
    P.stat = ADX_STAT_STOP;
    if (P.buf) { free(P.buf); P.buf = NULL; }
    P.pos = 0;

    /* Copy to our own buffer (caller may free theirs) */
    u8 *buf = (u8 *)memalign(16, size);
    if (!buf) return;
    memcpy(buf, data, size);

    if (parse_header(buf, size) < 0) {
        free(buf);
        return;
    }

    P.buf = buf;
    P.buf_size = size;
    P.pos = 0;
    P.stat = ADX_STAT_PLAYING;
    adx_paused = 0;
}

s32 ADX_GetState(void) {
    return (s32)adxGetStat();
}

void ADX_SetOutVol(s32 vol) {
    /* The volume table returns values in a -999..0 range.
       Map to our 0..127 range: treat 0 as max, -999 as silent. */
    s32 mapped;
    if (vol <= -999) {
        mapped = 0;
    } else if (vol >= 0) {
        mapped = 127;
    } else {
        mapped = (999 + vol) * 127 / 999;
    }
    adxSetVolume(mapped);
}

void ADX_SetMono(s32 mono) {
    /* Mono/stereo mode — not implemented on PSP yet. */
    (void)mono;
}

s32 ADX_GetNumFiles(void) {
    return adx_entry_count;
}

void ADX_ResetEntry(void) {
    adx_entry_count = 0;
}

void ADX_Pause(s32 pause) {
    adx_paused = pause;
    /* TODO: actually pause/resume the audio thread */
}

s32 ADX_IsPaused(void) {
    return adx_paused;
}

void ADX_ProcessTracks(void) {
    adxUpdate();
    /* Auto-advance seamless queue when current track ends */
    if (adx_entry_count > 0 && adxGetStat() == ADX_STAT_PLAYEND) {
        ADX_StartSeamless();
    }
}
