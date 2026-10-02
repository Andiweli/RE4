// game/Espgen42.cpp: effect controller 42, the simulated room water surface. The rest of the game
// uses AddWaterPower / GetWaterHeight / GetWaterCrossPos for splashes, floating and bullet hits.

#include "light.h"
#include "atari.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "camera.h"
#include "os_vi.h"
#include "db_log.h"
#include "main_sub.h"
#include "joy.h"
#include <dolphin/base/PPCArch.h>
#include "trans_lit.h"

// Effect controller 42: room water surface. A (nx+1) x (ny+1) height field simulated on two
// ping-pong buffers, rendered as triangle strips through a display list with an indirect bump
// texture built every frame from the normals. Shared with the weather water (espgen45):
// AddWaterPower / GetWaterHeight / GetWaterCrossPos test both generators.


void AddWaterPowerSub(cEspgen* w);
void GetWaterHeightSub(cEspgen* w);
void GetWaterCrossPosSub(cEspgen* w);
void Espgen42_Move00(cEspgen* w);
void Espgen42_TransSub(cEspgen* w);
void SetIndMtx(ESPGEN45_WK* p);
cEspgen* SetWaterWork(cEspgen* w, Vec* pos, Vec* rot, u32 nx, u32 ny, f32 size, f32 rate);

static cEspgen* g_pWater;
static Vec Chk_pos;
static f32 Height_ret;
static f32 Add_power;
static int Height_find;
static Vec Cross_Chk_pos;
static Vec Cross_Chk_dest;
static Vec Cross_Ret_pos;
static int Cross_find;
int g_bNoWater = 0;


// Room start: forgets both water generators (42 room water, 45 weather water), resets the
// Espgen45 override state and clears the no-water debug switch.
void EspWaterInit()
{
    Estgen45SetTargetCamera(1);
    g_pWater = NULL;
    g_pWater45 = NULL;
    Espgen45_static_init();
    g_bNoWater = 0;
    Estgen45SetTargetCamera(1);
    Estgen45SetTargetHeight(0);
    Estgen45SetSizeOverWrite(0);
    Estgen45SetColorOverWrite(0);
    Estgen45SetColorMul(0);
    Estgen45SetParamOverWrite(0);
}

// Pushes the height field down around Chk_pos (the cell and its four neighbours). The position is a
// by-value Vec parameter to get the target's stack temp copy, and `h = p->pSpdBuf + k` is in each arm so
// combine cannot fold the add into `lfsux`.
static inline void AddWaterPowerCore(cEspgen* w, Vec v)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) w->Free.buff;
    u32 x;
    u32 z;
    u32 idx;
    int i;

    PSMTXMultVec(p->Inv_mat, &v, &v);
    if (v.x < (f32) (-p->Width / 2)) {
        return;
    }
    if (v.z < (f32) (-p->Height / 2)) {
        return;
    }
    if (v.x > (f32) (p->Width / 2)) {
        return;
    }
    if (v.z > (f32) (p->Height / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->Height / 2));
    x = (u32) (v.x + (f32) (p->Width / 2));
    idx = z * (p->Width + 1) + x;
    u32 k = 0;
    f32 pw = 1.0f;
    if (x <= 1) {
        return;
    }
    if (z <= 1) {
        return;
    }
    if (x >= (u32) (p->Width - 2)) {
        return;
    }
    if (z >= (u32) (p->Height - 2)) {
        return;
    }
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 0:
            k = idx - 1;
            pw = 0.8f;
            break;
        case 1:
            k = idx + 1;
            pw = 0.8f;
            break;
        case 2:
            k = idx;
            pw = 1.0f;
            break;
        case 3:
            k = idx - p->Width;
            pw = 0.8f;
            break;
        case 4:
            k = idx + p->Width;
            pw = 0.8f;
            break;
        }
        if (k < (u32) (p->Width * p->Height)) {
            f32* h;
            if (pG->Frame_cnt & 1) {
                h = p->pSpdBuf + k;
            } else {
                h = p->pPosBuf + k;
            }
            *h += Add_power * pw;
        }
    }
}

// The 0x45 branch is a hand-written second copy, not the same inline: its `pw` assignments go through a
// temporary (a f32 parameter), which the loop optimiser hoists as `lfs f11`/`fmr f10,f12`
// with `fmr f12,fN` in the cases; the 0x42 copy keeps `lfs` in the cases with only the `lis` hoisted.
static inline void AddWaterPowerCore45(cEspgen* w, Vec v)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) w->Free.buff;
    u32 x;
    u32 z;
    u32 idx;
    int i;

    PSMTXMultVec(p->Inv_mat, &v, &v);
    if (v.x < (f32) (-p->Width / 2)) {
        return;
    }
    if (v.z < (f32) (-p->Height / 2)) {
        return;
    }
    if (v.x > (f32) (p->Width / 2)) {
        return;
    }
    if (v.z > (f32) (p->Height / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->Height / 2));
    x = (u32) (v.x + (f32) (p->Width / 2));
    idx = z * (p->Width + 1) + x;
    u32 k = 0;
    f32 pw = 1.0f;
    if (x <= 1) {
        return;
    }
    if (z <= 1) {
        return;
    }
    if (x >= (u32) (p->Width - 2)) {
        return;
    }
    if (z >= (u32) (p->Height - 2)) {
        return;
    }
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 0:
            k = idx - 1;
            pw = 0.8f;
            break;
        case 1:
            k = idx + 1;
            pw = 0.8f;
            break;
        case 2:
            k = idx;
            pw = 1.0f;
            break;
        case 3:
            k = idx - p->Width;
            pw = 0.8f;
            break;
        case 4:
            k = idx + p->Width;
            *(f32*) &pw = 0.8f;  // COMPILER-DIFF: store through a pointer cast (a plain store schedules differently)
            break;
        }
        if (k < (u32) (p->Width * p->Height)) {
            f32* h;
            if (pG->Frame_cnt & 1) {
                h = p->pSpdBuf + k;
            } else {
                h = p->pPosBuf + k;
            }
            *h += Add_power * pw;
        }
    }
}

