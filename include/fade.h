#ifndef FADE_H
#define FADE_H

#include "types.h"
#include "gx.h"

// game/fade.cpp: up to 4 full-screen colour fades (C linkage).
struct FADE_WORK {
    GXColor s_col;  // 0x00
    GXColor e_col;    // 0x04
    GXColor col;    // 0x08
    u8 pad_C[8];
    f32 z;          // 0x14
    u16 flags;      // 0x18  bit 0 = fading, bit 1 = keep drawing when done, bit 2 = late group (FadeControl(1))
    u16 state;        // 0x1A
    u32 time;       // 0x1C  frames
    u32 cnt;      // 0x20
};

extern FADE_WORK Fade[4];

extern "C" {
void FadeSet(int no, GXColor* start, GXColor* end, u32 time, u32 z, int late);
void FadeKillAll();
enum FADE_NO {
    FADE_NO_SYSTEM = 0,
    FADE_NO_SCENARIO = 1,
    FADE_NO_ROOM = 2,
    FADE_NO_ERROR = 3,
    FADE_NUM = 4
};

void FadeKill(int no);
void FadeInit();
void FadeControl(int flag);
void fadeDraw(FADE_WORK* pF);
}

// Full-screen fade between black and clear (every game-side FadeSet call). The colour pair is a
// local of this inline: a class with a user copy constructor is BLKmode (cp/class.c finish_struct_1),
// so it lives in the inline's frame and every inlined copy shares one temp slot, and integrate.c maps the inline frame to a pseudo P with a constant
// equivalence that it substitutes into the hard-register argument sets (`addi r5, r1, off` per
// call, never PRE'd; at frame offset 0 the frame register itself, so everything is direct). A
// store of a *constant* through P is rejected by recog (no store-immediate on PPC) and keeps P
// (`stw rZ, 4(rP)`, `mr r4, rP`, PRE'd across blocks); `black` is stale for that substitution
// after the label of the first `if`, so its store goes through the substituted frame address and
// its `li` follows the zero's. See docs/matching.md "FadeSet colour pair".
struct FadeColorPair {
    GXColor start;
    GXColor end;
    FadeColorPair() {}
    FadeColorPair(const FadeColorPair&) {}
};

static inline void FadeSetW(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 black;

    if (no & 0x80000000) {
        *(u32*) &col.start = 0xFF;
    } else {
        *(u32*) &col.start = 0;
    }
    black = 0xFF;
    if (no & 0x80000000) {
        *(u32*) &col.end = 0;
    } else {
        *(u32*) &col.end = black;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

// The two fade colours must live in a BLKmode object: a 4-byte GXColor local becomes an ADDRESSOF
// pseudo (SImode) and purge_addressof gives it a permanent frame slot instead of the shared temp at
// 8/12; a 12-byte struct reuses the Vec slot (temp reuse needs equal modes).
struct FadeColors {
    GXColor c0;
    GXColor c1;
    u32 pad;
};

// 30-frame fade between two packed RGBA colours (sce_com, r225).
static inline void FadeSetRGBA(u32 mode, u32 rgba0, u32 rgba1)
{
    FadeColors c;

    *(u32*) &c.c0 = rgba0;
    *(u32*) &c.c1 = rgba1;
    FadeSet(mode, &c.c0, &c.c1, 30, 0, 0);
}

// Wait for fade `no` to finish: the index stays a separate `addi` on the array base (r316, r31c).
void SceSleep(int frames);
static inline void FadeWait(int no)
{
    while (Fade[no].flags & 1) {
        SceSleep(1);
    }
}

#endif
