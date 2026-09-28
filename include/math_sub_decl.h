#ifndef MATH_SUB_DECL_H
#define MATH_SUB_DECL_H

// Declarations of game/math_sub.cpp and game/sub2.cpp and the angle constants, without the inline
// definitions of math_sub.h (fabsf as a volatile asm, getColumn). For units that read <math.h> with
// its own fabsf.

#include "types.h"
#include "vec.h"
#include "db_log.h"

#define PI 3.1415927f
#define PI2 6.2831855f       // 2 * PI
#define DEG2RAD 0.017453292f // PI / 180
#define DEG(d) ((d) * DEG2RAD)
// `x` limited to [lo, hi] (an expression; the arguments are evaluated more than once).
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

// game/math_sub.cpp
void RotMatrix(Mtx m, Vec* vec);
void TransMatrix(Mtx m, Vec* pos);
void ScaleMatrix(Mtx m, Vec* scale);

// game/math_sub.cpp
void SetOrientationZX(Vec* z, Vec* x, Mtx m);
void SetOrientationZY(Vec* z, Vec* y, Mtx m);
void low_RotMatrix(Mtx m, Vec* vec);
void RotMatrixZXY(Mtx m, Vec* vec);
void Matrix2AxisAngle(Mtx m, Vec* ang);
void VecRadLimit(Vec* v);
f32 VecElevation(Vec* v);
void MtxRotAxisPosRad(Mtx m, Vec* axis, Vec* pos, f32 rad);
void VecLinearCombination(Vec* a, f32 c0, Vec* b, f32 c1, Vec* vec);
void VecInternalDivisionAngle(Vec* a, f32 m, Vec* b, f32 n, Vec* vec);
void VecLinearDecomposition(Vec* v, Vec* vec1, Vec* vec2, f32* alpha1, f32* alpha2);
f32 hermite(f32* x, f32* v, f32 t);
f32** malloc_2dim_array_f32(int n, int m);
void free_2dim_array_f32(int n, int m, f32** A);
int de_Boor_Cox(int n, f32* p, f32 t, int order, f32* B);
f32 MtxNNLUDecomposition(int n, f32* A, int* ip);
f32 MtxNNInverse(int n, f32* m, f32* m_inv);
void MtxNNMultVecSR(int n, int m, f32* mat, f32* v, f32* v_dst);
void OrthographicProjection(Vec* src_pos, Vec* dst_pos, Vec* projection_dir, Vec* plane_pos, Vec* plane_norm);
f32 IPOW(f32 x, int y);
f32 SQRTF(f32 x);
// Distance between the points a and b (pointers): x, y, z, or x, z on the ground plane. Macros: an inline here
// would renumber the declarations of every unit that includes this header.
#define VEC_DIST(a, b) SQRTF(((a)->x - (b)->x) * ((a)->x - (b)->x) + ((a)->y - (b)->y) * ((a)->y - (b)->y) + ((a)->z - (b)->z) * ((a)->z - (b)->z))
#define VEC_DISTXZ(a, b) SQRTF(((a)->x - (b)->x) * ((a)->x - (b)->x) + ((a)->z - (b)->z) * ((a)->z - (b)->z))
f32 SINF(f32 x);
f32 COSF(f32 x);
f32 LIMIT_ANGLE(f32 x);
f32 VecAngle(Vec* vec_a, Vec* vec_b);
// game/sub2.cpp
f32 RootSumSquare3(Vec* v);
int GetScreenPos(Vec* pos, Vec* scr);
f32 GetDistance(Vec* v0, Vec* v1);      // squared distance
f32 GetDistance3(Vec* v0, Vec* v1);     // distance
f32 GetDistanceXZ(Vec* v0, Vec* v1);    // squared distance in the XZ plane
void RotVector(Vec* vec0, Vec* ang, Vec* vec_ans);
// Angle step from `ang` towards `target` seen from `pos`, clamped to +-limit.
f32 Muku(Vec* v0, Vec* v1, f32 dir, f32 dy);
// Step from `ang` towards `target`, at most +-limit.
f32 Muku2(f32 src_dir, f32 dst_dir, f32 add);
// Muku2 towards the XZ direction of `dir`.
f32 Muku3(f32 src_dir, Vec* v0, f32 dy);
// out = a + (b - a) * t
void PosToPos(Vec* pos1, Vec* pos2, Vec* pos3, f32 per);
f32 GetXZAngle(Vec* v0, Vec* v1);   // atan2 of to - from in the XZ plane, limited to +-PI
f32 GetXYAngle(Vec* v0, Vec* v1);
f32 GetXZAngleLocal(Vec* v0, Vec* v1, f32 v0_dir);   // GetXZAngle relative to `ang`
// Point `p` inside the XZ quad `quad[4]` (0-1-2-3 order)?
int HitCheckPoint4(Vec* pos, Vec* xz);
// pos += speed rotated by the model's rot
void AddSpeed(struct cModel* pEm, const Vec* speed);
// dst[8] = rotate(src[8], rot) + pos
void BoxWorldCalc(Vec* BoxSrc, Vec* BoxDst, Vec* pos, Vec* ang);
// World point under screen position (sx, sy): the floor hit when y == 1e8f, else at height y.
void Get3DPosFrom2D(Vec* pPos3d, f32 sx, f32 sy, f32 h);
// Rotate `v` (x, -y on the ground plane) into the camera's heading.
void VecToCamVec(Vec* v1, Vec* v2);
// Segment a-b against the sphere (c, r): 1 with the entry point in `out` (a itself when a is inside).
int LineSphereCrossCk(Vec* a, Vec* b, Vec* c, f32 r, Vec* pCross);
int SphereHitCk(Vec* pPos1, Vec* pPos2, f32 radius1, f32 radius2);
// Launch vector for a parabola from `from` to `to` peaking `h` above the higher end (gravity 20).
void CalcParabolaVector(Vec* spd, Vec* src, Vec* dst, f32 height);
f32 CalcStopDist(f32 v0, f32 a);
// Move `pos` `dist` towards `target`; 1 when it arrived.
int CalcMovePosDist(Vec* pPos, Vec* pTar, f32 dist);

// game/sub2.cpp
f32 GetDistance(Vec& v0, Vec& v1);
class cModel;
int Front_check(cModel* a, cModel* b, f32 ang);   // b within +-ang of a's heading
int Front_check(cModel* a, Vec* b, f32 ang);
int Front_check(Vec* a, f32 rot, Vec* b, f32 ang);

// Debug-checked normalize: zero vectors are reported with the caller's file/line.
#define VECNormalize(src, dst)                                                          \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                    \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

#endif