// Applies the pending Add_power at Chk_pos to generator `w` (id 0x42 or 0x45 layout).
void AddWaterPowerSub(cEspgen* w)
{
    if (w->Id == 0x42) {
        AddWaterPowerCore(w, Chk_pos);
    } else if (w->Id == 0x45) {
        AddWaterPowerCore45(w, Chk_pos);
    }
}

// Public splash entry (footsteps, bullets, bodies): pushes the water height field down by
// power x 5 at `pos` on every live water surface. No-op unless a water surface exists this frame
// (Status_flg[0] 0x200).
void AddWaterPower(Vec& pos, f32 pow)
{
    if (StaFlagChk(pG, STA_WATER_ALIVE)) {
        Height_find = 0;
        Add_power = pow * 5.0f;
        Chk_pos = pos;
        if (g_pWater != NULL) {
            u8 flg = g_pWater->Be_flg;

            if ((flg & 1) && !(flg & 2)) {
                AddWaterPowerSub(g_pWater);
            }
        }
        if (g_pWater45 != NULL) {
            u8 flg = g_pWater45->Be_flg;

            if ((flg & 1) && !(flg & 2)) {
                AddWaterPowerSub(g_pWater45);
            }
        }
    }
}

// Same shape as AddWaterPowerSub: a by-value Vec inline called once per id; jump2 cross-jumps the
// two copies into one body (w allocated before p: r30/r29).
static inline void GetWaterHeightCore(cEspgen* w, Vec v)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) w->Free.buff;

    PSMTXMultVec(p->Inv_mat, &v, &v);
    if (v.x < (f32) (-p->Width / 2)) {
        return;
    }
    if (v.z < (f32) (-p->Height / 2)) {
        return;
    }
    if (v.x > (f32) (p->Width / 2)) {
        return;
    }
    if (v.z > (f32) (p->Height / 2)) {
        return;
    }
    v.y = 0.0f;
    PSMTXMultVec(p->Wld_mat, &v, &v);
    if (v.y > Height_ret) {
        Height_ret = v.y;
    }
    Height_find = 1;
}

// Runs the height test for Chk_pos on generator `w` (only ids 0x42 / 0x45).
void GetWaterHeightSub(cEspgen* pGen)
{
    if (pGen->Id == 0x42) {
        GetWaterHeightCore(pGen, Chk_pos);
    } else if (pGen->Id == 0x45) {
        GetWaterHeightCore(pGen, Chk_pos);
    }
}

// Debug switch: makes GetWaterHeight report "no water" everywhere.
void Espgen42SetNoWater(int flg)
{
    g_bNoWater = flg;
}

// Surface height under `pos`: 1 and *height when the point (in grid space) lies inside the room
// water or the weather water (an unbounded weather surface answers its plane height); 0 when no
// live water covers it or no water exists this frame.
int GetWaterHeight(Vec* pos, f32* Ret)
{
    if (!StaFlagChk(pG, STA_WATER_ALIVE)) {
        return 0;
    }
    if (g_bNoWater == 1) {
        return 0;
    }
    Height_find = 0;
    Height_ret = -100000000.0f;
    Chk_pos = *pos;
    if (g_pWater != NULL) {
        if (!(g_pWater->Be_flg & 1) || (g_pWater->Be_flg & 2)) {
            if (g_pWater45 != NULL) {
                if (!(g_pWater45->Be_flg & 1) || (g_pWater45->Be_flg & 2)) {
                    return 0;
                }
            }
        }
    }
    if (g_pWater != NULL && (g_pWater->Be_flg & 1) && !(g_pWater->Be_flg & 2)) {
        GetWaterHeightSub(g_pWater);
    }
    if (g_pWater45 != NULL && (g_pWater45->Be_flg & 1) && !(g_pWater45->Be_flg & 2)) {
        ESPGEN45_WK* p = (ESPGEN45_WK*) g_pWater45->Free.buff;
        if (p->flag & 1) {
            GetWaterHeightSub(g_pWater45);
        } else {
            Vec v = {0.0f, 0.0f, 0.0f};
            PSMTXMultVec(p->Wld_mat, &v, &v);
            Height_find = 1;
            Height_ret = v.y;
        }
    }
    *Ret = Height_ret;
    return Height_find;
}

// Intersects the segment Cross_Chk_pos -> Cross_Chk_dest with the room water plane (id 0x42)
// and stores the hit in Cross_Ret_pos when it lies inside the grid.
void GetWaterCrossPosSub(cEspgen* pGen)
{
    ESPGEN45_WK* p;
    Vec d;
    Vec hit;
    Vec v;
    Vec v2;
    f32 t;

    if (pGen->Id == 0x42) {
        PSVECSubtract(&Cross_Chk_dest, &Cross_Chk_pos, &d);
        p = (ESPGEN45_WK*) pGen->Free.buff;
        v.z = 0.0f;
        v.y = 0.0f;
        v.x = 0.0f;
        PSMTXMultVec(p->Wld_mat, &v, &v);
        t = (v.y - Cross_Chk_pos.y) / (Cross_Chk_dest.y - Cross_Chk_pos.y);
        if (t < 0.0f) {
            return;
        }
        PSVECScale(&d, &d, t);
        PSVECAdd(&Cross_Chk_pos, &d, &hit);
        PSMTXMultVec(p->Inv_mat, &hit, &v);
        if (v.x < (f32) (-p->Width / 2)) {
            return;
        }
        if (v.z < (f32) (-p->Height / 2)) {
            return;
        }
        if (v.x > (f32) (p->Width / 2)) {
            return;
        }
        if (v.z > (f32) (p->Height / 2)) {
            return;
        }
        Cross_Ret_pos = hit;
        Cross_find = 1;
    } else if (pGen->Id == 0x45) {
        PSVECSubtract(&Cross_Chk_dest, &Cross_Chk_pos, &d);
        p = (ESPGEN45_WK*) pGen->Free.buff;
        v2.z = 0.0f;
        v2.y = 0.0f;
        v2.x = 0.0f;
        PSMTXMultVec(p->Wld_mat, &v2, &v2);
        t = (v2.y - Cross_Chk_pos.y) / (Cross_Chk_dest.y - Cross_Chk_pos.y);
        if (t < 0.0f) {
            return;
        }
        PSVECScale(&d, &d, t);
        PSVECAdd(&Cross_Chk_pos, &d, &hit);
        if (p->flag & 1) {
            PSMTXMultVec(p->Inv_mat, &hit, &v2);
            if (v2.x < (f32) (-p->Width / 2)) {
                return;
            }
            if (v2.z < (f32) (-p->Height / 2)) {
                return;
            }
            if (v2.x > (f32) (p->Width / 2)) {
                return;
            }
            if (v2.z > (f32) (p->Height / 2)) {
                return;
            }
        }
        Cross_Ret_pos = hit;
        Cross_find = 1;
    }
}

