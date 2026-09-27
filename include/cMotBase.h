#ifndef CMOTBASE_H
#define CMOTBASE_H

#include "types.h"
#include "vec.h"
#include "motion.h"

// Motion base follower (game/cMotBase.cpp, 0x38 bytes, cEm+0x7A4): moves a model by its
// motion's root speed from a base pose and eases it back onto the model over `cnt` frames.
class cMotBase {
private:
    cMotModel* pMod;  // 0x00
    Vec pos;            // 0x04  followed position
    Vec ang;            // 0x10  followed rotation
    Vec pos_old;        // 0x1C  model position at the last move
    Vec ang_old;        // 0x28  model rotation at the last move
public:
    u8 cnt;             // 0x34  frames left to ease back (0 = follow forever, 0xFF = off)
    u8 pad_35[3];

    cMotBase();
    void set(cMotModel* pMod0, Vec* pos, Vec* rot, u8 cnt);
    void set(cMotModel* m, MotionData* data, Vec* pPos, Vec* rot, u8 cnt);
    void adjust();
    void move();
};

#endif
