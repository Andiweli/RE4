// game/room_tex: the room texture registry (a cTexSys, texture.cpp) — created at room start with
// the core archive's common room textures, extended by the room's own texture data, queried by id
// for TPL / texture object / palette (light maps of the shadow lights, effects, room models).
#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "texture.h"
#include "room_tex.h"

#line 20 "D:/Bio4/Prog/room_tex.cpp"

cTexSys* g_pRoomTexSys;

// Boot: no room texture system yet.
void RoomTexInit()
{
    g_pRoomTexSys = NULL;
}

// Room start: allocates the "RoomTex" texture system (256 slots) and loads the core archive's
// common room textures (ofs_18) as owner 1.
void RoomTexRoomInit()
{
#line 41
    g_pRoomTexSys = (cTexSys*) MEM_ALLOC(sizeof(cTexSys), 1, 0xD);
    g_pRoomTexSys->Init("RoomTex", 256);
    RoomTexDataLoad((TexData*) CORE_ARC(CORE_ROOM_TEXTURE), 1);
}

// Registers a texture data block with the room texture system under `owner` (freed per owner).
int RoomTexDataLoad(TexData* data, u32 owner)
{
    return g_pRoomTexSys->DataLoad(data, owner, 1);
}

// TPL of room texture `id`.
int RoomGetTplAddr(u32 id, TEXPalette** out)
{
    return g_pRoomTexSys->GetTplAddr(id, out);
}

// GX texture object `no` of room texture `id`.
int RoomGetTexObj(u32 id, u32 no, GXTexObj** out)
{
    return g_pRoomTexSys->GetTexObj(id, no, out);
}

// GX palette object of room texture `id`.
int RoomGetTlutObj(u32 id, GXTlutObj** out)
{
    return g_pRoomTexSys->GetTlutObj(id, out);
}