// Where the segment pos -> pos + dir crosses a live water surface: 1 and *out on a hit
// (bullet splashes, item drops), 0 otherwise.
int GetWaterCrossPos(Vec* pos, Vec* dir, Vec* Ret)
{
    if (!StaFlagChk(pG, STA_WATER_ALIVE)) {
        return 0;
    }
    if (dir->x == 0.0f && dir->y == 0.0f && dir->z == 0.0f) {
        return 0;
    }
    Cross_find = 0;
    Cross_Chk_pos = *pos;
    PSVECAdd(pos, dir, &Cross_Chk_dest);
    if (g_pWater != NULL) {
        if (!(g_pWater->Be_flg & 1) || (g_pWater->Be_flg & 2)) {
            if (g_pWater45 != NULL) {
                if (!(g_pWater45->Be_flg & 1) || (g_pWater45->Be_flg & 2)) {
                    return 0;
                }
            }
        }
    }
    if (g_pWater != NULL && (g_pWater->Be_flg & 1) && !(g_pWater->Be_flg & 2)) {
        GetWaterCrossPosSub(g_pWater);
    }
    if (g_pWater45 != NULL && (g_pWater45->Be_flg & 1) && !(g_pWater45->Be_flg & 2)) {
        GetWaterCrossPosSub(g_pWater45);
    }
    *Ret = Cross_Ret_pos;
    return Cross_find;
}

// Bump texture (I8, 8x4 tiles) index of grid point (x, y). x/8 before y/4 (the two signed divisions are
// separate blocks, so their order is the source order) and `(y / 4) << 5`: with `* 32` fold would
// reassociate the constant onto `(w1) >> 3` and hoist `(w1 >> 3) * 32`; the target keeps `srwi` in the loop.
#define BUMP_INDEX(x, y, w1) (((x) / 8) * 32 + (((y) / 4) << 5) * ((w1) >> 3) + (((y) & 3) << 3) + ((x) & 7))
// Noise texture (0xFE) index of grid point (x, y).
#define NOISE_INDEX(x, y) ((((y) << 6) & 0xB00) + (((x) << 2) & 0xA0) + (((y) & 3) << 3) + ((x) & 7))

// u8 to f32 through GQR2 from a stack byte. The volatile asm is a scheduling barrier that orders the
// address adds and neighbour loads as in the target's loop A. The same definition is in trans.cpp,
// and the asm stays in this unit so asmcheck.py counts it.
#define PSQ_L_U8_TO(dst, p) asm volatile("psq_l %0,0(%1),1,2" : "=f"(dst) : "b"(p) : "memory")

