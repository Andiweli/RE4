#ifndef ROOM_DATA_H
#define ROOM_DATA_H

#include "types.h"
#include "cFlag.h"

// Per-room save data and room DLL control (game/roomdata.cpp).
struct OSModuleHeader;

// One room of a stage table (St<n>_data_tbl), 0xC bytes.
struct ROOM_DATA {
    u8 save_flg;         // 0x00  1 = the room has a save record
    u8 pad_1;
    u16 rel_file;      // 0x02  FileTbl index of the room DLL (0 = none)
    void (*pInit)();  // 0x04
    void (*pMain)();  // 0x08
};

// Room_data_tbl[10]: one row per stage.
struct StageTbl {
    ROOM_DATA* tbl;  // 0x00
    u16 num;            // 0x04
    u16 pad_6;
};

// Save buffer header, followed by num records of 0xD8 bytes.
struct RoomSaveHdr {
    u32 size;  // 0x00  total bytes including this header
    u32 num;   // 0x04
    u8 pad_8[8];
};

// One room save record (0xD8 bytes): stage, room, passed bits, the script flag words, the etc model
// flags and the room's BGM / stream tables.
struct ROOM_SAVE_DATA {
    union {
        u16 RoomNo;    // 0x00  stage << 8 | room
        struct {
            u8 Stage;  // 0x00
            u8 Room;   // 0x01
        };
    };
    u8 passed_flg;         // 0x02  bit (0x80 >> n): checkPassed/setPassed
    u8 _padding;           // 0x03
    u32 save_flg[1];       // 0x04
    u32 item_flg[4];       // 0x08
    u32 item_find_flg[4];  // 0x18
    u16 EtcModelFlg[64];   // 0x28
    u32 BgmTable[6];       // 0xA8  room BGM table: slot 0 low half, slot 1 high half
    u32 StrTable[6];       // 0xC0  room stream table
};

class cRoomData {
public:
    enum CTRL_FLAG {
        CTRL_STOP = 0,  // room DLL unlinked (stopRelData)
    };

private:
    u16 m_RoomNum;            // 0x00  rooms in all stage tables
    u16 m_SaveNum;            // 0x02  rooms with a save record
    cFlag<u16, CTRL_FLAG> m_CtrlFlag;  // 0x04
    u8 pad_6[2];
    OSModuleHeader* m_pModule;  // 0x08  linked room DLL (exception.cpp loads its symbols)
    void* m_pModule_bss;               // 0x0C  DLL bss
    void* m_pModule_bss_bak;            // 0x10  bss copy kept while the DLL is unlinked
    RoomSaveHdr* m_pRoomSaveHead;    // 0x14
    u8* m_pRoomSaveData;                // 0x18  room save records, 0xD8 bytes each
public:
    u16 m_RelNo;              // 0x1C  FileTbl index (rel_no) of the room dll loaded; cleared before linkRelData (stage.cpp)
    u16 x1E;                  // 0x1E

    cRoomData() { m_CtrlFlag.reset(); }
    ~cRoomData() {}  // the empty destructor is what makes GCC emit the static destructor function

    u32 getSaveDataSize() { return m_SaveNum * 0xD8 + 0x10; }
    OSModuleHeader* getModulePtr() { return m_pModule; }

    void init();
    void initRoomSet();
    void save(void* pData);
    void load(void* pData);
    void clear(void* p);
    // record for room `room` (stage << 8 | room_no), or NULL when the room has none
    u8* getRoomSavePtr(u16 room_no);
    void execInitFunc(u16 room_no);
    void execMainFunc(u16 room_no);
private:
    int checkRoomRange(u8 stage, u8 room);
public:
    int checkRelRead(u16 room_no);
    void linkRelData(u16 room_no);
    void stopRelData();
    void restartRelData();
    int checkPassed(u16 room_no, int part_no);
    void setPassed(u16 room_no, int part_no);
};

extern cRoomData RoomData;

// game/roomdata.cpp: the per-stage room tables the stage modules' Init fills (StN_data_tbl[no].init = ...).
extern ROOM_DATA St1_data_tbl[33];
extern ROOM_DATA St2_data_tbl[46];
extern ROOM_DATA St3_data_tbl[52];
extern ROOM_DATA St4_data_tbl[18];

#endif
