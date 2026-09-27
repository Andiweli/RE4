// game/math_sub: vector/matrix helpers shared by the game code (D:/Bio4/Prog/math_sub.cpp). The
// rotation convention is RotMatrix = Rz * Ry * Rx, so X is applied first.
#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "main_mem.h"
#include "math_sub.h"
#include <stdio.h>
#include <string.h>


// v * s into a static (unused).
// Never called in this build: only their static results survive (.bss).
static inline Vec* VecScaled(Vec* v, f32 s)
{
    static Vec ans;
    PSVECScale(v, &ans, s);
    return &ans;
}

// a + b into a static (unused).
static inline Vec* VecSum(Vec* a, Vec* b)
{
    static Vec ans;
    PSVECAdd(a, b, &ans);
    return &ans;
}

// Build a rotation matrix whose Z axis is `z` and whose X axis is `x` (orthonormalised).
#line 15 "D:/Bio4/Prog/math_sub.cpp"
void SetOrientationZX(Vec* z, Vec* x, Mtx m)
{
    Vec vx;
    Vec vy;
    Vec vz;

    VECNormalize(x, &vx);
    VECNormalize(z, &vz);

    PSVECCrossProduct(&vz, &vx, &vy);
    VECNormalize(&vy, &vy);
    PSVECCrossProduct(&vy, &vz, &vx);
    VECNormalize(&vx, &vx);

    PSMTXIdentity(m);
    m[0][0] = vx.x;
    m[1][0] = vx.y;
    m[2][0] = vx.z;
    m[0][1] = vy.x;
    m[1][1] = vy.y;
    m[2][1] = vy.z;
    m[0][2] = vz.x;
    m[1][2] = vz.y;
    m[2][2] = vz.z;
}

// Rotation matrix whose Z axis is z and Y axis is y (orthonormalised).
void SetOrientationZY(Vec* z, Vec* y, Mtx m)
{
    Vec vx;
    Vec vy;
    Vec vz;

#line 46 "D:/Bio4/Prog/math_sub.cpp"
    VECNormalize(y, &vy);
    VECNormalize(z, &vz);

    PSVECCrossProduct(&vy, &vz, &vx);
    VECNormalize(&vx, &vx);
    PSVECCrossProduct(&vz, &vx, &vy);
    VECNormalize(&vy, &vy);

    PSMTXIdentity(m);
    m[0][0] = vx.x;
    m[1][0] = vx.y;
    m[2][0] = vx.z;
    m[0][1] = vy.x;
    m[1][1] = vy.y;
    m[2][1] = vy.z;
    m[0][2] = vz.x;
    m[1][2] = vz.y;
    m[2][2] = vz.z;
}

#define MTX_COL(m, c, v)   \
    (v).x = (m)[0][c];     \
    (v).y = (m)[1][c];     \
    (v).z = (m)[2][c]


// Rotation matrix -> Euler angles (radians) in the RotMatrix convention: x and y from the Z column,
// then z from the residual rotation.
void Matrix2AxisAngle(Mtx m, Vec* ang)
{
    Vec v0;
    Vec v1;
    Vec v2;
    Vec v3;
    Mtx r;
    Mtx inv;
    Mtx t;

    PSMTXTranspose(m, t);
    MTX_COL(r, 0, v3);
    getColumn(t, 0, &v0);
    MTX_COL(t, 1, v1);
    MTX_COL(t, 2, v2);
    ang->x = ang->y = ang->z = 0.0f;

    ang->x = atan2f(-v2.y, v2.z);
    if (v2.x > 1.0f) {
        ang->y = asinf(1.0f);
    } else if (v2.x < -1.0f) {
        ang->y = asinf(-1.0f);
    } else {
        ang->y = asinf(v2.x);
    }
    ang->x = -ang->x;
    ang->y = -ang->y;
    RotMatrix(r, ang);
    PSMTXInverse(r, inv);
    PSMTXConcat(m, inv, r);
    MTX_COL(r, 0, v3);
    ang->z = atan2f(v3.y, v3.x);
}

// Wraps each component into [-PI, PI).
void VecRadLimit(Vec* v)
{
    f32* p = (f32*) v;
    int i;

    for (i = 0; i < 3; i++) {
        while (p[i] >= PI) {
            p[i] -= PI2;
        }
        while (p[i] < -PI) {
            p[i] += PI2;
        }
    }
}

