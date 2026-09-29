#ifndef PATH_H
#define PATH_H

// game/path.cpp: paths (poly-lines with per-vertex parts weights, moved along by distance) and
// B-spline "FuncPath" curves (id_sys). PathHasWeight/PathGetLength/PathGetPos/PathGetPosEm are
// declared in esp.h (void* path).

#include "types.h"
#include "vec.h"

class cModel;

// One path vertex (0x28 bytes).
struct PATH_UNIT {
    Vec pos;             // 0x00
    Vec normal;          // 0x0C  up vector (PathGetMatEm interpolates it for the matrix)
    f32 Length;          // 0x18  distance from the path start
    u8 weight_id[3];     // 0x1C  model parts the vertex follows (PathGetVtxMat)
    u8 weight_num;       // 0x1F  entries in weight_id/weight_rate, 0 = fixed vertex
    u8 weight_rate[3];   // 0x20  percent
    u8 pad_23[5];
};

struct Path {
    u16 num;            // 0x00
    u8 pad_2[2];
    PATH_UNIT vtx[1];   // 0x04  num entries
};

// B-spline control points (id_sys path0).
struct FuncPathData {
    s8 k;         // 0x00  spline order
    u8 pad_1[6];
    s8 n;         // 0x07  control point count
    Vec pos[1];   // 0x08
};

// Parametrised B-spline (id_sys path1).
struct FuncPathWork {
    int k;        // 0x00
    int n;        // 0x04
    Vec alpha[1]; // 0x08  n coefficients
};

int PathGetMatEm(void* pPdat, cModel* pMod, f32 dist, u16* pPntNo, Mtx pMat);
void PathGetVtxMat(Mtx pMat, cModel* pMod, PATH_UNIT* pPunit);
int FuncPathParametrize(void* pPath, void* pB);
int FuncPathCalc(void* pPath, void* pB, f32 t, Vec* p);
void FuncPathClear(void* pPath);

#endif
