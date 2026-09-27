// game/esp44.cpp: effect id 0x44, room sound trigger. Spawning it plays room SE number Work8[0]
// at the effect position through the room's SE callback; the effect itself lives zero frames.

#include "atari.h"
#include "light.h"
#include "esp.h"

// Room SE trigger effect: plays the generator's sound once, draws nothing.
class cEsp44 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x44] factory.
cEsp* Esp44_Create()
{
    return new cEsp44;
}

// Nothing to update: the effect is released by SetFreeWork returning 0.
void cEsp44::move()
{
}

// Plays room SE Work8[0] at m_Pos (skipped while the generator loop pre-runs so the sound is not
// replayed) and returns 0 so the sprite is released immediately.
int cEsp44::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    if (EspGenGetMoveLoop() == 0) {
        EffCallRoomSeFunc((s8)pSeq->Work8[0], &m_Pos);
    }
    return 0;
}
