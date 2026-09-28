#ifndef DBMODULE_H
#define DBMODULE_H

#include "types.h"
#include "vec.h"
#include "gx.h"

class cObj;
struct JOY;

// game/dbmodule.cpp: debug primitive drawing.
void init_dbmodule();
void Render_tile(void* pTile);
void Draw_tile(int x, int y, int w, int h, GXColor* col);
void Draw_line(Vec* p0, Vec* b, u32 col);
void Draw_quad(Vec* pPos, Vec* pSize, u32 col);
void Draw_line3d(Vec* p0, Vec* p1, u32 col, int blend);
void Draw_line3d_local(Vec* p0, Vec* b, Mtx mat, u32 col, int blend);
void Draw_line3d_init();
void Draw_line3d_end();
void Draw_poly(Vec* p, u32 col, int zmode);
void Draw_poly_local(Vec* p, Mtx mat, u32 col, int zmode);
void Draw_sphere(Vec* pos, f32 r, u32 rgb, int zmode, int zcheck);
void Draw_cylinder(Vec* pos, f32 r, f32 h, u32 rgba);
void Draw_cylinderMtx(Mtx mtx, Vec* pos, f32 r, f32 h, u32 color);
void Draw_corn3(Vec* pos, Vec* norm, f32 cutoff, u32 rgb);
void Draw_corn(Vec* pos, Vec* ang, f32 l, f32 r, u32 rgb);
void Draw_corn2(Vec* pPos, Vec* pVec, f32 size, f32 r, u32 col);
void Draw_box(Vec* pBoxVec, u32 col, int flg);
void Draw_pos(Vec* pos, int size);
void Draw_local_pos(Vec* pos, int size, Mtx mat);
void Draw_floor(int size, int num, u32 col);
void init_sphere();
void init_circle();
void init_cylinder();
void init_corn();
void DrawObjWireframe(cObj* pObj, int col);
void DrawRoomWireframe();
void DispTime(s16 x, int y, int col, int time, int flag);

JOY* GetBugCheckController();

#endif
