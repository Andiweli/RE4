#ifndef ETC_MODEL_H
#define ETC_MODEL_H

#include "types.h"
#include "vec.h"

// Etc model id (PS2 ETCMODEL_ID): EtcSetData::id, getRoomEtc `id`, the EtcModelSet switch. The GC data
// goes up to 0x67 (ETC_AUTO_DOOR5); ETC_IRON_DOOR26/27 were added on the PS2.
enum ETCMODEL_ID {
    ETC_WINDOW00 = 0,
    ETC_WOODBOX_SML = 1,
    ETC_WOODBOX_MDL = 2,
    ETC_DOOR00 = 3,
    ETC_TANA00 = 4,
    ETC_TANA01 = 5,
    ETC_HASIGO00 = 6,
    ETC_WINDOW07 = 7,
    ETC_HASIGO01 = 8,
    ETC_DOOR_SP00 = 9,
    ETC_TAIMATU02 = 10,
    ETC_LANTERN_A = 11,
    ETC_DENKYUU = 12,
    ETC_IRON_DOOR00 = 13,
    ETC_SWITCH = 14,
    ETC_BARRED00 = 15,
    ETC_LANTERN_B = 16,
    ETC_WOODBOX_BARREL = 17,
    ETC_DRAM = 18,
    ETC_DOOR01 = 19,
    ETC_TAIMATU01 = 20,
    ETC_TANA_BOX = 21,
    ETC_IRON_DOOR01 = 22,
    ETC_IRON_DOOR02 = 23,
    ETC_DOOR02 = 24,
    ETC_TAIMATU03 = 25,
    ETC_MEDAL00 = 26,
    ETC_BARRED01 = 27,
    ETC_DENKYUU01 = 28,
    ETC_WINDOW1D = 29,
    ETC_WOODBOX_BARREL2 = 30,
    ETC_NEST = 31,
    ETC_DOOR03 = 32,
    ETC_IRON_DOOR03 = 33,
    ETC_DOOR04 = 34,
    ETC_IRON_DOOR_DOWN00 = 35,
    ETC_IRON_DOOR04 = 36,
    ETC_WINDOW25 = 37,
    ETC_BARRED02 = 38,
    ETC_IRON_DOOR05 = 39,
    ETC_BARRED03 = 40,
    ETC_WINDOW29 = 41,
    ETC_IRON_DOOR06 = 42,
    ETC_IRON_DOOR07 = 43,
    ETC_WINDOW2C = 44,
    ETC_BOMB_BARREL = 45,
    ETC_TUBO_A_S = 46,
    ETC_TUBO_A_L = 47,
    ETC_YOROI = 48,
    ETC_IRON_DOOR_DOWN02 = 49,
    ETC_IRON_DOOR11 = 50,
    ETC_IRON_DOOR12 = 51,
    ETC_IRON_DOOR13 = 52,
    ETC_WINDOW35 = 53,
    ETC_WINDOW36 = 54,
    ETC_IRON_DOOR_DOWN01 = 55,
    ETC_DENKYUU02 = 56,
    ETC_IRON_DOOR08 = 57,
    ETC_BARRED04 = 58,
    ETC_IRON_DOOR10 = 59,
    ETC_GUS_BOMBE = 60,
    ETC_DOOR05 = 61,
    ETC_IRON_DOOR14 = 62,
    ETC_IRON_DOOR15 = 63,
    ETC_DOOR06 = 64,
    ETC_IRON_DOOR17 = 65,
    ETC_DEKA_ITA = 66,
    ETC_AUTO_DOOR = 67,
    ETC_WINDOW44 = 68,
    ETC_IRON_DOOR18 = 69,
    ETC_IRON_DOOR19 = 70,
    ETC_IRON_DOOR20 = 71,
    ETC_WINDOW48 = 72,
    ETC_IRON_DOOR21 = 73,
    ETC_WINDOW4A = 74,
    ETC_AUTO_DOOR2 = 75,
    ETC_BARRED05 = 76,
    ETC_IRON_DOOR22 = 77,
    ETC_BARRED06 = 78,
    ETC_AUTO_DOOR3 = 79,
    ETC_WINDOW50 = 80,
    ETC_WINDOW51 = 81,
    ETC_WINDOW52 = 82,
    ETC_WINDOW53 = 83,
    ETC_WINDOW54 = 84,
    ETC_WINDOW55 = 85,
    ETC_WINDOW56 = 86,
    ETC_WINDOW57 = 87,
    ETC_WINDOW58 = 88,
    ETC_IRON_DOOR23 = 89,
    ETC_WINDOW5A = 90,
    ETC_WINDOW5B = 91,
    ETC_WINDOW5C = 92,
    ETC_WINDOW5D = 93,
    ETC_WINDOW5E = 94,
    ETC_WINDOW5F = 95,
    ETC_WINDOW60 = 96,
    ETC_AUTO_DOOR4 = 97,
    ETC_IRON_DOOR25 = 98,
    ETC_IRON_DOOR24 = 99,
    ETC_WINDOW64 = 100,
    ETC_WINDOW65 = 101,
    ETC_ZOU = 102,
    ETC_AUTO_DOOR5 = 103,
    ETC_IRON_DOOR26 = 104,
    ETC_IRON_DOOR27 = 105,
    ETC_ID_MAX = 106
};