// Step 0, every frame: the wave simulation, which also sets Status_flg[0] 0x200 (water present).
// Mode 1 is a cheaper single-pass variant, and in the effect tool the B button drops the surface.
void Espgen42_Move00(cEspgen* pGen)
{
    static f32 wt_pow = 10.0f;
    // p is set at its declaration so alias.c sees an unknown base for it, which makes the p-based loads in
    // loop B issue after the frame and hB stores as in the target. Setting it after the tex call would let
    // those loads float above the stores.
    ESPGEN45_WK* p = (ESPGEN45_WK*) pGen->Free.buff;
    Vec d0;
    Vec d1;
    Vec v;
    Vec d2;
    Vec d3;
    u8 tmp;
    GXTexObj* tex;
    u8* noise;
    f32* c;
    u32 frame;
    int i;
    int j;
    int k;
    int nz;   // noise index / byte of both loops: one function-level pseudo (see loop A)
    // loop A's `j / 8` and loop B's `(i / 4) << 5`: one function-level pseudo (r10 in both loops; a block-local
    // `(i / 4) << 5` would be tied into the `mullw` by local-alloc, the target ties the nx term).
    int jx;
    // loop B's `k + p->Width` (before the call) and `p->Width` (after it): one function-level pseudo (r11 in both places,
    // `-dl` says "set 2 times; dies in 2 places" = no local-alloc tie, so the Z block's `addi r9,r11,1` starts a
    // fresh r9 chain as in the target).
    int mx;
    // noise value of both loops: also one function-level pseudo (global alloc, f10 in both loops). A block-local `n`
    // ties to the loop-A psq_l output / loop-B frsp result and permutes the loop-B FPR names (f12/f13/f0) and the
    // sched2 slots of `lfs 4(r9)` / `stw r22 | addi r3` (34 -> 11 words).
    f32 n;
    // normal pointer `&nrm[k]` of both loops: one function-level pseudo (36 refs / 168 insns, 1.07) ranks above loop A's
    // k*12 giv (26 / 102, 1.02), loop B's k*4 giv (35 / 167, 1.05) and k (32 / 217, 0.74) in global alloc and takes r30
    // for both loops; the givs then open r31, k gets r28. A block-local pointer per loop (18 / 110, 0.65) came after k.
    Vec* nk;
    // `j & 7` of both loops (noise index and bump index): one function-level pseudo (18 refs / 156 insns) ranks above
    // `i` (54 / 698) in global alloc and takes r24, i r23, (k-nx)*12 r26; a block-local `j & 7` per loop (9 refs) came
    // after i, which then took r24. u8, not int: the narrow store is one more loop.c insn in loop B (176 -> 177, the
    // 4.0 pool pair must stay for the outer pass: 44*2*2 = 176 < 177); combine strips the zero-extension at both uses.
    u8 j7;

    d0.x = 1.0f;
    d0.z = 0.0f;
    d1.x = 0.0f;
    d1.z = 1.0f;
    d2.x = -1.0f;
    d2.z = 0.0f;
    d3.x = 0.0f;
    d3.z = -1.0f;
    frame = pG->Frame_cnt % 60;
    StaFlagOn(pG, STA_WATER_ALIVE);
    PPCMtpmc1(0);
    PPCMtpmc2(0);
    PPCMtpmc3(0);
    PPCMtpmc4(0);
    PPCMtmmcr1(0x78000000);
    PPCMtmmcr0(0x42);
    tex = EspGetTexObj(0xFE, frame);
    if (tex == NULL) {
        return;
    }
    noise = (u8*) GXGetTexObjData(tex) + 0x80000000;
    u32 nx = p->Width;
    u32 ny = p->Height;
    f32 hx = (f32) (int) (nx / 2);
    f32 hy = (f32) (int) (ny / 2);
    f32 inx = 1.0f / (f32) (int) nx;
    f32 iny = 1.0f / (f32) (int) ny;
    if (p->Type != 1) {
        if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && (Joy[0].on & 0x100)) {
            // The index is the loop variable `k` (target `lwz r28` = k's register, base+index `lfsx f0,hB,k4`).
            k = (int) ((f32) (int) (nx * ny) * 0.5f);
            // Byte offset in a variable: inside an address `p->pSpdBuf[k]` expands to `(plus (mult k 4) hB)` (expr.c
            // both_summands puts a MULT first) = `lfsx k4,hB`; a register index keeps `(plus hB k4)` = `lfsx hB,k4`.
            u32 k4 = k * 4;
            *(f32*) ((u8*) p->pSpdBuf + k4) -= wt_pow;
        }
        f32 damp = p->Prm_a;
        f32 cdamp = 2.0f - damp * 4.0f;
        f32 spread = p->Prm_dmp;
        f32* cur;
        f32* next;
        if (pG->Frame_cnt & 1) {
            cur = p->pPosBuf;
            next = p->pSpdBuf;
        } else {
            cur = p->pSpdBuf;
            next = p->pPosBuf;
        }
        f32 fy = 1.0f;
        for (i = 1; i < p->Height; i++) {
            f32 fx = 1.0f;
            k = i * (nx + 1);   // two statements: the product lands in k's register (`mullw r28; addi r28,r28,1`)
            k++;
            for (j = 1; j < (int) nx; j++) {
                int i3 = (i & 3) << 3;   // set before the dead test below, so loop.c still hoists it (maybe_never)
                // The k*4 giv is discovered BEFORE the k*12 giv (this statement precedes the dead test's `k * 12`): loop.c
                // emits the giv inits in bl->giv order = reverse discovery, so the preheader is `mr r31,k12 | slwi r27,k,2`
                // (k4 init last). With `c += k` as the first k*4 use (after the test) the two inits were swapped.
                u32 k4 = k * 4;
                // COMPILER-DIFF: candidate (loop.c insn_count): dead test, +4 real insns at loop time (lwz/cmpwi/bne/li;
                // gone by jump2). With 130 (not 126) insns the 0.25 pool pair is "not desirable" in the inner loop
                // (threshold 71 - 3*13 moves = 32, 32*2*2 = 128 < 130) and the OUTER scan hoists it into its
                // preheader (f17); window 129..152. The compare must not read a giv: a `k * 12` compare gave loop A's
                // k*12 giv +3 depth-weighted refs (29 / 102, 1.14) and ranked it above the shared `nk` pointer (1.07),
                // which then lost r30 to it.
                if (p->Type == 3) c = NULL;
                // Before the (volatile) psq_l: `lwz pos; add c; add pos+k12` issue before the noise lbzx (target order).
                Vec* pv = &p->pHeightBuf[k];   // a pointer variable: `add pos,k12` (operand order); `p->pHeightBuf[k].y = ..` gives `add k12,pos`
                // `c` is a function-level pointer set twice per iteration (set_in_loop != 1, so loop.c
                // does not treat it as a giv): the neighbours stay `lfs 4(c)/-4(c)` off `add c = cur + k*4`
                // and `*(c - nx - 1)` becomes `subf` + `lfs -4` off the hoisted `nx*4`.
                c = cur;
                c = (f32*) ((u8*) c + k4);
                // The index in the function-level `nz` (set in both loops = global pseudo, allocated after local-alloc): the
                // byte pseudo then finds r0 free in local-alloc (its fake-lifetime pass would refuse the register of a
                // block-local index dying at the lbzx), and global alloc gives nz r0 too: `lbzx r0,noise,r0; stb r0`.
                j7 = j & 7;
                nz = (((i) << 6) & 0xB00) + (((j) << 2) & 0xA0) + i3 + j7;
                tmp = noise[nz];
                PSQ_L_U8_TO(n, &tmp);
                n -= 80.0f;
                f32 sum = c[-1] + c[1] + *(c - nx - 1) + *(c + nx + 1);
                f32 h = damp * sum + cdamp * cur[k];
                h -= next[k];
                h = n * 0.0002f + h;
                h *= spread;
                pv->y = next[k] = h;   // the pos address is computed before the next[k] store (target `lwz pos` early)
                Vec* nrm = p->pNorBuf;   // before the v.x/v.z reads: kept across the call (`lfsx nrm[k].x`, `4(nrm+k*12)`)
                v.x = p->pHeightBuf[k - 1].y - p->pHeightBuf[k + 1].y;
                v.y = 2.0f;
                v.z = p->pHeightBuf[k - nx].y - p->pHeightBuf[k + nx].y;
                nk = &nrm[k];   // the function-level pointer (see its declaration); `nrm[k].y/.z` below fold onto it in cse
                PSVECNormalize(&v, nk);
                {
                    // BUMP_INDEX with the function-level `j7` as the last term. Both signed divisions go through the
                    // one temp `t` so both share r0 as in the target, and `jx << 5` is a shift because expand would put
                    // a MULT first in the address sum, while the target starts it with the i term.
                    int t = j;
                    if (j < 0) t = j + 7;
                    jx = t >> 3;
                    t = i;
                    if (i < 0) t = i + 3;
                    p->pTexBuf[((t >> 2) << 5) * ((nx + 1) >> 3) + (jx << 5) + i3 + j7] = (u8) (nrm[k].x * 255.0f * 2.0f + 128.0f);
                }
                nrm[k].x += (fx - hx) * inx;
                nrm[k].z += (fy - hy) * iny;
                nrm[k].y *= 0.25f;
                fx += 1.0f;
                k++;
            }
            fy += 1.0f;
        }
    } else {
        // COMPILER-DIFF: loop.c move_movables. The target hoists one more invariant out of the inner loop before the
        // 4.0 pool pair (threshold 71 - 3 per moved insn), so 4.0 stays for the outer pass (f20, outer preheader) and
        // 0.0018 moves in the inner pass 2 (f25). A codeless asm set of `i` used after the inner loop is that extra
        // moved insn (no register: it takes the free r19). It emits nothing. The real construct is unknown.
        int dead;
        for (i = 1; i < p->Height; i++) {
            k = i * (p->Width + 1);
            for (j = 1; j < p->Width; j++) {
                asm("" : "=r"(dead) : "r"(i));
                j7 = j & 7;
                nz = (((i) << 6) & 0xB00) + (((j) << 2) & 0xA0) + (((i) & 3) << 3) + j7;   // NOISE_INDEX with the shared j7
                nz = noise[nz];   // same variable: `lbzx r0,noise,r0; xoris r0` (see loop A)
                // As in loop A: the pos address before the hB/hA stores (p has an unknown alias base, so a `lwz pos`
                // placed after them would wait for them; the target issues it before the first `stfsx hB[k]`).
                Vec* pv = &p->pHeightBuf[k];
                f32* hA = p->pPosBuf;
                f32* hB = p->pSpdBuf;
                c = hA;
                c += k;
                f32 sum = c[-1] + c[1] + *(c - p->Height - 1) + *(c + p->Height + 1);
                // n before the hB[k] update: the 0x4330/pool-double and 80.0 movables precede the 4.0 pair in loop.c's
                // list (the target's inner preheader order is lfd; lfs 80.0; lfs 1.0; ...; 4.0 is in the outer one).
                n = (f32) nz - 80.0f;
                // hB through a byte offset in a variable (`stfsx hB,k4` base first, see the wt_pow store above); hA's
                // address is the cse'd `c` (`(plus hA k4)` from `c += k`, a non-address context) and is base first as is.
                u32 k4 = k * 4;
#define HB (*(f32*) ((u8*) hB + k4))
                HB += sum - hA[k] * 4.0f;
                hA[k] += n * 0.0001f + HB * 0.04f;
                HB *= 0.92f;
#undef HB
                pv->y = n * 0.0018f + hA[k];
                Vec* nrm = p->pNorBuf;
                v.x = p->pHeightBuf[k - 1].y - p->pHeightBuf[k + 1].y;
                v.y = 2.0f;
                // `d` first: the `add mx` is then the last use of the `lhz nx` (REG_WEIGHT 0) and issues before the `subf`.
                int d = k - p->Width;
                mx = k + p->Width;
                v.z = p->pHeightBuf[d].y - p->pHeightBuf[mx].y;
                nk = &nrm[k];   // the function-level pointer shared with loop A (r30 in both loops, see its declaration)
                PSVECNormalize(&v, nk);
                {
                    // Loop B's index differs from loop A's: `j / 8` in its own statement (first division, own temp), the i
                    // term FIRST (`jx * nx8`: the nx chain `lhz; addi; srawi` is then evaluated after the i-division's branch,
                    // inside the Z block as in the target, and `mullw r9,r10,r9` ties the nx term), `jq << 5` (see loop A).
                    // COMPILER-DIFF: candidate #17 (r0 occupant): `j / 8` pinned to r0 = a hard-register conflict of the i
                    // copy `t_i` with r0 during the i-division blocks, so global.c's preference override skips r0 and t_i
                    // takes r9 (the target's `mr r9,i`); jq's shift then inherits r0 (`slwi r0,r0,5`).
                    register int jq asm("r0") = j / 8;
                    jx = (i / 4) << 5;
                    mx = p->Width;
                    // COMPILER-DIFF: candidate (sched1 slot fillers). Three codeless frame stores of `v` (dead after the
                    // call) that read `jx`: ready at t2, priority 90 (the `lfsx nrm[k].x` below may alias them), they take
                    // the t2/t3 issue slots and delay that load to t4, so the byte chain (fmuls .. psq_st) runs one cycle
                    // later and the fast-cast loadaddr (`unspec 17`, prints nothing) is born after the `mullw` (jx dead:
                    // it can take r10), the j loadaddr lives longer than the byte (byte r11, loadaddr r8) and `xoris j` is
                    // issued after the first index `add` (jq32 and the xoris share r0). Without them the loadaddr is issued
                    // at t2 and the xoris before the add: 45 words in the Z block. What the original had there is unknown.
                    asm("" : "=m"(v.x) : "r"(jx));
                    asm("" : "=m"(v.y) : "r"(jx));
                    asm("" : "=m"(v.z) : "r"(jx));
                    p->pTexBuf[jx * ((mx + 1) >> 3) + (jq << 5) + ((i & 3) << 3) + j7] = (u8) (nrm[k].x * 255.0f * 2.0f + 128.0f);
                }
                nrm[k].x += ((f32) j - (f32) (p->Width / 2)) * (1.0f / (f32) (int) p->Width);
                nk->z += ((f32) i - (f32) (p->Height / 2)) * (1.0f / (f32) (int) p->Height);
                nk->y *= 0.25f;
                k++;
            }
            asm("" : : "r"(dead));   // COMPILER-DIFF: use of the moved asm set (see above); emits nothing
        }
    }
    {
        u32 n = sizeof(Vec) * (p->Width + 1) * (p->Height + 1);   // nx first: fold attaches the 12 to (ny + 1) as the target
        DCStoreRange(p->pHeightBuf, n);
        DCStoreRange(p->pNorBuf, n);
        DCStoreRange(p->pTexBuf, sizeof(Vec) * (p->Width + 1) * (p->Height + 1));
    }
}

