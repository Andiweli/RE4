// game/espgen45: effect controller 45, the open-air water surface (D:/Bio4/Prog/espgen45.cpp) that
// follows the camera target. Room code tunes it through the Estgen45Set* entry points.
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

// Effect controller 45: weather water surface (same height-field model as Espgen42, following the
// camera). The Estgen45Set* entry points let the room script (esp4c) override its parameters.
// Espgen42 owns the water init (EspWaterInit): it resets this unit's overrides and g_pWater45.

void Espgen45_Move00(cEspgen* w);
void Espgen45_TransSub(cEspgen* w);
static void SetIndMtx(ESPGEN45_WK* p);   // this unit's own copy; Espgen42.cpp has its own separate one
cEspgen* SetWaterWork45(cEspgen* w, Vec* pos, Vec* rot, u32 nx, u32 ny, f32 size, f32 rate);

cEspgen* g_pWater45;
static int g_bTargetCamera = 1;
static int g_bTargetHeight = 1;
static int g_bSizeOverWrite = 0;
static int g_bColorOverWrite = 0;
static int g_bColorMul = 0;
static int g_bSetParam = 0;
static f32 g_Target_x = 0.0f;
static f32 g_Target_y = 0.0f;
static f32 g_Target_z = 0.0f;
static f32 g_Size = 0.0f;
static u8 g_r = 0;
static u8 g_g = 0;
static u8 g_b = 0;
static u8 g_a = 0;
static f32 g_sr = 0.0f;
static f32 g_sg = 0.0f;
static f32 g_sb = 0.0f;
static f32 g_sa = 0.0f;
static f32 inv_mul = 1.0f;
static ESP4C_WK g_Free;


// Resets the room override state (camera-follow on, height-follow on, all overwrites off): called by
// EspWaterInit (Espgen42.cpp) at effect system init.
void Espgen45_static_init()
{
    g_bTargetCamera = 1;
    g_bTargetHeight = 1;
    g_Target_x = 10000000000.0f;
    g_Target_y = -10000000000.0f;
    g_Target_z = 100000000000.0f;
    g_bSizeOverWrite = 0;
    g_bColorOverWrite = 0;
    g_bColorMul = 0;
    g_bSetParam = 0;
    g_Size = 0.0f;
    g_r = 0;
    g_g = 0;
    g_b = 0;
    g_a = 0;
    g_sr = 0.0f;
    g_sg = 0.0f;
    g_sb = 0.0f;
    g_sa = 0.0f;
}

// u8 to f32 through GQR2 from a stack byte, since the compiler only emits psq_l from its own fpmem slot.
// The volatile asm is also the scheduling barrier that gives the target's order in loop A. Same definition
// as trans.cpp, kept in this unit so asmcheck.py counts it.
#define PSQ_L_U8_TO(dst, p) asm volatile("psq_l %0,0(%1),1,2" : "=f"(dst) : "b"(p) : "memory")

// Bump texture (I8, 8x4 tiles) index of grid point (x, y). x/8 before y/4 (the two signed divisions are
// separate blocks, so their order is the source order) and `(y / 4) << 5`: with `* 32` fold would
// reassociate the constant onto `(w1) >> 3` and hoist `(w1 >> 3) * 32`; the target keeps `srwi` in the loop.
#define BUMP_INDEX(x, y, w1) (((x) / 8) * 32 + (((y) / 4) << 5) * ((w1) >> 3) + (((y) & 3) << 3) + ((x) & 7))
// Noise texture (0xFE) index of grid point (x, y).
#define NOISE_INDEX(x, y) ((((y) << 6) & 0xB00) + (((x) << 2) & 0xA0) + (((y) & 3) << 3) + ((x) & 7))