// Angle between two vectors in radians (0 when either is zero length).
f32 VecAngle(Vec* vec_a, Vec* vec_b)
{
    f32 d = PSVECDotProduct(vec_a, vec_b);
    f32 l = PSVECMag(vec_a);

    l *= PSVECMag(vec_b);
    d /= l;
    return acosf(d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d));
}

// Elevation angle of v above the XZ plane (radians).
f32 VecElevation(Vec* v)
{
    return atan2f(v->y, SQRTF(v->x * v->x + v->z * v->z));
}

// Rotation of `rad` radians about `axis` through the point `pos`.
void MtxRotAxisPosRad(Mtx m, Vec* axis, Vec* pos, f32 rad)
{
    Vec p;
    Vec d;
    Mtx t1;
    Mtx t2;
    Mtx t3;
    Mtx t4;
    Mtx t5;

    PSMTXIdentity(t4);
    PSMTXRotAxisRad(m, axis, rad);
    PSMTXMultVecSR(m, pos, &p);
    PSVECSubtract(pos, &p, &d);
    m[0][3] += d.x;
    m[1][3] += d.y;
    m[2][3] += d.z;

    PSMTXTrans(t1, -pos->x, -pos->y, -pos->z);
    PSMTXRotAxisRad(t2, axis, rad);
    PSMTXTrans(t3, pos->x, pos->y, pos->z);
    PSMTXConcat(t2, t1, m);
    PSMTXConcat(t3, m, m);
}

// out = s * a + t * b.
void VecLinearCombination(Vec* a, f32 c0, Vec* b, f32 c1, Vec* vec)
{
    Vec ta;
    Vec tb;

    PSVECScale(a, &ta, c0);
    PSVECScale(b, &tb, c1);
    PSVECAdd(&ta, &tb, vec);
}

// Interpolate the direction of `a` towards `b` by the ratio s : t.
void VecInternalDivisionAngle(Vec* a, f32 m, Vec* b, f32 n, Vec* vec)
{
    Mtx r;
    Vec axis;
    f32 ang = VecAngle(a, b);

    if (ang == 0.0f) {
#line 342 "D:/Bio4/Prog/math_sub.cpp"
        VECNormalize(a, vec);
    } else if (ang == PI) {
    } else {
        PSVECCrossProduct(a, b, &axis);
        PSMTXRotAxisRad(r, &axis, m / (n + m) * ang);
        PSMTXMultVecSR(r, a, vec);
#line 354 "D:/Bio4/Prog/math_sub.cpp"
        VECNormalize(vec, vec);
    }
}

// Decompose `v` on the plane spanned by `a` and `b`: v = s * a + t * b.
void VecLinearDecomposition(Vec* v, Vec* vec1, Vec* vec2, f32* alpha1, f32* alpha2)
{
    Vec n;
    Vec m;
    Vec ab;
    Vec va;
    f32 k;
    f32 l;

    PSVECCrossProduct(vec1, vec2, &n);
    PSVECCrossProduct(v, &n, &m);
    PSVECSubtract(vec2, vec1, &ab);
    PSVECSubtract(v, vec1, &va);
    k = PSVECDotProduct(&m, &va);
    k /= PSVECDotProduct(&m, &ab);
    PSVECScale(&ab, &va, k);
    PSVECAdd(vec1, &va, &va);
    l = PSVECMag(v);
    l /= PSVECMag(&va);
    *alpha1 = l * (1.0f - k);
    *alpha2 = l * k;
}

// Scales the three columns of m by s.
void ScaleMatrix(Mtx m, Vec* vec)
{
    m[0][0] *= vec->x;
    m[0][1] *= vec->y;
    m[0][2] *= vec->z;
    m[1][0] *= vec->x;
    m[1][1] *= vec->y;
    m[1][2] *= vec->z;
    m[2][0] *= vec->x;
    m[2][1] *= vec->y;
    m[2][2] *= vec->z;
}

// Sets the translation column of m.
void TransMatrix(Mtx m, Vec* pos)
{
    m[0][3] = pos->x;
    m[1][3] = pos->y;
    m[2][3] = pos->z;
}