// Espgen move entry for id 0x42: runs the step function unless Stop_flg 0x40000 freezes water.
void Espgen42_Move(cEspgen* pGen)
{
    static void (*Espgen42MoveTbl[])(cEspgen*) = {Espgen42_Move00};

    if (SpfFlagChk(pG, SPF_WATER)) {
        return;
    }
    Espgen42MoveTbl[pGen->Rno0](pGen);
}

// Queues Espgen42_TransSub in the world OT (0x10, layer 1, priority 0x80) while the generator is
// live and not suspended.
void Espgen42_Trans(cEspgen* pGen)
{
    if ((pGen->Be_flg & 1) && !(pGen->Be_flg & 2)) {
        AddOtDirect(0x10, pGen, (void (*)()) Espgen42_TransSub, 1, 0x80, NULL, 0.0f);
    }
}

// Loads indirect texture matrix 1 with the bump scale (indS x 0.001 + 0.01, indT x 0.007 + 0.07).
void SetIndMtx(ESPGEN45_WK* p)
{
    f32 m[2][3];

    m[0][0] = (f32) p->Shimmer_pow1 * 0.001f + 0.01f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = (f32) p->Shimmer_pow2 * 0.007f + 0.07f;
    m[1][2] = 0.0f;
    GXSetIndTexMtx(1, m, 1);
}

