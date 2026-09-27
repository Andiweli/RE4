// game/filter: dispatcher of the full-screen post filters 00..0b in filterXX.cpp
// (D:/Bio4/Prog/filter.cpp). Filter 08 runs before 02, so zoom blur comes before depth of field.
#include "filter.h"
#include "main_sub.h"

// Boot init of all twelve filters (allocates filter00's blur buffer).
int FilterInit()
{
    Filter00Init();
    Filter01Init();
    Filter02Init();
    Filter03Init();
    Filter04Init();
    Filter05Init();
    Filter06Init();
    Filter07Init();
    Filter08Init();
    Filter09Init();
    Filter0aInit();
    Filter0bInit();
    return 1;
}

// Room init of all twelve filters: resets their parameters and forgets their temp buffers.
int FilterRoomInit()
{
    Filter00RoomInit();
    Filter01RoomInit();
    Filter02RoomInit();
    Filter03RoomInit();
    Filter04RoomInit();
    Filter05RoomInit();
    Filter06RoomInit();
    Filter07RoomInit();
    Filter08RoomInit();
    Filter09RoomInit();
    Filter0aRoomInit();
    Filter0bRoomInit();
    return 1;
}

// Per-frame: queues every filter's render pass, only on the full 512x448 frame buffer with blur
// permission and while the pause filter (09) is not active.
// Full-screen filters only run on the full-size frame buffer.
void FilterTrans()
{
    if (Render_checkBlurPermission()) {
        if (Screen.width == 512.0f && Screen.height == 448.0f && !Filter09GetbUse()) {
            Filter00Trans();
            Filter01Trans();
            Filter08Trans();
            Filter02Trans();
            Filter03Trans();
            Filter04Trans();
            Filter05Trans();
            Filter06Trans();
            Filter07Trans();
            Filter0aTrans();
            Filter0bTrans();
        }
    }
}