// Step 0 of Espgen45MoveTbl: recentres the surface on the camera target and simulates one frame of
// waves, a damped wave equation or, in mode 1, a spring model. Debug: L trigger with Debug_flg[1]
// 0x00800000 drops a wave in the middle.
void Espgen45_Move00(cEspgen* pGen)
{
    static f32 g45_wave_mul = 0.001f;
    static f32 wt_pow = 10.0f;
    ESPGEN45_WK* p = (ESPGEN45_WK*) pGen->Free.buff;
    Vec d0;
    Vec d1;
    Vec v;
    Vec d2;
    Vec d3;
    Mtx m;
    u8 tmp;
    GXTexObj* tex;
    u8* noise;
    f32* c;
    u32 frame;
    f32 size;
    f32 rate;
    u8 rotY;
    u8 mode;
    int i;
    int j;
    int k;
    int nz;   // noise index / byte of both loops: one function-level pseudo (see loop A)
    // loop A's `j / 8` and loop B's `(i / 4) << 5`: one function-level pseudo (r10 in both loops; a block-local
    // `(i / 4) << 5` would be tied into the `mullw` by local-alloc, the target ties the nx term).
    int jx;
    // loop B's `k + p->Width` (before the call) and `p->Width` (after it): one function-level pseudo (r11 in both places,
    // "set 2 times; dies in 2 places" = no local-alloc tie, so the Z block's `addi r9,r11,1` starts a fresh r9 chain).
    int mx;
    // noise value of both loops: also one function-level pseudo (global alloc, f10 in both loops). A block-local `n`
    // ties to the loop-A psq_l output / loop-B frsp result and permutes the loop-A/B FPR names and the sched2 slots
    // of the loop-B sum chain (77 -> 47 words).
    f32 n;
    // normal pointer `&nrm[k]` of both loops: one function-level pseudo (36 refs / 170 insns, 1.06) ranks above loop B's
    // k*4 giv (35 / 169, 1.04) and k (32 / 215, 0.74) in global alloc and takes r30 for both loops; k*4 then finds r30
    // taken and gets r29, k r31. A block-local pointer per loop (18 / 111, 0.65) came after both (k*4 r30, pointer r29).
    Vec* nk;
    // `j & 7` of both loops (noise index and bump index): one function-level pseudo ranks above `i` in global alloc and
    // takes r24 (i r23, (k-nx)*12 r26); a block-local `j & 7` per loop came after i, which then took r24. u8 as in
    // Espgen42 (there the narrow store is the +1 loop.c insn that keeps the 4.0 pool pair for the outer pass).
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
    if (g_bTargetCamera == 1) {
        g_Target_x = pG->Camera.param.Target.x;
        g_Target_z = pG->Camera.param.Target.z;
    }
    p->Pos.x = g_Target_x;
    p->Pos.z = g_Target_z;
    if (g_bTargetHeight == 1) {
        p->Pos.y = g_Target_y;
    } else {
        p->Pos.y = p->Base_y;
    }
    size = p->Size;
    if (g_bSizeOverWrite == 1) {
        size = g_Size;
    }
    if (g_bSetParam == 0) {
        PSMTXScale(p->Wld_mat, size, size * 0.05f + 100.0f, size);
    } else {
        RotMatrix(p->Wld_mat, &g_Free.ang);
        PSMTXScale(m, size, size * 0.05f + 100.0f, size);
        PSMTXConcat(p->Wld_mat, m, p->Wld_mat);
    }
    if (g_bSetParam == 0) {
        rotY = p->wave_ratio_base;
    } else {
        rotY = g_Free.wave_ratio_base;
    }
    rate = 1.0f - (f32) (int) rotY / 255.0f;
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->Wld_mat[1][1] *= rate;
    PSMTXTransApply(p->Wld_mat, p->Wld_mat, p->Pos.x, p->Pos.y, p->Pos.z);
    PSMTXInverse(p->Wld_mat, p->Inv_mat);
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
    f32 hx = (f32) (int) (nx / 2);
    f32 hy = (f32) (int) (p->Height / 2);
    f32 inx = 1.0f / (f32) (int) nx * inv_mul;
    f32 iny = 1.0f / (f32) (int) p->Height * inv_mul;
    if (g_bSetParam == 0) {
        mode = p->Type;
    } else {
        mode = g_Free.Type;
    }
    if (mode != 1) {
        if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && (Joy[0].on & 0x100)) {
            // The index is the loop variable `k` (target `lwz r31` = k's register, base+index `lfsx f0,hB,k4`).
            k = (int) ((f32) (int) (p->Width * p->Height) * 0.5f);
            // Byte offset in a variable: inside an address `p->pSpdBuf[k]` expands to `(plus (mult k 4) hB)` (expr.c
            // both_summands puts a MULT first) = `lfsx k4,hB`; a register index keeps `(plus hB k4)` = `lfsx hB,k4`.
            u32 k4 = k * 4;
            *(f32*) ((u8*) p->pSpdBuf + k4) -= wt_pow;
        }
        f32 damp;
        f32 spread;
        if (g_bSetParam == 0) {
            damp = p->Prm_a;
        } else {
            damp = g_Free.Prm_a;
        }
        f32 cdamp = 2.0f - damp * 4.0f;
        if (g_bSetParam == 0) {
            spread = p->Prm_dmp;
        } else {
            spread = g_Free.Prm_dmp;
        }
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
            k = i * (nx + 1);   // two statements: the product lands in k's register (`mullw r31; addi r31,r31,1`)
            k++;
            for (j = 1; j < (int) nx; j++) {
                int i3 = (i & 3) << 3;
                // The k*4 giv is discovered BEFORE the k*12 giv (`&p->pHeightBuf[k]` below): loop.c emits the giv inits in bl->giv
                // order = reverse discovery, so the preheader is `mr r29,k12 | slwi r31,k,2` (k4 init last) and the two
                // `mulli` take r8/r10 as the target. With `c += k` as the first k*4 use the two inits were swapped.
                u32 k4 = k * 4;
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
                next[k] = damp * sum + cdamp * cur[k] - next[k];
                // FGet: a reference read is a MEM with neither the struct nor the scalar flag, so it depends on the
                // `stfsx next[k]` store (next's alias base is 0) and issues right after it like the target's
                // `lfs g45_wave_mul`; the plain static read is a fixed scalar that never aliases the in-struct store
                // and floated 6 insns up. The pos address is computed before the store (target `lwz pos` early).
                pv->y = next[k] = (n * g45_wave_mul + next[k]) * spread;
                Vec* nrm = p->pNorBuf;   // before the v.x/v.z reads: kept across the call (`lfsx nrm[k].x`, `4(nrm+k*12)`)
                v.x = p->pHeightBuf[k - 1].y - p->pHeightBuf[k + 1].y;
                v.y = 2.0f;
                v.z = p->pHeightBuf[k - nx].y - p->pHeightBuf[k + nx].y;
                nk = &nrm[k];   // the function-level pointer (see its declaration); `nrm[k].y/.z` below fold onto it in cse
                PSVECScale(&v, nk, 1.0f / 2.3f);
                {
                    // BUMP_INDEX with the function-level `j7` as the last term. Both divisions go through one
                    // temp `t` so both temps share r0 as in the target, and `jx << 5` is a shift because a MULT
                    // in an address sum would start the add chain, which the target starts with the i term.
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
        // 4.0 pool pair (threshold 71 - 3 per moved insn), so 4.0 stays for the outer pass (outer preheader) and
        // 0.0018 moves in the inner pass 2. A codeless asm set of `i` used after the inner loop is that extra moved
        // insn (no register: it takes a free callee-saved GPR); emits nothing. The real construct is unknown.
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
                PSVECScale(&v, nk, 1.0f / 2.3f);
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
                    // at t2 and the xoris before the add: 49 words in the Z block. What the original had there is unknown.
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

// EspgenMoveTbl entry for controller type 0x45; frozen while Stop_flg bit 0x40000 is set.
void Espgen45_Move(cEspgen* pGen)
{
    static void (*Espgen45MoveTbl[])(cEspgen*) = {Espgen45_Move00};

    if (SpfFlagChk(pG, SPF_WATER)) {
        return;
    }
    Espgen45MoveTbl[pGen->Rno0](pGen);
}

// EspgenTransTbl entry: queues Espgen45_TransSub in OT layer 0x10 (drawn after the opaque scene) and
// clears Status_flg[1] bit 0x20 (the "override parameters changed this frame" flag).
void Espgen45_Trans(cEspgen* pGen)
{
    if ((pGen->Be_flg & 1) && !(pGen->Be_flg & 2)) {
        AddOtDirect(0x10, pGen, (void (*)()) Espgen45_TransSub, 1, 0x80, NULL, 0.0f);
    }
    StaFlagOff(pG, STA_ESPGEN45_SET);
}

// Loads indirect texture matrix 1 for the bump stage: S scale indS*0.001+0.01, T scale
// indT*0.007+0.07 (or the ESP4C_WK Shimmer_pow1/2 when the parameter override is on).
static void SetIndMtx(ESPGEN45_WK* p)
{
    f32 m[2][3];
    s16 indS;
    s16 indT;

    if (g_bSetParam == 0) {
        indS = p->Shimmer_pow1;
    } else {
        indS = g_Free.Shimmer_pow1;
    }
    if (g_bSetParam == 0) {
        indT = p->Shimmer_pow2;
    } else {
        indT = g_Free.Shimmer_pow2;
    }
    m[0][0] = (f32) indS * 0.001f + 0.01f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = (f32) indT * 0.007f + 0.07f;
    m[1][2] = 0.0f;
    GXSetIndTexMtx(1, m, 1);
}

// Border quads of the unbounded surface: grid half sizes in grid units; the far edge is
// g45_mul cells out, the near edge g45_mul2, normals follow SetWaterWork45's slope.
#define G45_NXH ((f32) (int) (p->Width >> 1))
#define G45_NXHN ((f32) (-(int) p->Width / 2))
#define G45_NX ((f32) (int) p->Width)
#define G45_NXN ((f32) (-(int) p->Width))
#define G45_NY ((f32) (int) p->Height)
#define G45_NYN ((f32) (-(int) p->Height))
#define G45_NYU ((f32) p->Height)

// One vertex = one inline with all eight values as parameters, normal first: every value is evaluated
// before the first FIFO store (one (f32)(-ny) conversion serves the normal z and the position z), and the
// ny conversion precedes the nx one because the normal arguments come first.
static inline void Vtx45(f32 nx, f32 ny, f32 nz, f32 x, f32 y, f32 z, f32 s, f32 t)
{
    GXPosition3f32(x, y, z);
    GXNormal3f32(nx, ny, nz);
    GXTexCoord2f32(s, t);
}

// Draws the water with a screen copy as the refraction texture, a bump indirect stage, the specular
// and optional mask textures, the far border quads unless flag bit 0, then the grid display list.
void Espgen45_TransSub(cEspgen* w)
{
    static f32 g45_mul = 15.0f;
    static f32 g45_mul2 = 1.0f;
    GxStageWork* st;
    ESPGEN45_WK* p;
    void* buf;
    s32 stage;

    if (!(w->Be_flg & 1) || (w->Be_flg & 2)) {
        return;
    }
    st = &pG->gxStage;
    p = (ESPGEN45_WK*) w->Free.buff;
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
    GXColor amb = p->Amb;
    if (g_bColorOverWrite == 1) {
        amb.r = (u8) (g_sr * 255.0f);
        amb.g = (u8) (g_sg * 255.0f);
        amb.b = (u8) (g_sb * 255.0f);
        amb.a = (u8) (g_sa * 255.0f);
    } else if (g_bColorMul == 1) {
        amb.r = (u8) ((f32) amb.r * g_sr);
        amb.g = (u8) ((f32) amb.g * g_sg);
        amb.b = (u8) ((f32) amb.b * g_sb);
        amb.a = (u8) ((f32) amb.a * g_sa);
    }
    commonWaterLightSet(model.LightInfo.pLight, 5, amb.a);
    GXColor white;
    white.r = white.g = white.b = white.a = 0xFF;
    GXSetChanMatColor(4, white);
    GXSetChanAmbColor(4, amb);
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
        pLog->warn(0, 0, "Espgen45() : not enough memory");
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
            GXColor col = p->Color;
            if (g_bColorOverWrite == 1) {
                col.r = g_r;
                col.g = g_g;
                col.b = g_b;
                col.a = g_a;
            } else if (g_bColorMul == 1) {
                col.r = (u8) ((f32) col.r * (f32) (int) g_r / 255.0f);
                col.g = (u8) ((f32) col.g * (f32) (int) g_g / 255.0f);
                col.b = (u8) ((f32) col.b * (f32) (int) g_b / 255.0f);
                col.a = (u8) ((f32) col.a * (f32) (int) g_a / 255.0f);
            }
            GXSetTevColor(1, col);
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
                pLog->err(0, 0, "Espgen45 : TexId[%x] invalid.", p->Spec_Tex);
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
        if ((g_bSetParam == 1 && (g_Free.flag & 2)) || (g_bSetParam == 0 && (p->flag & 2))) {
            u8 texId;
            ESP_TEX_WK* tw;
            if (g_bSetParam == 1) {
                texId = g_Free.Mask_Tex;
            } else {
                texId = p->Mask_Tex;
            }
            tw = EspGetTexWk(texId, 1);
            if (tw == NULL || tw->Owner == EFF_NONE) {
                pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", texId);
            } else {
                // the same frame slots as the first block's tex/tm: PRE shares their addresses
                GXTexObj tex;
                GXTlutObj tlut;
                TEXDescriptor* td = TEXGet(tw->Tpl_addr, 0);
                TEXHeader* th = td->textureHeader;

                if (th->format == 8 || th->format == 9) {
                    GXInitTexObjCI(&tex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                    GXInitTlutObj(&tlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                    GXLoadTlut(&tlut, 1);
                } else {
                    GXInitTexObj(&tex, th->data, th->width, th->height, th->format, 0, 0, 0);
                }
                GXLoadTexObj(&tex, st->texMap);
                GXLoadTexMtxImm(tw->_Mtx, 0x21, 1);
                GXSetTexCoordGen(st->texCoord, 1, 4, 0x21);
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0xF, 0xF, 0);
                GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 4, 5, 7);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
        }
        GXSetNumTevStages(st->tevStage);
        GXSetNumTexGens(st->texCoord);
        if (!(p->flag & 1)) {
            Vec n;
            f32 hx;
            f32 hy;
            f32 inx;
            f32 iny;
            f32 far;

            GXClearVtxDesc();
            GXSetVtxDesc(9, 1);
            GXSetVtxDesc(0xA, 1);
            GXSetVtxDesc(0xD, 1);
            GXSetVtxAttrFmt(0, 9, 1, 4, 0);
            GXSetVtxAttrFmt(0, 0xA, 0, 4, 0);
            GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
            hx = G45_NXH;
            hy = (f32) (int) (p->Height >> 1);
            inx = 1.0f / G45_NX * inv_mul;
            iny = 1.0f / G45_NY * inv_mul;
            far = g45_mul / g45_mul2;
            {
                GXBegin(0x80, 0, 4);
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45(nx, 0.25f, (G45_NYN * g45_mul2 + hy) * iny * far, G45_NXHN * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 0.0f, 0.0f);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NYN * g45_mul2 + hy) * iny * far, G45_NXH * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 1.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, nz, G45_NXH * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 1.0f, 1.0f);
                n.x = nx;
                n.y = 0.25f;
                n.z = nz;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 0.0f, 1.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, nz, G45_NXH * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 0.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx * far, 0.25f, nz, G45_NXH * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 1.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx * far, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXH * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 1.0f, 1.0f);
                n.x = (G45_NX * g45_mul2 - hx) * inx;
                n.y = 0.25f;
                n.z = (G45_NY * g45_mul2 - hy) * iny;
                Vtx45(n.x, n.y, n.z, G45_NXH * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 0.0f, 1.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45(nx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXHN * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 0.0f, 1.0f);
                n.x = nx;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXH * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 1.0f, 1.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NY * g45_mul2 - hy) * iny * far, G45_NXH * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 1.0f, 0.0f);
                n.z = (G45_NY * g45_mul2 - hy) * iny * far;
                n.y = 0.25f;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 0.0f, 0.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45((G45_NXN * g45_mul2 + hx) * inx * far, 0.25f, nz, G45_NXHN * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 0.0f, 0.0f);
                Vtx45(nx, 0.25f, nz, G45_NXHN * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 1.0f, 0.0f);
                Vtx45(nx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXHN * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 1.0f, 1.0f);
                n.x = (G45_NXN * g45_mul2 + hx) * inx * far;
                n.y = 0.25f;
                n.z = (G45_NY * g45_mul2 - hy) * iny;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 0.0f, 1.0f);
            }
        }
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

