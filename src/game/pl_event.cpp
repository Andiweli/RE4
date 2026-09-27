// game/pl_event: the player's event routine (Pl_R0_Event), in which the scenario / event system
// controls the player. Entered and left through cPlayer::beginAction / endEvent0 (pl_class).

#include "player.h"
#include "atari.h"
#include "math_sub.h"

// Routine 0 == 5 (event control): the scenario / event drives the player; r_no_1 picks Normal
// (play a motion), ToWalk (walk to m_VecWork0), Smooth (motion change with footwork).
void Pl_R0_Event(cPlayer* pEm)
{
    static void (*funcTbl[])(cPlayer*) = {
        pl_R1_Event_Normal,
        pl_R1_Event_ToWalk,
        pl_R1_Event_Smooth,
    };

    funcTbl[pEm->r_no_1](pEm);
}

// Event sub-routine 0: plays the set motion; when it ends and m_Flag 0x100 (return when done) is
// set, back to routine 0/0 (normal control).
void pl_R1_Event_Normal(cPlayer* pEm)
{
    if (pEm->r_no_2 == 0) {
        pEm->r_no_2 = 1;
    }
    if (pEm->motionMove()) {
        if (pEm->m_Flag & 0x100) {
            pEm->m_Flag &= ~0x100;
            pEm->r_no_0 = 0;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
    }
}

// Event sub-routine 1: turns toward m_VecWork0 (turn motion when more than 60 degrees off, m_Fwork0
// = turn speed per frame), walks (m_MotTbl[2]) until within 100 units, then stands (m_MotTbl[0])
// and sets m_Work0 = 1 for the event script to see.
void pl_R1_Event_ToWalk(cPlayer* pEm)
{
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        ang = Muku(&pEm->pos, &pEm->m_VecWork0, pEm->ang.y, PI * 2.0f);
        if (fabsf(ang) > PI / 3.0f) {
            pEm->motionSet(pEm->m_MotTbl[2], 5, 0, 4, 0);
            pEm->r_no_2 = 1;
            break;
        }
        goto set_walk;
    case 1:
        ang = Muku(&pEm->pos, &pEm->m_VecWork0, pEm->ang.y, pEm->m_Fwork0);
        pEm->ang.y += ang;
        if (fabsf(ang) < pEm->m_Fwork0 * 0.5f) {
        set_walk:
            pEm->motionSet(pEm->m_MotTbl[2], 5, 0, 5, 0);
            pEm->r_no_2 = 2;
        }
        break;
    case 2:
        ang = Muku(&pEm->pos, &pEm->m_VecWork0, pEm->ang.y, pEm->m_Fwork0);
        pEm->ang.y += ang;
        if (GetDistance(&pEm->pos, &pEm->m_VecWork0) < 10000.0f) {
            pEm->motionSet(pEm->m_MotTbl[0], 5, 0, 1, 0);
            pEm->m_Work0 = 1;
            pEm->r_no_2 = 3;
        }
        break;
    }
    pEm->motionMove();
}

// Event sub-routine 2: lets the current motion end, then re-plants the feet (setFootwork) and
// keeps playing.
void pl_R1_Event_Smooth(cPlayer* pEm)
{
    switch (pEm->r_no_2) {
    case 0:
        if (pEm->motionMove()) {
            pEm->r_no_2 = 2;
        }
        break;
    case 2:
        pEm->setFootwork();
        pEm->motionMove();
        pEm->r_no_2 = 3;
    case 3:
        pEm->motionMove();
        break;
    }
}
