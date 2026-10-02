// Core archive BODY word slots, including its four-word header (slot 4 is sub-file 0).
// Descriptive names from the cited uses, not recovered vendor identifiers or filenames.
#ifndef ARC_CORE_H
#define ARC_CORE_H

#define CORE_SPECULAR_0                   0x04  // CoreDataRead: SpecularInit, first palette
#define CORE_EFFECT                       0x05  // eff_sys: EspDataLoad, EFF_CORE
#define CORE_ROOM_TEXTURE                 0x06  // RoomTexDataLoad
#define CORE_VIBRATION                    0x07  // VibSetData
#define CORE_DUMMY_BIN                    0x08  // Event::ExeBeginEvt: etc/core/dummy.bin
#define CORE_DUMMY_TPL                    0x09  // Event::ExeBeginEvt: etc/core/dummy.tpl
#define CORE_MESSAGE                      0x0A  // MesData.registData, types 0..2
#define CORE_LIGHT                        0x0B  // game: LightMgr.roomInit
#define CORE_CAMERA                       0x0C  // game: CamCtrl.CoreDataRead
#define CORE_LIGHT_PATH                   0x0F  // game: LightMgr.initPath
#define CORE_GLOBAL_ILLUMINATION          0x10  // CoreDataRead: GlobalIlmTexInit
#define CORE_SPECULAR_1                   0x11  // CoreDataRead: SpecularInit, second palette
#define CORE_SPECULAR_2                   0x12  // CoreDataRead: SpecularInit, third palette
#define CORE_SPECULAR_3                   0x13  // CoreDataRead: SpecularInit, fourth palette
#define CORE_ITEM_EFFECT                  0x14  // eff_sys: EspDataLoad, EFF_ITM
#define CORE_MESSAGE_3                    0x15  // MesData.registData, type 3
#define CORE_EXAMINE_LIGHT_0              0x16  // ItemExamine::init: light_no 0
#define CORE_EXAMINE_LIGHT_1              0x17  // ItemExamine::init: light_no 1
#define CORE_EXAMINE_LIGHT_2              0x18  // ItemExamine::init: light_no 2
#define CORE_EXAMINE_LIGHT_3              0x19  // ItemExamine::init: light_no 3
#define CORE_EXAMINE_LIGHT_4              0x1A  // ItemExamine::init: light_no 4
#define CORE_SYSTEM_MESSAGE               0x1B  // dvd: pMes[4]
#define CORE_TV_MODE_MESSAGE              0x1C  // tv_mode: MesData.registData
#define CORE_COCKPIT_TEXTURE              0x1D  // cockpit: IdTexDataLoad, TEX_OWNER_ID_COCKPIT
#define CORE_EXAMINE_ID                   0x1E  // ItemExamine: IDC_EXAMINE
#define CORE_LIFE_METER_ID                0x1F  // cockpit: IDC_LIFE_METER
#define CORE_ACTION_BUTTON_ID             0x20  // cockpit: IDC_ACT_BUTTON
#define CORE_COUNT_DOWN_ID                0x21  // cockpit: IDC_COUNT_DOWN
#define CORE_CINESCO_ID                   0x22  // cockpit: IDC_CINESCO
#define CORE_UNDER_CONSTRUCTION_TEXTURE   0x24  // gameDebugDisp: DBG_UNDER_CONST
#define CORE_MESSAGE_WINDOW_ID            0x25  // cockpit: IDC_MSG_WINDOW
#define CORE_BULLET_ICON_ID               0x26  // cockpit: IDC_BLLT_ICON
#define CORE_SUB_MISSION_ID               0x27  // stage: IDC_SUB_MISSION

#endif