// Allocates a controller from the pool and builds a water surface on it (SetWaterWork45).
// Dead-stripped from the DOL (string kept, no pool: STRIP_UNUSED): pulls a generator and sets the
// surface up.
static cEspgen* SetWater(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
{
    cEspgen* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen45 : work pull failed");
        return NULL;
    }
    return SetWaterWork45(w, pos, rot, nx, ny, size, rate);
}

// Builds the surface work: id 0x45, nx x ny cells of `size` units, matrix (with the rotation
// override), allocates the height/speed, rendered position, normal and bump buffers plus the strip
// display list (memory group 13), fills the
// zig-zag triangle strip indices/UVs, the flat grid positions (random +-0.2 ripple), the sloped
// normals and zero edge heights. Returns NULL (controller released) when an allocation fails.
cEspgen* SetWaterWork45(cEspgen* w, Vec* pos, Vec* rot, u32 nx, u32 ny, f32 size, f32 rate)
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

    w->Id = 0x45;
    p->Width = nx;
    p->Height = ny;
    p->Size = size;
    p->Prm_a = 0.05f;
    p->Prm_dmp = 0.95f;
    p->Pos = *pos;
    if (g_bSetParam == 0) {
        PSMTXScale(p->Wld_mat, p->Size, p->Size * 0.05f + 100.0f, p->Size);
    } else {
        RotMatrix(p->Wld_mat, &g_Free.ang);
        PSMTXScale(m, p->Size, p->Size * 0.05f + 100.0f, p->Size);
        PSMTXConcat(p->Wld_mat, m, p->Wld_mat);
    }
    PSMTXTransApply(p->Wld_mat, p->Wld_mat, p->Pos.x, p->Pos.y, p->Pos.z);
    PSMTXInverse(p->Wld_mat, p->Inv_mat);
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->Wld_mat[1][1] *= rate;
    n = sizeof(f32) * (p->Width + 1) * (p->Height + 1);
