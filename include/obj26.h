#ifndef OBJ26_H
#define OBJ26_H

#include "types.h"
#include "vec.h"
#include "obj.h"

class cCtrl;
class cEm;

// Fading attachment work (game/obj26.cpp): scales toward `tgtScale`, then shrinks and fades out.
// Be_flag, Timer, Se_wait, Seid, Lost_wait and pCtrlGroup are PS2 fields this file never reads or
// writes; PS2's Vec is 16 bytes (aligned for its vector unit) so Scale sits at 0x20 there, but GC's
// 12-byte Vec needs no such padding and packs straight after Lost_wait.
struct FREE_OBJ26 {
    u32 Be_flag;       // 0x00
    s32 Timer;         // 0x04
    cEm* pEm;          // 0x08  followed parts 2 of this object
    s32 Se_wait;       // 0x0C
    u32 Seid;          // 0x10
    s32 Lost_wait;     // 0x14
    Vec Scale;         // 0x18
    cCtrl* pCtrlGroup; // 0x24
};

// Attachment that follows parts 2 of its parent, scales toward a target size (routine 0) and
// then shrinks/fades away (routine 1). Routine index in xFD, step in xFE.
class cObj26 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  FREE_OBJ26

    virtual void move();
};

#define OBJ26_WK(o) ((FREE_OBJ26*) (o)->free)

#endif