// Draws the water: sets up a temporary model with 5 lights (commonWaterLightSet), the material /
// ambient colours, position and normal matrices, the water texture (texId) with the bump map as
// an indirect stage plus `stages` extra TEV stages, then calls the pre-built display list of
// triangle strips; restores the TEV state afterwards.
void Espgen42_TransSub(cEspgen* pGen)
{
    GxStageWork* st;
    ESPGEN45_WK* p;
    void* buf;
    s32 stage;

    if (!(pGen->Be_flg & 1) || (pGen->Be_flg & 2)) {
        return;
    }
    st = &pG->gxStage;
    p = (ESPGEN45_WK*) pGen->Free.buff;
    st->tevStage = 0;
    st->texMap = 0;
    st->texCoord = 0;
    CameraCurrentProjection();
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    cModel model;
    u8 modelPad[0x320 - sizeof(cModel)];
    PSMTXIdentity(model.mat);
    {
        static const Vec p0 = {0.0f, 0.0f, 0.0f};
        static const Vec p1 = {10000.0f, 10000.0f, 10000.0f};
        model.LightInfo.init2(1, 0, &p0, &p1, 0x10);
    }
    model.pos.x = p->Wld_mat[0][3];
    model.pos.y = p->Wld_mat[1][3];
    model.pos.z = p->Wld_mat[2][3];
    LightMgr.setCloth(&model, 5);
    commonWaterLightSet(model.LightInfo.pLight, 5, p->Amb.a);
    GXColor white;
    white.r = white.g = white.b = white.a = 0xFF;
    GXSetChanMatColor(4, white);
    GXSetChanAmbColor(4, p->Amb);
    Mtx nrm;
    Mtx mv;
    Mtx tmp;
    PSMTXConcat(pG->Camera.v_mat, p->Wld_mat, mv);
    PSMTXCopy(p->Wld_mat, tmp);
    tmp[1][1] = p->Size * 0.05f + 100.0f;
    PSMTXConcat(pG->Camera.v_mat, tmp, tmp);
    PSMTXInverse(tmp, nrm);
    PSMTXTranspose(nrm, nrm);
    GXLoadNrmMtxImm(nrm, 0);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    buf = GetDrawTmpBufAddr(0xE);
    if (buf == NULL) {
        pLog->warn(0, 0, "Espgen42() : not enough memory");
        return;
    }
    {
        f32 ofs = 56.0f;
        GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
        GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
        GXCopyTex(buf, 0);
        GXPixModeSync();
        GXInvalidateTexAll();
        {
            GXTexObj tex;
            Mtx tm;
            Mtx pm;
            GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
            GXLoadTexObj(&tex, st->texMap);
            C_MTXLightPerspective(pm, pG->Camera.param.Fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
            PSMTXConcat(pm, mv, tm);
            GXLoadTexMtxImm(tm, 0x1E, 0);
            GXSetTexCoordGen(st->texCoord, 0, 0, 0x1E);
            GXSetTevColor(1, p->Color);
            GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
            GXSetTevColorIn(st->tevStage, 0xF, 2, 8, 0xF);
            if (p->Refrect_type > 0) {
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
            } else {
                GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
            }
            GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
            GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
            stage = st->tevStage;
            st->tevStage++;
            st->texMap++;
            st->texCoord++;
            GXInitTexObj(&tex, p->pTexBuf, p->Width, p->Height, 1, 0, 0, 0);
            GXLoadTexObj(&tex, st->texMap);
            GXSetNumIndStages(1);
            GXSetTexCoordGen(st->texCoord, 1, 4, 0x3C);
            GXSetIndTexOrder(0, st->texCoord, st->texMap);
            GXSetIndTexCoordScale(0, 0, 0);
            SetIndMtx(p);
            GXSetTevIndWarp(stage, 0, 1, 0, 1);
            st->texCoord++;
            st->texMap++;
            if (p->Refrect_type > 1) {
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0, 0, 0xF);
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
            if (p->Refrect_type > 2) {
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0, 0, 0xF);
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
        }
        {
            GXTexObj* tex2 = EspGetTexObj(p->Spec_Tex, 0);
            if (tex2 == NULL) {
                pLog->err(0, 0, "Espgen42 : TexId[%x] invalid.", p->Spec_Tex);
                tex2 = &Specular;
            }
            GXLoadTexObj(tex2, st->texMap);
        }
        {
            Mtx ms;
            Mtx mt;
            Mtx m3;
            PSMTXCopy(pG->Camera.v_mat, m3);
            PSMTXInverse(m3, m3);
            PSMTXTranspose(m3, m3);
            PSMTXScale(ms, 1.0f, -0.5f, 0.0f);
            PSMTXTrans(mt, 0.5f, 0.5f, 1.0f);
            PSMTXConcat(ms, m3, m3);
            PSMTXConcat(mt, m3, m3);
            GXLoadTexMtxImm(m3, 0x21, 0);
        }
        GXSetTexCoordGen(st->texCoord, 0, 1, 0x21);
        GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
        GXSetTevColorIn(st->tevStage, 0xF, 0xA, 9, 0);
        GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
        GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
        st->tevStage++;
        st->texMap++;
        st->texCoord++;
        GXSetNumTevStages(st->tevStage);
        GXSetNumTexGens(st->texCoord);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 3);
        GXSetVtxDesc(10, 3);
        GXSetVtxDesc(13, 1);
        GXSetVtxAttrFmt(0, 9, 1, 4, 0);
        GXSetVtxAttrFmt(0, 10, 0, 4, 0);
        GXSetVtxAttrFmt(0, 13, 1, 4, 0);
        GXSetArray(9, p->pHeightBuf, sizeof(Vec));
        GXSetArray(10, p->pNorBuf, sizeof(Vec));
        GXCallDisplayList(p->pDisplayList, p->Dpl_size);
        GXSetNumTevStages(1);
        GXSetNumTexGens(0);
        GXSetNumIndStages(0);
        GXSetTevDirect(0);
        GXSetTevDirect(1);
    }
}