#line 1452 "D:/Bio4/Prog/espgen45.cpp"
    p->pPosBuf = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->pPosBuf == NULL) {
        goto nomem;
    }
    memclr_asm(p->pPosBuf, n);
#line 1459 "D:/Bio4/Prog/espgen45.cpp"
    p->pSpdBuf = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->pSpdBuf == NULL) {
        goto nomem;
    }
    memclr_asm(p->pSpdBuf, n);
    n = sizeof(Vec) * (p->Width + 1) * (p->Height + 1);
#line 1468 "D:/Bio4/Prog/espgen45.cpp"
    p->pHeightBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pHeightBuf == NULL) {
        goto nomem;
    }
    memclr_asm(p->pHeightBuf, n);
#line 1475 "D:/Bio4/Prog/espgen45.cpp"
    p->pNorBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pNorBuf == NULL) {
        goto nomem;
    }
    memclr_asm(p->pNorBuf, n);
#line 1484 "D:/Bio4/Prog/espgen45.cpp"
    p->pTexBuf = (u8*) MEM_ALLOC(sizeof(Vec) * p->Width * p->Height, 1, 13);
    if (p->pTexBuf == NULL) {
        goto nomem;
    }
    p->Dpl_size = ((p->Width + 1) * 2 * p->Height * 12 + 0x61) & ~0x1F;