// The game's Euler rotation matrix (radians): m = Rz(rot.z) * Ry(rot.y) * Rx(rot.x), i.e. a
// vector is rotated about X first, then Y, then Z; translation cleared. Used for every model angle.
void RotMatrix(Mtx m, Vec* vec)
{
    f32 sx;
    f32 sy;
    f32 sz;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 szcx;
    f32 czsx;
    f32 szsx;
    f32 czcx;

    sx = sinf(vec->x);
    sy = sinf(vec->y);
    sz = sinf(vec->z);
    cx = cosf(vec->x);
    cy = cosf(vec->y);
    cz = cosf(vec->z);
    szcx = sz * cx;
    szsx = sz * sx;
    czsx = cz * sx;
    czcx = cz * cx;

    m[0][0] = cz * cy;
    m[0][1] = czsx * sy - szcx;
    m[0][2] = czcx * sy + szsx;
    m[0][3] = 0.0f;
    m[1][0] = sz * cy;
    m[1][1] = szsx * sy + czcx;
    m[1][2] = szcx * sy - czsx;
    m[1][3] = 0.0f;
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
    m[2][3] = 0.0f;
}

// RotMatrix using the game's fast SINF/COSF (zero angles short-cut); same matrix. Used by the
// effect and parts code.
void low_RotMatrix(Mtx m, Vec* vec)
{
    f32 sx;
    f32 cx;
    f32 sy;
    f32 cy;
    f32 sz;
    f32 cz;
    f32 szsx;
    f32 czcx;
    f32 szcx;
    f32 czsx;

    if (vec->x == 0.0f) {
        sx = 0.0f;
        cx = 1.0f;
    } else {
        sx = SINF(vec->x);
        cx = COSF(vec->x);
    }
    if (vec->y == 0.0f) {
        sy = 0.0f;
        cy = 1.0f;
    } else {
        sy = SINF(vec->y);
        cy = COSF(vec->y);
    }
    if (vec->z == 0.0f) {
        sz = 0.0f;
        cz = 1.0f;
    } else {
        sz = SINF(vec->z);
        cz = COSF(vec->z);
    }
    szsx = sz * sx;
    czcx = cz * cx;
    szcx = sz * cx;
    czsx = cz * sx;

    m[0][0] = cz * cy;
    m[0][1] = czsx * sy - szcx;
    m[0][2] = czcx * sy + szsx;
    m[0][3] = 0.0f;
    m[1][0] = sz * cy;
    m[1][1] = szsx * sy + czcx;
    m[1][2] = szcx * sy - czsx;
    m[1][3] = 0.0f;
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
    m[2][3] = 0.0f;
}

// m = Ry * Rx * Rz through PSMTXRotRad: a vector is rotated about Z first, then X, then Y (the
// effect speed spread uses it).
void RotMatrixZXY(Mtx m, Vec* vec)
{
    Mtx t;

    PSMTXRotRad(m, 'y', vec->y);
    PSMTXRotRad(t, 'x', vec->x);
    PSMTXConcat(m, t, m);
    PSMTXRotRad(t, 'z', vec->z);
    PSMTXConcat(m, t, m);
}

// Cubic Hermite interpolation of p[0]..p[1] with tangents v[0]..v[1].
f32 hermite(f32* x, f32* v, f32 t)
{
    f32 t2 = t * t;
    f32 t3 = t * t2;
    f32 h01 = -(t3 + t3) + 3.0f * t2;
    f32 h11 = t3 - t2;
    f32 h10 = h11 - t2 + t;
    f32 h00 = -h01 + 1.0f;

    return x[0] * h00 + x[1] * h01 + v[0] * h10 + v[1] * h11;
}

// n x m float matrix on the debug heap (rows allocated separately); NULL on failure.
f32** malloc_2dim_array_f32(int n, int m)
{
    f32** p;
    int i;
    int j;

#line 954 "D:/Bio4/Prog/math_sub.cpp"
    p = (f32**) MEM_ALLOC(n * sizeof(f32*), 1, 13);
    if (p == NULL) {
        pLog->err(0, 0, "malloc_2dim_array_f32(): Memory Allocation Error!");
        return NULL;
    }
    for (i = 0; i < n; i++) {
#line 962 "D:/Bio4/Prog/math_sub.cpp"
        p[i] = (f32*) MEM_ALLOC(m * sizeof(f32), 1, 13);
        if (p[i] == NULL) {
            pLog->err(0, 0, "malloc_2dim_array_f32(): Memory Allocation Error!");
            for (j = 0; j < i; j++) {
                Mem_free(p[j]);
            }
            return NULL;
        }
    }
    return p;
}