// One room etc model record (0x28 bytes, EtcModelListSet steps through them) handed to the
// Et*_init functions (EtcModel.cpp, et00.cpp).
struct EtcSetData {
    u16 id;          // 0x00  ETCMODEL_ID (EtcModelSet switch, 0x00..0x67)
    union {
        u16 no;      // 0x02  g_EtcTbl slot (< 0x40)
        struct {
            u8 pad_2;
            u8 type; // 0x03  low byte of `no`: the etc number the Set* functions take (WindowData row)
        };
    };
    u8 pad_4[0x10 - 0x4];
    Vec ang;         // 0x10
    Vec pos;         // 0x1C
};

class cEmItem;   // emitem.h

// C++ linkage (sym_map: GetEtcFlgPtr__Fii, getRoomEtcItem__FiPP7cEmItemi)
u16* GetEtcFlgPtr(u32 etc_no, u16 room_no);   // etc flag word of etc model `no` in `room` (stage << 8 | room), 0 when none
int getRoomEtcItem(int room, cEmItem** ppEm, int bErrDisp);

void* GetEtcAddr(void* arc, const char* name);   // file `name` inside the room etc archive
// Model a light of parent type 3 (room etc model) hangs on; 1 = found (light.cpp)
int getRoomEtcOnLight(u32 no, class cModel** ppEm, int bErrDisp);

// Room etc enemies by etc number (the stage rooms delete / hide them); 1 = found.
class cEm;
class cEmWindow;   // emwindow.h
class cEmDoor;     // emdoor.h
class cObjLadder;  // obj13.h
class cEmRack;     // em.h
class cEmBox;      // embox.h
class cEmTorch;    // emtorch.h
class cEmSwitch;   // emswitch.h
class cEmBarred;   // emBarred.h
class cEmBarrel;   // embarrel.h
int getRoomEtcBreak(int no, cEm** ppEm, int bErrDisp);
int setRoomEtcDisp(int no, int bDisp, int bErrDisp);
int getRoomEtcWindow(int no, cEmWindow** ppEm, int bErrDisp);
int getRoomEtcBox(int no, cEmBox** ppEm, int bErrDisp);
int getRoomEtcDoor(int no, cEmDoor** ppEm, int bErrDisp);
int getRoomEtcRack(int no, cEmRack** ppEm, int bErrDisp);
int getRoomEtcLadder(int no, cObjLadder** ppEm, int bErrDisp);
int getRoomEtcTorch(int no, cEmTorch** ppEm, int bErrDisp);
int getRoomEtcSwitch(int no, cEmSwitch** ppEm, int bErrDisp);
int getRoomEtcBarred(int no, cEmBarred** ppEm, int bErrDisp);
int getRoomEtcDram(int no, cEmBarrel** ppEm, int bErrDisp);
int EtcGetDasAddr(int id, void** pRet_addr);   // archive of etc model `id` (r400 setLadderMotion)
// Generic lookup by etc type (getRoomEtc* call it; r20d counts the torches / lamps with it).
int getRoomEtc(int no, ETCMODEL_ID id, cEm** pRet, int bDispErr);

// Additive ambient kind of an etc model (PS2 ETC_AMB_KIND): EtcSetAddAmb `kind`, the row of the etc_*_rgb tables.
enum ETC_AMB_KIND {
    ETC_AMB_TORCH = 0,
    ETC_AMB_BARREL = 1,
    ETC_AMB_DOOR = 2,
    ETC_AMB_BOX = 3,
    ETC_AMB_ITEM = 4,
    ETC_AMB_HIT = 5,
    ETC_AMB_WINDOW00 = 6,
    ETC_AMB_WINDOW90 = 7,
    ETC_AMB_RACK = 8,
    ETC_AMB_NEST = 9,
    ETC_AMB_DRAM = 10,
    ETC_AMB_TUBO = 11,
    ETC_AMB_FALL_LANTERN = 12,
    ETC_AMB_BAR = 13,
    ETC_AMB_WINDOW30 = 14,
    ETC_AMB_WINDOW40 = 15,
    ETC_AMB_WINDOW50 = 16
};

// Which etc_*_rgb table the room uses (PS2 ETC_AMB_TYPE): GetEtcAmbType.
enum ETC_AMB_TYPE {
    ETC_AMB_DAY = 0,
    ETC_AMB_NIGHT = 1,
    ETC_AMB_DAY2 = 2,
    ETC_AMB_DAY3 = 3,
    ETC_AMB_DAY4 = 4
};

// Init, room setup, room data load and the debug list (main.cpp / game.cpp / t_sce_item.cpp).
struct EtcList;
void EtcModelInit();
void EtcModelRoomInit();
int EtcModelDataLoad(void* addr);
int EtcModelListSet(EtcList* list);
int EtcModelGetLastNo();
void EtcModelDebugDisp();
// Adds the room etc model's ambient to `m` (the object enemies call it from their model setup).
void EtcSetAddAmb(class cModel* pMod, int kind);   // no: ETC_AMB_KIND

#endif