// Dead-stripped from the DOL (string kept): pulls a generator and sets the surface up.
static cEspgen* SetWater(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
{
    cEspgen* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen42 : work pull failed");
        return NULL;
    }
    return SetWaterWork(w, pos, rot, nx, ny, size, rate);
}

// Builds an nx x ny water grid of cell `size` at pos / rot (y scaled by size x 0.05 + 100, then
// `rate`): allocates the height/speed, normal, rendered position and bump buffers and the display
// list of (nx + 1) x 2 strip vertices per row with texture coordinates, and fills the flat start state.
// Returns NULL (and releases the generator) on memory failure.
cEspgen* SetWaterWork(cEspgen* w, Vec* pos, Vec* rot, u32 nx, u32 ny, f32 size, f32 rate)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) w->Free.buff;
    Mtx m;
    u32 n;
    u8* d;
    int i;
    int j;
    int k;
    f32 fx;
    f32 fy;

    w->Id = 0x42;
    p->Prm_a = 0.05f;
    p->Prm_dmp = 0.95f;
    p->Width = nx;
    p->Height = ny;
    p->Size = size;
    RotMatrix(p->Wld_mat, rot);
    PSMTXScale(m, p->Size, p->Size * 0.05f + 100.0f, p->Size);
    PSMTXConcat(p->Wld_mat, m, p->Wld_mat);
    PSMTXTransApply(p->Wld_mat, p->Wld_mat, pos->x, pos->y, pos->z);
    PSMTXInverse(p->Wld_mat, p->Inv_mat);
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->Wld_mat[1][1] *= rate;
    n = sizeof(f32) * (p->Width + 1) * (p->Height + 1);
#line 1050 "D:/Bio4/Prog/Espgen42.cpp"
    p->pPosBuf = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->pPosBuf == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pPosBuf, n);
#line 1057 "D:/Bio4/Prog/Espgen42.cpp"
    p->pSpdBuf = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->pSpdBuf == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pSpdBuf, n);
    n = sizeof(Vec) * (p->Width + 1) * (p->Height + 1);
#line 1066 "D:/Bio4/Prog/Espgen42.cpp"
    p->pHeightBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pHeightBuf == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pHeightBuf, n);
#line 1073 "D:/Bio4/Prog/Espgen42.cpp"
    p->pNorBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pNorBuf == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pNorBuf, n);
#line 1082 "D:/Bio4/Prog/Espgen42.cpp"
    p->pTexBuf = (u8*) MEM_ALLOC(sizeof(Vec) * p->Width * p->Height, 1, 13);
    if (p->pTexBuf == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    p->Dpl_size = ((p->Width + 1) * 2 * p->Height * 12 + 0x61) & ~0x1F;
#line 1095 "D:/Bio4/Prog/Espgen42.cpp"
    p->pDisplayList = (u8*) MEM_ALLOC(p->Dpl_size, 1, 13);
    if (p->pDisplayList == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pDisplayList, p->Dpl_size);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    d = p->pDisplayList;
    *d = 0;
    d++;
    *d = 0x98;
    d++;
    *(u16*) d = (p->Width + 1) * 2 * p->Height;
    d += 2;
    for (i = 0; i < p->Height; i++) {
        k = i * (p->Width + 1);
        for (j = 0; j < p->Width + 1; j++) {
            *(u16*) d = k;
            d += 2;
            *(u16*) d = k;
            d += 2;
            *(f32*) d = (f32) j / (f32) (p->Width + 1);
            d += 4;
            *(f32*) d = (f32) i / (f32) (p->Height + 1);
            d += 4;
            *(u16*) d = p->Width + (k + 1);
            d += 2;
            *(u16*) d = p->Width + (k + 1);
            d += 2;
            *(f32*) d = (f32) j / (f32) (p->Width + 1);
            d += 4;
            *(f32*) d = (f32) (i + 1) / (f32) (p->Height + 1);
            d += 4;
            k++;
        }
        i++;
        if (i < p->Height) {
            for (j = p->Width; j >= 0; j--) {
                k = i * (p->Width + 1) + j;
                *(u16*) d = k;
                d += 2;
                *(u16*) d = k;
                d += 2;
                *(f32*) d = (f32) j / (f32) (p->Width + 1);
                d += 4;
                *(f32*) d = (f32) i / (f32) (p->Height + 1);
                d += 4;
                *(u16*) d = p->Width + (k + 1);
                d += 2;
                *(u16*) d = p->Width + (k + 1);
                d += 2;
                *(f32*) d = (f32) j / (f32) (p->Width + 1);
                d += 4;
                *(f32*) d = (f32) (i + 1) / (f32) (p->Height + 1);
                d += 4;
            }
        }
    }
    // One counter pair for the init loops: `jj` (inner fRand loop, then the two x edges: it crosses the
    // call, so callee-saved r28) and `i2`/`idx` (fRand rows, `i2 = p->Height` for the far edge, the two y edges).
    fy = 0.0f;
    int jj;
    int i2;
    int idx;
    for (i2 = 0; i2 < p->Height + 1; i2++) {
        idx = i2 * (p->Width + 1);
        fx = 0.0f;
        for (jj = 0; jj < p->Width + 1; jj++) {
            p->pHeightBuf[idx].x = fx - (f32) (int) (p->Width / 2);
            p->pHeightBuf[idx].y = fRand1_1() * 0.2f;
            p->pHeightBuf[idx].z = fy - (f32) (int) (p->Height / 2);
            p->pNorBuf[idx].x = 0.0f;
            p->pNorBuf[idx].y = 1.0f;
            p->pNorBuf[idx].z = 0.0f;
            p->pPosBuf[idx] = 0.0f;
            p->pSpdBuf[idx] = 0.0f;
            fx += 1.0f;
            idx++;
        }
        fy += 1.0f;
    }
    {
        static f32 g42_init_y = 0.0f;
        static f32 g42_init_y2 = 0.0f;
        for (jj = 0; jj < p->Width + 1; jj++) {
            p->pHeightBuf[jj].y = g42_init_y;
        }
        i2 = p->Height;
        idx = i2 * (p->Width + 1);
        for (jj = 0; jj < p->Width + 1; jj++) {
            p->pHeightBuf[idx + jj].y = g42_init_y;
        }
        // The y edges also go through `idx` (one pseudo across all four loops = the target's r8 in every
        // loop), and the far edge is `idx = row; idx += nx` (the product lands in idx's register, not a temp).
        for (i2 = 0; i2 < p->Height + 1; i2++) {
            idx = i2 * (p->Width + 1);
            p->pHeightBuf[idx].y = g42_init_y2;
        }
        for (i2 = 0; i2 < p->Height + 1; i2++) {
            idx = i2 * (p->Width + 1);
            idx += p->Width;
            p->pHeightBuf[idx].y = g42_init_y2;
        }
    }
    // Block-local sizes at the tail: local-alloc ties the `nx + 1` temp into them (`addi r30; mullw r30`);
    // the function-level `n` is only the MEM_ALLOC size.
    {
        u32 n2 = sizeof(Vec) * (p->Width + 1) * (p->Height + 1);
        DCStoreRange(p->pHeightBuf, n2);
        DCStoreRange(p->pNorBuf, n2);
    }
    DCStoreRange(p->pTexBuf, sizeof(Vec) * (p->Width + 1) * (p->Height + 1));
    {
        u32 n3 = sizeof(f32) * (p->Width + 1) * (p->Height + 1);
        DCStoreRange(p->pPosBuf, n3);
        DCStoreRange(p->pSpdBuf, n3);
    }
    DCStoreRange(p->pDisplayList, p->Dpl_size);
    return w;
}