// Frees a matrix from malloc_2dim_array_f32.
void free_2dim_array_f32(int n, int m, f32** A)
{
    int i;

    for (i = 0; i < n; i++) {
        Mem_free(A[i]);
    }
    Mem_free(A);
}

// B-spline basis functions of order k + 1 for n control points at parameter t (de Boor-Cox
// recursion). `knot` may be NULL for a uniform knot vector. Returns 0 on allocation failure.
int de_Boor_Cox(int n, f32* p, f32 t, int order, f32* B)
{
    int m = order + 1;
    f32** tmp_B;
    f32* q;
    int i;
    int j;

    tmp_B = malloc_2dim_array_f32(n + m, m);
    if (tmp_B == NULL) {
        pLog->err(0, 0, "de_Boor_Cox(): tmp_B -> Memory Allocation Error!");
        return 0;
    }
#line 1048 "D:/Bio4/Prog/math_sub.cpp"
    q = (f32*) MEM_ALLOC((n + m) * sizeof(f32), 1, 13);
    if (q == NULL) {
        pLog->err(0, 0, "de_Boor_Cox(): q -> Memory Allocation Error!");
        free_2dim_array_f32(n + m, m, tmp_B);
        return 0;
    }

    for (i = 0; i < n + m; i++) {
        for (j = 0; j < m; j++) {
            tmp_B[i][j] = 0.0f;
        }
    }

    if (p != NULL) {
        for (j = 0; j < m; j++) {
            q[j] = p[0];
        }
        for (j = m; j < n; j++) {
            q[j] = (p[j - m] + p[j]) * 0.5f;
        }
        for (j = n; j < n + m; j++) {
            q[j] = p[n - 1];
        }
    } else {
        for (j = 0; j < m; j++) {
            q[j] = 0.0f;
        }
        for (j = m; j < n; j++) {
            q[j] = (f32) (j - m) + (f32) m * 0.5f;
        }
        for (j = n; j < n + m; j++) {
            q[j] = (f32) (n - 1);
        }
    }

    for (i = 0; i < n; i++) {
        if (q[i] <= t && t < q[i + 1]) {
            tmp_B[i][0] = 1.0f;
        }
    }
    if (q[n + m - 2] <= t && t <= q[n + m - 1] + 0.00001f) {
        tmp_B[n - 1][0] = 1.0f;
    }

    for (j = 1; j < m; j++) {
        for (i = 0; i < n; i++) {
            tmp_B[i][j] = 0.0f;
            if (q[i + 1] != q[i + j + 1]) {
                tmp_B[i][j] += (q[i + j + 1] - t) * tmp_B[i + 1][j - 1] / (q[i + j + 1] - q[i + 1]);
            }
            if (q[i] != q[i + j]) {
                tmp_B[i][j] += (t - q[i]) * tmp_B[i][j - 1] / (q[i + j] - q[i]);
            }
        }
    }

    for (i = 0; i < n; i++) {
        B[i] = tmp_B[i][m - 1];
    }
    Mem_free(q);
    free_2dim_array_f32(n + m, m, tmp_B);
    return 1;
}

// Sign of the permutation, an unused inline. GCC 2.95 still emits its string literal and aggregate
// initializers, which the original has between de_Boor_Cox's and MtxNNLUDecomposition's constant
// pools. The body is a guess that reproduces the bytes.
static inline f32 MtxNNPivotSign(int n, f32* a, int* ip)
{
    fprintf(stderr, "Error: Can't calc Inverse Matrix !\n");
    {
        f32 sign[2] = {1.0f, -1.0f};
        Vec zaxis = {0.0f, 0.0f, 1.0f};
        return sign[n & 1] * zaxis.z * a[ip[0]];
    }
}

// LU decomposition with partial pivoting of the n x n matrix `a` (row permutation in `ip`).
// Returns the determinant, 0 if singular.
f32 MtxNNLUDecomposition(int n, f32* A, int* ip)
{
    int i;
    int j;
    int k;
    int l = 0;
    f32 det;
    f32 max;
    f32 v;
    f32 t;

    for (i = 0; i < n; i++) {
        ip[i] = i;
    }
    det = 1.0f;
    for (k = 0; k < n; k++) {
        max = -1.0f;
        for (i = k; i < n; i++) {
            v = A[ip[i] * n + k];
            v = fabsf(v);
            if (v > max) {
                max = v;
                l = i;
            }
        }
        if (l != k) {
            j = ip[l];
            ip[l] = ip[k];
            ip[k] = j;
            det = -det;
        }
        max = A[ip[k] * n + k];
        det *= max;
        if (max == 0.0f) {
            fprintf(stderr, "Error: Can't calc Inverse Matrix !\n");
            return 0.0f;
        }
        for (i = k + 1; i < n; i++) {
            t = A[ip[i] * n + k] / max;
            A[ip[i] * n + k] = t;
            for (j = k + 1; j < n; j++) {
                A[ip[i] * n + j] -= t * A[ip[k] * n + j];
            }
        }
    }
    return det;
}

