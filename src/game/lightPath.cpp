// game/lightPath: light brightness paths (D:/Bio4/Prog/lightPath.cpp). The core archive's light
// path block (cLightPathHeader: offsets to byte strings) drives light type 5: each byte is a
// brightness 0..200, 0xFF ends the path; cLightPath walks one string, looping.
#include "lightPath.h"
#include "db_log.h"
#include "main_mem.h"

// Total byte size of the path block (header, offset table and the last string up to its 0xFF).
u32 cLightPathHeader::getSize()
{
    u8* p;

    if (nPath == 0) {
        return 4;
    }
    p = (u8*) this + (*(u32*) ((u8*) this + nPath * 4) + 4); // TODO: should last `4` be sizeof(cLightPathHeader)?
    while (*p != 0xFF) {
        p++;
    }
    p++;
    return (u32) p - (u32) this;
}

// Path string `no` (0 with an error log when out of range).
cLightPathData* cLightPathHeader::getPathData(u32 idx)
{
    if (idx >= nPath) {
        pLog->err(0, 0, "cLightPathHeader::getPathData() IDX OVER %d", idx);
        return 0;
    }
    return (cLightPathData*) ((u8*) this + *(u32*) ((u8*) this + sizeof(cLightPathHeader) + idx * 4));
}

// Length of the string including the 0xFF terminator.
u32 cLightPathData::getSize()
{
    u8* p = data;
    u32 n = 1;

    while (*p != 0xFF) {
        p++;
        n++;
    }
    return n;
}

// Starts walking `data`; `no` is the flag byte (bit 1 = inverted brightness 200 - v).
int cLightPath::setPath(u8* pPath, u8 flag)
{
    if (!VALID_PTR(pPath)) { pLog->err(0, 0, "setPath() INVALID PTR %08X", pPath); return 0; }
    pCur = pStart = pPath;
    Flag = flag;
    return 1;
}
// Returns the current brightness (0..200, inverted with Flag bit 1) and advances; wraps to the
// start at 0xFF.
int cLightPath::movePath()
{
    u8 v;
    if (!VALID_PTR(pStart) || !VALID_PTR(pCur)) { pLog->err(2, 0, "Light05() INVALID PATH DATA"); return 0; }
    v = pCur[0];
    if (v <= 200) {
        if (Flag & 2) v = 200 - v;
        pCur = pCur + 1;
    } else if ((Flag & 1) == 0) {
        pCur = pStart;
        v = pCur[0];
        if (Flag & 2) v = 200 - v;
        // COMPILER-DIFF: tie. The loop notes double this store's `this` ref weight (9 refs > pCur's
        // 5/14 priority), which puts `this` in r9 and pCur in r11 like the original; no code changes.
        do { pCur = pCur + 1; } while (0);
    }
    return v;
}
