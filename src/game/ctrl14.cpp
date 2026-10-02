// game/ctrl14.cpp: control 0x14, the dragon head statues of the stage 4 fire trap room. The em10
// ganados that operate them move and turn them, and their flame burns the player.

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "global.h"
#include "model.h"
#include "player.h"
#include "scroll.h"
#include "esp.h"
#include "snd.h"
#include "math_sub.h"
#include "est.h"

// Creates the control for dragon `type` from the room's scroll objects (base / head / jaws, ids
// 0xA.., 0xF.., 0x14..), gives it an effect Core_kind and three collision pieces from room
// collision file 5. NULL when the objects or a work are missing.
cCtrl* GetCtrlDragon(u32 type)
{
    cObj* pDragon[5];
    Vec pos;
    Vec rot;
    cCtrl* c;
    CTRL14_FREE* w;
    u32 no;

    switch (type) {
    case 0:
        pDragon[0] = SmdGetObjPtr(0xA);
        pDragon[1] = SmdGetObjPtr(0xB);
        pDragon[3] = SmdGetObjPtr(0xD);
        no = 0xE;
        break;
    case 1:
        pDragon[0] = SmdGetObjPtr(0xF);
        pDragon[1] = SmdGetObjPtr(0x10);
        pDragon[3] = SmdGetObjPtr(0x12);
        no = 0x13;
        break;
    case 2:
        pDragon[0] = SmdGetObjPtr(0x14);
        pDragon[1] = SmdGetObjPtr(0x15);
        pDragon[3] = SmdGetObjPtr(0x16);
        no = 0x17;
        break;
    default:
        goto create;
    }
    pDragon[4] = SmdGetObjPtr(no);
    if (pDragon[0] == NULL || pDragon[1] == NULL || pDragon[3] == NULL || pDragon[4] == NULL) {
        return NULL;
    }
    pDragon[0]->setMove(1);
    pDragon[1]->setMove(1);
    pDragon[3]->setMove(1);
    pDragon[4]->setMove(1);
create:
    c = CtrlMgr.createBack(0x14);
    if (c == NULL) {
        return NULL;
    }
    w = (CTRL14_FREE*) c->work;
    w->pDragon[0] = pDragon[0];
    w->pDragon[1] = pDragon[1];
    w->pDragon[3] = pDragon[3];
    w->pDragon[4] = pDragon[4];
    w->Type = type;
    w->EffKindId = EspPullCoreKind();
    pos = w->pDragon[1]->pos;
    rot = w->pDragon[1]->ang;
    if (pDragon[0]) {
        w->pEat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
    }
    w->pEat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    // pG: the original loads pG after the sat[1] store; a plain pG here is hoisted above the argument setup
    w->pEat[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
    return c;
}

// Per-frame: the flame wind-up / jet timers (est 1/2 jet start, 1/4 jet end, SEs), keeps the
// collision pieces on the head, and plays the start / stop movement SEs (per dragon) when the
// moving flag (bit1, set by addWidth / addHeight this frame) changes.
void cCtrl14::move()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    Vec pos;
    Vec rot;
    u8 flags;

    if (w->Fire_wait) {
        w->Fire_wait--;
        if (w->Fire_wait == 0) {
            EstSet(w->pDragon[1], -1, NULL, NULL, EFF_ROOM, 2, 1, w->EffKindId, w->pDragon[1], NULL);
            SndCall(6, 2, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
            w->Fire_timer = 60;
        }
    }
    if (w->Fire_timer) {
        w->Fire_timer--;
        if (w->Fire_timer == 0) {
            EffectEspgenDelete(0, w->EffKindId, w->pDragon[1]);
            EstSet(w->pDragon[1], -1, NULL, NULL, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, w->pDragon[1], NULL);
        }
    }
    pos = w->pDragon[1]->getPartsPtr(0)->world;
    rot = w->pDragon[1]->ang;
    if (w->pDragon[0]) {
        w->pEat[0]->setCoord(&pos, &rot);
    }
    w->pEat[1]->setCoord(&pos, &rot);
    w->pEat[2]->setCoord(&pos, &rot);
    flags = w->Chain_se_ck;
    if (flags & 1) {
        if (!(flags & 2)) {
            w->Chain_se_ck = flags & ~1;
            switch (w->Type) {
            case 0:
                SndCall(6, 8, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            case 1:
                SndCall(6, 0xA, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            case 2:
                SndCall(6, 0xC, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            }
        }
    } else {
        if (flags & 2) {
            w->Chain_se_ck = flags | 1;
            switch (w->Type) {
            case 0:
                SndCall(6, 7, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            case 1:
                SndCall(6, 9, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            case 2:
                SndCall(6, 0xB, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
                break;
            }
        }
    }
    w->Chain_se_ck &= ~2;
}

// Matrix of piece `idx` (0 base, 1 head, ...).
void cCtrl14::getBaseMtx(Mtx m, int type)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[type];

    if (o) {
        PSMTXCopy(o->mat, m);
    }
}

// The head's position.
void cCtrl14::getPos(Vec* pPos)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[1];

    if (o) {
        *pPos = o->pos;
    }
}

// The head's yaw.
f32 cCtrl14::getDir()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[1];

    if (o == NULL) {
        return 0.0f;
    }
    return o->ang.y;
}

// The head's yaw relative to the base (how far it is turned).
f32 cCtrl14::getDir2()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    f32 ret = 0.0f;

    if (w->pDragon[1] && w->pDragon[0]) {
        ret = Muku2(w->pDragon[0]->ang.y, w->pDragon[1]->ang.y, PI);
    } else {
        ret = 0.0f;
    }
    return ret;
}

// Slides the whole dragon by `x` along the base's local x, clamped to the rail limits of its
// type (type 2 does not slide); marks it moving.
void cCtrl14::addWidth(f32 add)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[0];
    Vec v;
    Vec p;

    if (o) {
        v.x = add;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVecSR(o->mat, &v, &v);
        PSVECAdd(&w->pDragon[0]->pos, &v, &p);
        switch (w->Type) {
        case 0:
            if (p.x > -6300.0f) {
                p.x = -6300.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 1:
            if (p.x > -32000.0f) {
                p.x = -32000.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 2:
            return;
        }
        PSVECSubtract(&p, &w->pDragon[0]->pos, &v);
        v.y = 0.0f;
        if (w->pDragon[0]) {
            PSVECAdd(&w->pDragon[0]->pos, &v, &w->pDragon[0]->pos);
        }
        if (w->pDragon[1]) {
            PSVECAdd(&w->pDragon[1]->pos, &v, &w->pDragon[1]->pos);
        }
        if (w->pDragon[3]) {
            PSVECAdd(&w->pDragon[3]->pos, &v, &w->pDragon[3]->pos);
        }
        if (w->pDragon[4]) {
            PSVECAdd(&w->pDragon[4]->pos, &v, &w->pDragon[4]->pos);
        }
    }
}

// Raises / lowers the whole dragon by `y` (upper limits per type); marks it moving.
void cCtrl14::addHeight(f32 add)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;

    if (w->Type == 2) {
        if (w->pDragon[0]->pos.y > 30000.0f && add > 0.0f) {
            return;
        }
    } else {
        if (w->pDragon[0]->pos.y > 5000.0f && add > 0.0f) {
            return;
        }
    }
    if (w->pDragon[0]) {
        w->pDragon[0]->pos.y += add;
    }
    if (w->pDragon[1]) {
        w->pDragon[1]->pos.y += add;
    }
    if (w->pDragon[3]) {
        w->pDragon[3]->pos.y += add;
    }
    if (w->pDragon[4]) {
        w->pDragon[4]->pos.y += add;
    }
    add = fabsf(add);
    if (add > 1.0f) {
        w->Chain_se_ck |= 2;
    }
}

// Turns the base by `add` radians; the head follows within +-45 degrees of the base.
void cCtrl14::addDir(f32 add)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[1];

    if (o) {
        o->ang.y += add;
        if (w->pDragon[0]) {
            w->pDragon[1]->ang.y = w->pDragon[0]->ang.y + Muku2(w->pDragon[0]->ang.y, w->pDragon[1]->ang.y, PI / 4.0f);
            w->pDragon[1]->ang.y = LIMIT_ANGLE(w->pDragon[1]->ang.y);
        }
    }
}

// Sets the head's yaw relative to the base.
void cCtrl14::setDir(f32 dir)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;

    if (w->pDragon[1] && w->pDragon[0]) {
        w->pDragon[1]->ang.y = w->pDragon[0]->ang.y + dir;
        w->pDragon[1]->ang.y = LIMIT_ANGLE(w->pDragon[1]->ang.y);
    }
}

// Eases the head back to the base's direction (0.35 degrees per frame).
void cCtrl14::resetDir()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;

    if (w->pDragon[1]) {
        w->pDragon[1]->ang.y += Muku2(w->pDragon[1]->ang.y, w->pDragon[0]->ang.y, 0.0061359233f);
        w->pDragon[1]->ang.y = LIMIT_ANGLE(w->pDragon[1]->ang.y);
    }
}