// Inverse of the n x n matrix `m` into `inv`; returns the determinant (0 = singular / no memory).
f32 MtxNNInverse(int n, f32* m, f32* m_inv)
{
    int* ip;
    f32* m_tmp;
    int i;
    int j;
    int k;
    int p;
    f32 det;
    f32 t;

#line 1278 "D:/Bio4/Prog/math_sub.cpp"
    ip = (int*) MEM_ALLOC(n * sizeof(int), 1, 13);
    if (ip == NULL) {
        pLog->err(0, 0, "MtxNNInverse(): ip, Memory allocation error!");
        return 0.0f;
    }
#line 1286 "D:/Bio4/Prog/math_sub.cpp"
    m_tmp = (f32*) MEM_ALLOC(n * n * sizeof(f32), 1, 13);
    if (m_tmp == NULL) {
        pLog->err(0, 0, "MtxNNInverse(): m_tmp, Memory allocation error!");
        Mem_free(ip);
        return 0.0f;
    }
    memcpy(m_tmp, m, n * n * sizeof(f32));
    det = MtxNNLUDecomposition(n, m_tmp, ip);
    if (det != 0.0f) {
        for (k = 0; k < n; k++) {
            for (i = 0; i < n; i++) {
                p = ip[i];
                t = (p == k) ? 1.0f : 0.0f;
                for (j = 0; j < i; j++) {
                    t -= m_tmp[p * n + j] * m_inv[j * n + k];
                }
                m_inv[i * n + k] = t;
            }
            for (i = n - 1; i >= 0; i--) {
                p = ip[i];
                t = m_inv[i * n + k];
                for (j = i + 1; j < n; j++) {
                    t -= m_tmp[p * n + j] * m_inv[j * n + k];
                }
                m_inv[i * n + k] = t / m_tmp[p * n + i];
            }
        }
    }
    Mem_free(m_tmp);
    Mem_free(ip);
    return det;
}

// out = mtx (n x m) * v
void MtxNNMultVecSR(int n, int m, f32* mat, f32* v, f32* v_dst)
{
    int i;
    int j;

    for (i = 0; i < n; i++) {
        v_dst[i] = 0.0f;
        for (j = 0; j < m; j++) {
            v_dst[i] += mat[m * i + j] * v[j];
        }
    }
}

// Project `p` along `dir` onto the plane (plane_p, plane_n).
void OrthographicProjection(Vec* src_pos, Vec* dst_pos, Vec* projection_dir, Vec* plane_pos, Vec* plane_norm)
{
    Vec d;
    f32 s;

    PSVECSubtract(plane_pos, src_pos, &d);
    s = PSVECDotProduct(plane_norm, &d);
    s /= PSVECDotProduct(plane_norm, projection_dir);
    PSVECScale(projection_dir, dst_pos, s);
    PSVECAdd(src_pos, dst_pos, dst_pos);
}

// x to the integer power n.
f32 IPOW(f32 x, int y)
{
    f32 r = 1.0f;
    int i;

    for (i = 0; i < y; i++) {
        r *= x;
    }
    return r;
}

// Fast reciprocal-square-root based sqrt (one Newton step), as the SDK inline asm.
f32 SQRTF(f32 x)
{
    f32 half = 0.5f;
    f32 three = 3.0f;
    f32 r;

    if (x <= 0.00001f) {
        return 0.0f;
    }
    asm("frsqrte 2, %1\n\t"
        "fmuls 3, 2, 2\n\t"
        "fmuls 4, 2, %2\n\t"
        "fnmsubs 3, 3, %1, %3\n\t"
        "fmuls 2, 3, 4\n\t"
        "fmuls %0, %1, 2"
        : "=f"(r)
        : "f"(x), "f"(half), "f"(three)
        : "fr2", "fr3", "fr4");
    return r;
}