// Frees all grid buffers and forgets the room water generator.
void Espgen42_Destruct(cEspgen* pGen)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) pGen->Free.buff;

    if (p->pPosBuf != NULL) {
        Mem_free(p->pPosBuf);
        p->pPosBuf = NULL;
    }
    if (p->pSpdBuf != NULL) {
        Mem_free(p->pSpdBuf);
        p->pSpdBuf = NULL;
    }
    if (p->pHeightBuf != NULL) {
        Mem_free(p->pHeightBuf);
        p->pHeightBuf = NULL;
    }
    if (p->pNorBuf != NULL) {
        Mem_free(p->pNorBuf);
        p->pNorBuf = NULL;
    }
    if (p->pTexBuf != NULL) {
        Mem_free(p->pTexBuf);
        p->pTexBuf = NULL;
    }
    if (p->pDisplayList != NULL) {
        Mem_free(p->pDisplayList);
        p->pDisplayList = NULL;
    }
    g_pWater = NULL;
}

// Espgen SetFreeWork for id 0x42: sets up the room water from the room's effect data. It needs
// the noise texture 0xFE and runs one move step at once.
int Espgen42_SetFreeWork(cEspgen* pGen, cEspSeqTbl* pSeq, cEspSeqHead* pSeqHed, cModel* pMod, u16 Null_parts_no, Mtx* pMat,
                         Vec* pOffset, Vec* pAng, ESPSEQ_CONTROL* pSct)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) pGen->Free.buff;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;
    f32 rate;

    if (EspGetTexObj(0xFE, 0) == NULL) {
        pLog->err(0, 0, "Espgen42 : WaterTex(0xfe) not found!");
        return 0;
    }
    if (pSeq->WorkSp8[0] != 0) {
        nx = pSeq->WorkSp8[0];
        if (nx > 0xB8) {
            nx = 0xB8;
            pLog->warn(0, 0, "ESP_WATER : width > 184");
        }
    }
    if (pSeq->WorkSp8[1] != 0) {
        ny = pSeq->WorkSp8[1];
        if (ny > 0xB8) {
            ny = 0xB8;
            pLog->warn(0, 0, "ESP_WATER : height > 184");
        }
    }
    if (nx & 7) {
        u32 n = nx - (nx & 7);
        pLog->warn(0, 0, "ESP_WATER : width (%d -> %d)", nx, n);
        nx = n;
    }
    if (ny & 7) {
        u32 n = ny - (ny & 7);
        pLog->warn(0, 0, "ESP_WATER : height (%d -> %d)", ny, n);
        ny = n;
    }
    rate = 1.0f - (f32) (int) pSeq->WorkSp8[2] / 255.0f;
    PSVECScale(&pSeq->Ang, &r, 6.28f / 360.0f);
    if (SetWaterWork(pGen, (Vec*) &pSeq->Pos.x, &r, nx, ny, pSeq->Size_base_x, rate) != NULL) {
        p->Color.r = pSeq->Col_start_r;
        p->Color.g = pSeq->Col_start_g;
        p->Color.b = pSeq->Col_start_b;
        p->Color.a = pSeq->Col_start_a;
        p->Amb.r = pSeq->Col_d_r * 255.0f;
        p->Amb.g = pSeq->Col_d_g * 255.0f;
        p->Amb.b = pSeq->Col_d_b * 255.0f;
        p->Amb.a = pSeq->Col_d_a * 255.0f;
        p->Type = pSeq->Work8[0];
        if (p->Type == 2) {
            p->Prm_a = 0.5f - (f32) (s8) pSeq->Work8[1] * 0.005f;
            if (p->Prm_a > 0.5f) {
                p->Prm_a = 0.5f;
            }
            if (p->Prm_a < 0.0f) {
                p->Prm_a = 0.0f;
            }
            p->Prm_dmp = 0.99f - (f32) (int) pSeq->Work8[2] * 0.001f;
        }
        p->Spec_Tex = pSeq->Tex_id;
        p->Shimmer_pow1 = pSeq->prm.h.xCE;
        p->Shimmer_pow2 = pSeq->prm.h.xD2;
        p->Refrect_type = pSeq->Work8[3];
        g_pWater = pGen;
        Espgen42_Move(pGen);
        return 1;
    }
    return 0;
}

asm(".section .sdata; .balign 8");
