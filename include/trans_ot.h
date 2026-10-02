#ifndef TRANS_OT_H
#define TRANS_OT_H

#include "types.h"
#include "vec.h"

// game/trans_ot.cpp: ordering-table draw lists. 23 tables (OT_MAX); each has `max` depth buckets whose
// heads chain downwards (bucket n -> n-1), entries are inserted after the bucket head.
#define OT_MAX 0x17

struct OtData {
    void* data;              // 0x00  argument of func
    void (*func)(void*);     // 0x04
    OtData* next;            // 0x08
    u16 kind;                // 0x0C  OtGetPrevKind() of the entry drawn before the current one
    u8 pad_E[2];
};

// Allocated (PrimBuff) entry: the list node plus the MakeOtData argument.
struct OtPrim {
    OtData ot;               // 0x00
    void* data;              // 0x10
};

struct OtWork {
    OtData* list;  // 0x00  bucket heads (max entries)
    u16 max;       // 0x04
    u16 prev_kind; // 0x06  kind of the last entry executed
};

struct OtMirrorWork {
    u8 pad_0[0x1C];
};

extern OtWork g_OtWork[OT_MAX];
extern OtMirrorWork g_OtMirrirWk[2];
extern f32 OT_MUL;
extern int g_NowExecOtType;

void InitOt();
void ClearOt();
void clearOtWork(OtWork* w);
OtData* MakeOtData(void* data);
int AddOtWorldPos(void* data, void (*func)(void*), Vec* pos, u16 kind, f32 zlimit);
int AddOtWorldPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit);
int AddOtModelPosRadius(void* data, void (*func)(void*), Vec* pos, f32 radius, u16 kind, f32 zlimit);
// Queue `func` in ordering table `ot`; `no` is the slot (clamped), `pos`/`radius` do a frustum cull when given.
int AddOtDirect(int ot, void* data, void (*func)(), u32 no, u16 flag, Vec* pos, f32 radius);
enum OT_TYPE {
    OT_TYPE_TEX_RENDER0 = 0,
    OT_TYPE_TEX_RENDER1 = 1,
    OT_TYPE_SHADOW_SETUP = 2,
    OT_TYPE_SUBSCRN_FAR = 3,
    OT_TYPE_SCROLL = 4,
    OT_TYPE_SUBSCRN = 5,
    OT_TYPE_MODEL = 6,
    OT_TYPE_SHADOW_DRAW = 7,
    OT_TYPE_SUBSCRN_NEAR = 8,
    OT_TYPE_EFFECT = 9,
    OT_TYPE_WORLD = 10,
    OT_TYPE_EFFECT_VU1 = 11,
    OT_TYPE_SCREEN = 12,
    OT_TYPE_COCKPIT = 13,
    OT_TYPE_ID_MODEL = 14,
    OT_TYPE_MESSAGE = 15,
    OT_TYPE_AFTER_RENDER = 16,
    OT_TYPE_DEBUG = 17,
    OT_TYPE_MAX = 18
};

int ExecOt(int type);
u16 OtGetPrevKind();
void CrearOtMirrorWork();
void DeleteOtData(u32 type, u32 no);

#endif
