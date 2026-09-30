// game/ctrl12.cpp: control 0x12, shared room state for the enemy modules: 13 countdown timers,
// 6 counters, a per-frame flag word and the texture render targets (TexRenderMng) the em2b /
// em2c / em32 bosses draw their special textures with.

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "TexRender.h"
#include "esp.h"

// Per-frame: counts the timers down and clears the frame flags.
void cCtrl12::move()
{
    CTRL12_FREE* w = (CTRL12_FREE*) work;
    int i;
    u32 j;

    for (i = 0; i < 13; i++) {
        if (w->Timer[i] != 0) {
            w->Timer[i]--;
        }
    }
    for (j = 0; j < 1; j++) {
        w->ClrCnt[j] = 0;
    }
}

// The room's single ctrl12 (created at the back of the pool on first use); NULL when full.
cCtrl* GetCtrlCtrl12()
{
    cCtrl* c;
    u32 i;
    u32 n = CtrlMgr.getArrayNum();

    for (i = 0; i < n; i++) {
        c = CtrlMgr.at(i);
        if (c->isAlive() && c->Id == 0x12) {
            return c;
        }
    }
    c = CtrlMgr.createBack(0x12);
    if (c == 0) {
        return 0;
    }
    return c;
}

// Sets timer `idx` (0..12) to `val` frames.
void Ctrl12Set(cCtrl* pCtrl, int idx, s16 val)
{
    CTRL12_FREE* w;

    if (pCtrl == 0) {
        return;
    }
    if (pCtrl->Id != 0x12) {
        return;
    }
    if (idx > 12) {
        return;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    w->Timer[idx] = val;
}

// 1 while timer `idx` is running.
int Ctrl12Ck(cCtrl* pCtrl, int id)
{
    CTRL12_FREE* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    if (id > 12) {
        return 0;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    if (w->Timer[id] != 0) {
        return 1;
    }
    return 0;
}

// Adds `add` to counter `idx` (0..5), saturating at 0xFFFF.
void Ctrl12CntAdd(cCtrl* pCtrl, int id, int add)
{
    CTRL12_FREE* w;
    u16 v;

    if (pCtrl == 0) {
        return;
    }
    if (pCtrl->Id != 0x12) {
        return;
    }
    if (id > 5) {
        return;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    v = w->Cnt[id];
    w->Cnt[id] = v + add;
}

// 1 when counter `idx` has reached `val`.
int Ctrl12CntCk(cCtrl* pCtrl, int id, u16 over)
{
    CTRL12_FREE* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    if (id > 5) {
        return 0;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    return w->Cnt[id] >= over;
}

// The em2b (El Gigante) texture render target, allocated on first use together with its est
// (owner 1, est 0x42) that renders into it.
TexRenderMng* Ctrl12GetTexRenderEm2b(cCtrl* pCtrl)
{
    CTRL12_FREE* w;
    TexRenderMng* t;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    t = w->pMgrEm2b;
    if (t == 0) {
        GetTexRenderMgr(&w->pMgrEm2b);
        if (w->pMgrEm2b != 0) {
            EstSet(0, -1, 0, 0, EFF_ROOM, 0x42, w->pMgrEm2b->GetCoreFlg() | 1, ESP_CORE_KIND_NONE, t, 0);
        }
    }
    return w->pMgrEm2b;
}

// The em2c texture render target, allocated on first use.
TexRenderMng* Ctrl12GetTexRenderEm2c(cCtrl* pCtrl)
{
    CTRL12_FREE* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    if (w->pMgrEm2c == 0) {
        GetTexRenderMgr(&w->pMgrEm2c);
    }
    return w->pMgrEm2c;
}

// The em32 (U3) texture render target, allocated on first use.
TexRenderMng* Ctrl12GetTexRenderEm32(cCtrl* pCtrl)
{
    CTRL12_FREE* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (CTRL12_FREE*) pCtrl->work;
    if (w->pMgrEm32 == 0) {
        GetTexRenderMgr(&w->pMgrEm32);
    }
    return w->pMgrEm32;
}