#line 1497 "D:/Bio4/Prog/espgen45.cpp"
    p->pDisplayList = (u8*) MEM_ALLOC(p->Dpl_size, 1, 13);
    if (p->pDisplayList == NULL) {
    nomem:
        pLog->err(0, 0, "Espgen45 : not enough memory");
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
            fx += 1.0f;
            p->pHeightBuf[idx].z = fy - (f32) (int) (p->Height / 2);
            p->pNorBuf[idx].x = 0.0f;
            p->pNorBuf[idx].y = 1.0f;
            p->pNorBuf[idx].z = 0.0f;
            p->pPosBuf[idx] = 0.0f;
            p->pSpdBuf[idx] = 0.0f;
            Vec* n = &p->pNorBuf[idx];
            n->x += ((f32) jj - (f32) (int) (p->Width / 2)) * (1.0f / (f32) (int) p->Width);
            n->z += ((f32) i2 - (f32) (int) (p->Height / 2)) * (1.0f / (f32) (int) p->Height);
            n->y *= 0.25f;
            idx++;
        }
        fy += 1.0f;
    }
    {
        static f32 g45_init_y = 0.0f;
        static f32 g45_init_y2 = 0.0f;
        for (jj = 0; jj < p->Width + 1; jj++) {
            p->pHeightBuf[jj].y = g45_init_y;
        }
        i2 = p->Height;
        idx = i2 * (p->Width + 1);
        for (jj = 0; jj < p->Width + 1; jj++) {
            p->pHeightBuf[idx + jj].y = g45_init_y;
        }
        // The y edges also go through `idx` (one pseudo across all four loops = the target's r8 in every
        // loop), and the far edge is `idx = row; idx += nx` (the product lands in idx's register, not a temp).
        for (i2 = 0; i2 < p->Height + 1; i2++) {
            idx = i2 * (p->Width + 1);
            p->pHeightBuf[idx].y = g45_init_y2;
        }
        for (i2 = 0; i2 < p->Height + 1; i2++) {
            idx = i2 * (p->Width + 1);
            idx += p->Width;
            p->pHeightBuf[idx].y = g45_init_y2;
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

// Frees the six grid buffers and clears g_pWater45.
void Espgen45_Destruct(cEspgen* pGen)
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
    g_pWater45 = NULL;
}

// Builds the water from the effect record, registers g_pWater45 and runs the first move. Returns 0
// when the noise texture 0xFE or memory is missing.
int Espgen45_SetFreeWork(cEspgen* pGen, cEspSeqTbl* pSeq, cEspSeqHead* pSeqHed, cModel* pMod, u16 Null_parts_no, Mtx* pMat,
                         Vec* pOffset, Vec* pAng, ESPSEQ_CONTROL* pSct)
{
    ESPGEN45_WK* p = (ESPGEN45_WK*) pGen->Free.buff;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;
    f32 rate;

    if (EspGetTexObj(0xFE, 0) == NULL) {
        pLog->err(0, 0, "Espgen45 : WaterTex(0xfe) not found!");
        return 0;
    }
    if (pSeq->Tool_flg & 1) {
        p->flag |= 1;
    }
    if (pSeq->Tool_flg & 0x4000) {
        p->flag |= 2;
        p->Mask_Tex = pSeq->MaskTex_id;
        p->flag |= 1;
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
    p->wave_ratio_base = pSeq->WorkSp8[2];
    PSVECScale(&pSeq->Ang, &r, 6.28f / 360.0f);
    if (SetWaterWork45(pGen, (Vec*) &pSeq->Pos.x, &r, nx, ny, pSeq->Size_base_x, rate) != 0) {
        p->Color.r = pSeq->Col_start_r;
        p->Color.g = pSeq->Col_start_g;
        p->Color.b = pSeq->Col_start_b;
        p->Color.a = pSeq->Col_start_a;
        p->Amb.r = pSeq->Col_d_r * 255.0f;
        p->Amb.g = pSeq->Col_d_g * 255.0f;
        p->Amb.b = pSeq->Col_d_b * 255.0f;
        p->Amb.a = pSeq->Col_d_a * 255.0f;
        p->Type = pSeq->Work8[0];
        p->Base_y = p->Pos.y;
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
        g_pWater45 = pGen;
        Espgen45_Move(pGen);
        return 1;
    }
    return 0;
}

// Room override: 1 = the surface follows the camera target (default), 0 = uses Estgen45SetTargetPos.
void Estgen45SetTargetCamera(int bTc)
{
    g_bTargetCamera = bTc;
}

// Room override: 1 = surface height from Estgen45SetHeight, 0 = the record's Base_y.
void Estgen45SetTargetHeight(int bTc)
{
    g_bTargetHeight = bTc;
}

// Room override: use the Estgen45SetSize size instead of the record's.
void Estgen45SetSizeOverWrite(int bTc)
{
    g_bSizeOverWrite = bTc;
}

// Room override: replace the record colours by the Estgen45SetColor values.
void Estgen45SetColorOverWrite(int bTc)
{
    g_bColorOverWrite = bTc;
}

// Room override: multiply the record colours by the Estgen45SetColor values.
void Estgen45SetColorMul(int bTc)
{
    g_bColorMul = bTc;
}

// Room override: take Type/wave ratio/damp/spread/indirect/rotation/mask from the ESP4C_WK block.
void Estgen45SetParamOverWrite(int bTc)
{
    g_bSetParam = bTc;
}

// Sets the override centre of the surface and flags Status_flg[1] bit 0x20.
void Estgen45SetTargetPos(f32 x, f32 z)
{
    g_Target_x = x;
    g_Target_z = z;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override water height.
void Estgen45SetHeight(f32 y)
{
    g_Target_y = y;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override cell size.
void Estgen45SetSize(f32 size)
{
    g_Size = size;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override tev colour (r,g,b,a) and ambient/scale factors (rs..as, 0..1).
void Estgen45SetColor(u8 r, u8 g, u8 b, u8 a, f32 sr, f32 sg, f32 sb, f32 sa)
{
    g_r = r;
    g_g = g;
    g_b = b;
    g_a = a;
    g_sr = sr;
    g_sg = sg;
    g_sb = sb;
    g_sa = sa;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Copies the esp4c parameter block used when the parameter override is on.
void Estgen45SetParam(ESP4C_WK* pFree)
{
    g_Free = *pFree;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

asm(".section .sdata; .balign 8");