// Unused accuracy test of SINF/COSF (its strings survive in .rodata).
// Never called in this build (see MtxNNPivotSign): the sin/cos accuracy and timing test whose
// strings and local aggregate initializers sit between SQRTF's and COSF's constant pools.
static inline void SinCosTest()
{
    int i;
    f32 a;

    printf("\nsinf\n");
    for (i = -30; i <= 30; i++) {
        a = (f32) i * PI / 10.0f;
        printf("% f:\t% f,% f\tdiff(% f)\n", a, sinf(a), SINF(a), sinf(a) - SINF(a));
    }
    printf("\n");
    printf("\ncosf\n");
    for (i = -30; i <= 30; i++) {
        a = (f32) i * PI / 10.0f;
        printf("% f:\t% f,% f\tdiff(% f)\n", a, cosf(a), COSF(a), cosf(a) - COSF(a));
    }
    printf("sinf =%d\n", 0);
    printf("SINF =%d\n", 0);
    printf("cosf =%d\n", 0);
    printf("COSF =%d\n", 0);
    {
        f64 magic[1] = {4503601774854144.0};
        f32 range[4] = {PI, 3.0f * PI, -3.0f * PI, 0.0f};
        a = (f32) magic[0] + range[i & 3];
    }
}

// Taylor series sin/cos on paired singles: Coeff holds the odd/even coefficients pairwise.
f32 Coeff[10] = {
    1.0000012f, 2.9073722e-06f, -0.16666685f, -5.727683e-06f, 0.008331681f,
    4.281338e-06f, -0.00019622728f, -6.4099936e-07f, 2.363633e-06f, 4.0048576e-08f,
};
f32 powx[2] = {1.0f, 1.0f};
f32 sum[2] = {0.0f, 0.0f};

// sin(x) by a paired-single Taylor series after wrapping x into [-PI, PI).
f32 SINF(f32 x)
{
    f32 r;

    x = LIMIT_ANGLE(x);
    asm volatile(
        "lis 9, Coeff@ha\n\t"
        "li 10, powx@sda21\n\t"
        "addi 9, 9, Coeff@l\n\t"
        "li 11, sum@sda21\n\t"
        "psq_l 3, 0(10), 0, 0\n\t"
        "psq_l 4, 0(11), 0, 0\n\t"
        "ps_merge00 2, 3, %1\n\t"
        "ps_muls0 2, 2, %1\n\t"
        "psq_l 5, 0(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 8(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 16(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 24(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 32(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "ps_sum0 %0, 4, 4, 4"
        : "=f"(r)
        : "f"(x)
        : "r9", "r10", "r11", "fr2", "fr3", "fr4", "fr5");
    return r;
}

// cos(x) by the same series.
f32 COSF(f32 x)
{
    f32 r;

    x = LIMIT_ANGLE(x + 1.5707964f);
    asm volatile(
        "lis 9, Coeff@ha\n\t"
        "li 10, powx@sda21\n\t"
        "addi 9, 9, Coeff@l\n\t"
        "li 11, sum@sda21\n\t"
        "psq_l 3, 0(10), 0, 0\n\t"
        "psq_l 4, 0(11), 0, 0\n\t"
        "ps_merge00 2, 3, %1\n\t"
        "ps_muls0 2, 2, %1\n\t"
        "psq_l 5, 0(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 8(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 16(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 24(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 32(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "ps_sum0 %0, 4, 4, 4"
        : "=f"(r)
        : "f"(x)
        : "r9", "r10", "r11", "fr2", "fr3", "fr4", "fr5");
    return r;
}

// Wrap an angle into [-PI, PI) (SDK-style inline asm loop).
f32 LIMIT_ANGLE(f32 x)
{
    f32 min = -PI;
    f32 max = PI;
    f32 step = PI2;

    asm("fcmpu 0, %0, %2\n\t"
        "blt 1f\n"
        "0:\n\t"
        "fsubs %0, %0, %3\n\t"
        "fcmpu 0, %0, %2\n\t"
        "bge 0b\n\t"
        "b 2f\n"
        "1:\n\t"
        "fcmpu 0, %0, %1\n\t"
        "bge 2f\n"
        "3:\n\t"
        "fadds %0, %0, %3\n\t"
        "fcmpu 0, %0, %1\n\t"
        "blt 3b\n"
        "2:"
        : "+f"(x)
        : "f"(min), "f"(max), "f"(step)
        : "cr0");
    return x;
}