// Starts a flame: cancels a running jet, 30 frame wind-up est (1/3) with the SE, then the jet.
void cCtrl14::setFire()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[1];

    if (o) {
        if (w->Fire_timer) {
            w->Fire_timer = 0;
            EffectEspgenDelete(0, w->EffKindId, o);
            EstSet(w->pDragon[1], -1, NULL, NULL, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, w->pDragon[1], NULL);
        }
        w->Fire_wait = 30;
        EstSet(w->pDragon[1], -1, NULL, NULL, EFF_ROOM, 3, 1, w->EffKindId, w->pDragon[1], NULL);
        SndCall(6, 1, &w->pDragon[1]->pos, 0, 0, w->pDragon[1]);
    }
}

// 1 when the jet is burning and `p` lies in the flame box in head space (5000..15000 ahead,
// +-1500 wide, +-5000 high).
int cCtrl14::ckHitFire(Vec* pPos)
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    cModel* o = w->pDragon[1];
    Mtx inv;
    Vec v;

    if (o == NULL) {
        return 0;
    }
    if (w->Fire_timer == 0) {
        return 0;
    }
    PSMTXInverse(o->mat, inv);
    PSMTXMultVec(inv, pPos, &v);
    if (v.z < 5000.0f) {
        return 0;
    }
    if (v.z > 15000.0f) {
        return 0;
    }
    if (v.x < -1500.0f) {
        return 0;
    }
    if (v.x > 1500.0f) {
        return 0;
    }
    if (v.y < -5000.0f) {
        return 0;
    }
    if (v.y > 5000.0f) {
        return 0;
    }
    return 1;
}

// 1 when a wall (effect collision, attribute 0x400000) stands between the head and the player
// at chest height, so the flame does not reach him.
int cCtrl14::ckHitFireBlocked()
{
    CTRL14_FREE* w = (CTRL14_FREE*) work;
    Vec a;
    Vec b;

    if (w->pDragon[1]) {
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = -2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->pDragon[1]->mat, &a, &a);
        PSMTXMultVec(w->pDragon[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->pDragon[1]->mat, &a, &a);
        PSMTXMultVec(w->pDragon[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->pDragon[1]->mat, &a, &a);
        PSMTXMultVec(w->pDragon[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        return EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000) == 0;
    }
    return 0;
}
