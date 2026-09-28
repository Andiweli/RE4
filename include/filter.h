#ifndef FILTER_H
#define FILTER_H

#include "types.h"
#include "vec.h"

// Screen post-process filters (game/filter.cpp dispatches to game/filter00.cpp .. filter0b.cpp).
int FilterInit();
int FilterRoomInit();
void FilterTrans();

void Filter00Init();
void Filter00RoomInit();
void Filter00Trans();
void Filter01Init();
void Filter01RoomInit();
void Filter01Trans();
void Filter02Init();
void Filter02RoomInit();
void Filter02Trans();
void Filter03Init();
void Filter03RoomInit();
void Filter03Trans();
void Filter04Init();
void Filter04RoomInit();
void Filter04Trans();
void Filter05Init();
void Filter05RoomInit();
void Filter05Trans();
void Filter05SetParam(int a, int b, int c, int d, int e, f32 d_alpha, f32 start_a, f32 size, int blend, int tex_id);
void Filter06Init();
void Filter06RoomInit();
void Filter06Trans();
void Filter07Init();
void Filter07RoomInit();
void Filter07Trans();
void Filter08Init();
void Filter08RoomInit();
void Filter08Trans();
void Filter09Init();
void Filter09RoomInit();
int Filter09GetbUse();
void Filter0aInit();
void Filter0aRoomInit();
void Filter0aTrans();
void Filter0bInit();
void Filter0bRoomInit();
void Filter0bTrans();

// filter00.cpp blur parameters (cLightMgr::setBlur)
void Filter00SetAlpha(u8 alpha);
void Filter00SetPower(s8 pow);
void Filter00SetType(u32 type);
void Filter00SetContrast(u8 r, u8 g, u8 bias);

// filter00.cpp: additive radial blur request (highest priority wins)
void Filter00SetAddSpread(u32 pri, int on, u8 r, u8 g, u8 b, u8 rate, u8 type, u32 num, f32 cx, f32 cy, f32 pow);
// filter03.cpp
void Filter03SetParam(int level, u8 r, u8 g, u8 b, u8 pri, int bUse_AlphaDraw2);
// filter06.cpp
void Filter06SetParam(u32 level, int r, int g, int b, int a, f32 rate, f32 alpha, Vec* spd, Vec* spdRand, f32 scale,
                      int alphaMin);
// filter01.cpp: depth-of-field request (cam_extra.cpp; the event camera passes the focus z itself)
void Filter01SetParam(int mode, int z, u8 type, f32 level);
void Filter01SetParam_CamZ(int mode, u8 type, f32 level, f32 camz);
// filter09.cpp: EFB capture of the pause screen and the blur-use switch (game.cpp)
void Filter09GetEFB_801D19E0();
void Filter09SetbUse(int use, int bSpred);
// filter0b.cpp: capture buffer of the r333 screen effect
void Filter0bAllocBuf();
void Filter0bFreeBuf();
void Filter0bCapture();
void Filter0bSetAlpha(u8 alpha);

// filter0a.cpp: the mask filter switch and its parameters (cam_extra.cpp sets them per camera cut).
extern u8 use_filter0a;
extern u8 filter0a_mask_flag;
extern u8 filter0a_mask_id;
extern u8 filter0a_mask_alpha;

#endif
