// game/esp3f.cpp: effect id 0x3F, not a visible effect but a vector buffer carved out of the esp
// pool. esp12/esp16 use it for their per-particle position and speed arrays.

#include "atari.h"
#include "light.h"
#include "esp.h"

#define ESP3F_BUF_MAX 0x12

// Vector buffer effect: a parent esp owns up to 0x12 child esps whose work areas hold Vec arrays.
struct Esp3fWork {
    u8 pad_0[2];
    u8 nWork;                     // 0x02 number of child buffers
    u8 nElem;                  // 0x03 vectors per child buffer
    cEsp* pBuf[ESP3F_BUF_MAX];   // 0x04
};

class cEsp3f : public cEsp {
public:
    Esp3fWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
    virtual void Destruct();
};

// Child buffer: its work area is the vector array.
class cEsp3fBuf : public cEsp {
public:
    Vec vec[1];  // 0xF8
};

// EspCreateTbl[0x3F] factory (used for both the parent and the child buffers).
cEsp* Esp3f_Create()
{
    return new cEsp3f;
}

// Nothing to update: the buffer's life is managed by its owner effect.
void cEsp3f::move()
{
}

// Parent (Rno0 == 0) release: pushes every still-live child buffer esp.
void cEsp3f::Destruct()
{
    if (m_Rno0 == 0) {
        Esp3fWork* w = &m_Free;
        u32 i;
        for (i = 0; i < w->nWork; i++) {
            cEsp* p = w->pBuf[i];
            if (p && (p->m_Be_flg & 1)) {
                PushEsp(p);
            }
        }
    }
}

// Allocates a buffer of `num` elements of `size` bytes: `per` elements fit one child (0x58 bytes),
// so num / per + 1 children are pulled (max 0x12, else an error). Returns 1 and the parent in
// *out; on any pool failure everything pulled so far is released and 0 returned.
int Esp3f_Alloc(u32 WorkSize, u32 Num, cEsp3f** ppEsp, EspInfo* pEff_core)
{
    cEsp* dmy = EspGetDmyPtr();
    u32 per = 0x58 / WorkSize;
    u32 n;
    cEsp* p;
    cEsp* c;
    cEsp3f* e;
    Esp3fWork* w;
    u32 i;

    *ppEsp = 0;
    n = Num / per + 1;
    if (n > ESP3F_BUF_MAX) {
        pLog->err(0, 0, "ESP_3F : Buf size over.[%d/%d]", n, ESP3F_BUF_MAX);
        return 0;
    }
    if (!PullEsp(&p, 0x3f)) {
        return 0;
    }
    e = (cEsp3f*)p;
    e->info = *pEff_core;
    w = &e->m_Free;
    w->nElem = per;
    w->nWork = n;
    for (i = 0; i < w->nWork; i++) {
        if (PullEsp(&c, 0x3f)) {
            c->m_Rno0 = 1;
            c->info = *pEff_core;
            w->pBuf[i] = c;
        } else {
            u32 j;
            for (j = 0; j < w->nWork; j++) {
                if (w->pBuf[j]) {
                    PushEsp(w->pBuf[j]);
                }
            }
            PushEsp(p);
            return 0;
        }
    }
    *ppEsp = (cEsp3f*)p;
    return 1;
}

// Address of element `no` (child no / per, slot no % per); NULL with an error when out of range.
Vec* Esp3f_GetVecPtr(cEsp3f* pEsp, u32 idx)
{
    Esp3fWork* w = &pEsp->m_Free;
    u32 per = w->nElem;
    u32 n = w->nWork;
    u32 buf = idx / per;
    Vec* ret;

    if (buf < n) {
        cEsp3fBuf* b = (cEsp3fBuf*)w->pBuf[buf];
        Vec* vp = b->vec;
        ret = &vp[idx % per];
        if ((int)ret < 0) {
            return ret;
        }
    }
    pLog->err(0, 0, "ESP_3F : access out of range.[%d/%d]", buf, n);
    return 0;
}

// No generator parameters (buffers are only created through Esp3f_Alloc).
int cEsp3f::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    return 1;
}
