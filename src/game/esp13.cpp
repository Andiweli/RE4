// game/esp13.cpp: effect id 0x13, a retired id. The class still exists so the create table has an
// entry, but SetFreeWork refuses every spawn with an error.

#include "atari.h"
#include "esp.h"

class cEsp13 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x13] factory.
cEsp* Esp13_Create()
{
    return new cEsp13;
}

// Standard sprite update (never reached in practice: SetFreeWork rejects the spawn).
void cEsp13::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Always fails ("Invalid ID 'ESP13'"), so EspSeqSet releases the effect straight away.
int cEsp13::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    pLog->err(0, 0, "ESP : Invalid ID 'ESP13' ");
    return 0;
}
