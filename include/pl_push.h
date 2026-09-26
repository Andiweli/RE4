#ifndef PL_PUSH_H
#define PL_PUSH_H

#include "types.h"
#include "vec.h"
#include "player.h"
#include "em.h"

// Player push-object control (game/pl_push.cpp): catching a pushable enemy (id 0x45) and
// pushing it while checking the scenario. Partial layout (0x10 bytes known).
class cPlPush {
private:
    cPlayer* m_pEm;     // 0x00  PS2 cEm*
    cEmRack* m_Target;   // 0x04  object being pushed (NULL = none)
    u8 m_Ctr;           // 0x08  cleared when a target is caught
    u8 m_Flag;          // 0x09  pushTargetInit flag (bit0: getWHY uses `dir`, else `dirSub`)
    u8 m_Dummy02;       // 0x0A
    u8 m_Dummy03;       // 0x0B
    int m_Dir;            // 0x0C  side of the object the player stands on (0..3, 4 = none); read as u8 too (lbz 0xF)

    void getWHY(f32* w, f32* h, f32* dy);

public:
    cPlPush(cPlayer* pl) {
        m_pEm = pl;
        m_Ctr = 0;
        m_Target = 0;
    }
    int catchCheck();
    void pushTargetInit(u8 flag);
    int pushTarget();
    void stopTarget();
    int scrHitCheck();
    int scrHitCheckSub(f32 w, f32 h, f32 dy, Vec* pos, f32 sign);
    int emSandCheck(f32 w, f32 h, f32 dy, Vec* pos);
    int plAdjust();
    // 1 when the push target is gone or dead.
    int isBroken() {
        if (m_Target == 0 || m_Target->hp <= 0) {
            return 1;
        }
        return 0;
    }
};

#endif
