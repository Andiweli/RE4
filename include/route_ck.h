#ifndef ROUTE_CK_H
#define ROUTE_CK_H

// game/route_ck.cpp: enemy routing over the room's "RTP" way-point graph (pG->pRoomRtp).

#include "types.h"
#include "vec.h"

class cEm;

// One way point (16 bytes).
struct RTP_POINT {
    Vec pos;       // 0x00
    u16 offLine;   // 0x0C  first entry in the link table
    u16 nLine;     // 0x0E  linked points
};

// Link table entry (4 bytes).
struct RTP_LINE {
    s16 connect;  // 0x00  way point index, -1 = none
    u16 dist;     // 0x02  distance / 10
};

// RTP file header; the tables are at byte offsets from the header (the file's own offsets, where
// PS2 holds them relocated to pointers).
struct RTP {
    u32 ID;              // 0x00  "2RTP" (0x32525450)
    u16 mapped;          // 0x04
    u16 nPoint;          // 0x06
    u16 nLine;           // 0x08
    u16 nNext;      // 0x0A  nPoint * nPoint (next table size)
    u32 pPoint;     // 0x0C  RTP_POINT[nPoint]
    u32 pLine;      // 0x10  RTP_LINE[nLine]
    u32 pNext;      // 0x14  s8 next[nPoint][nPoint]: next hop from row to column, -1 = unreachable
};

void RouteCk();
// Next position for `em` on its way to `target`; returns 1 when the target itself is reachable.
int RouteCkToEm(cEm* pMy, cEm* pTo, Vec* pDest, int mode);
// Position away from `from` along the nearest point's links.
void RouteCkEscEm(cEm* pMy, cEm* pTo, Vec* pDest);
int RouteCkToPos(cEm* pMy, Vec* pPos, Vec* pDest, int mode, f32* pMax);
int RouteCkPosToPos(Vec* pPos1, Vec* pPos2, Vec* pDest);
int RouteCkConnectPosCk(Vec* pPos1, Vec* pPos2);
f32 RouteCkPosToPosDis(Vec* pPos1, Vec* pPos2);
void RouteCkGetPoint(int no, Vec* out);
int RouteCkGetPointNumber();
f32 RouteCkGetDist(int n0, int n1);
int RouteCkGetNearPoint(Vec* pos);
// Nearest way point of `em`, cached in rckNear for the frame.
int getNearInfo(cEm* pEm, int mode, int flag);
// Nearest way point to `pos` that the position can reach (a != 0: nearest regardless), -1 = none.
s8 getNearPoint(Vec* pPos, int mode, int flag);
void Draw_rtp();
void Draw_eminfo();

#endif
