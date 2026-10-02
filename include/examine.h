#ifndef EXAMINE_H
#define EXAMINE_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "model.h"
#include "id_sys.h"
#include "light.h"

// Item examine view (game/examine.cpp): shows an item model in front of the item camera.

// Per-item display parameters (exam_info / exam_info_ext, 0x20 bytes).
struct ITEM_EXAMINE_INFO {
    u16 item_id;     // 0x00  item id
    u16 x2;     // 0x02
    Vec rot;    // 0x04  initial rotation (degrees)
    f32 scale;  // 0x10  camera distance divisor
    s32 light_no;  // 0x14  light cut selected from pG->pCore (0..4)
    s32 axis_scr;   // 0x18  rotation axis for mode 0 (0 world Y, 1 model Y)
    s32 axis_id;   // 0x1C  rotation axis for mode 1
};

class ItemExamine {
private:
    IDSystem* m_pIdSys;    // 0x00  IdSys (mode 0) / IdSub (mode 1, 2)
    u32 m_be_flag_bak;        // 0x04  model->be_flag at init
    Vec m_pos_bak;         // 0x08
    Vec m_ang_bak;         // 0x14
    u8 m_ot_type_bak;         // 0x20
    u8 m_scrn_flag;             // 0x21  0 in game, 1 sub screen, 2 puzzle
    s8 m_level[4];            // 0x22  weapon tune levels (power, speed, reload, bullet)
public:
    u8 pad_26[2];
private:
    cCoord* m_pList_pParent_bak;  // 0x28  model->pList->pParent at init
    Vec m_pList_pos_bak;    // 0x2C
    Vec m_pList_ang_bak;    // 0x38
public:
    u16 m_item_id;              // 0x44  item id
    u8 pad_46[2];
    cModel* m_pModel;       // 0x48
    ITEM_EXAMINE_INFO* m_pInfo;      // 0x4C
    cLight* m_pLight[3];    // 0x50

    void setup();
    void idSet();
    void init(u16 id, cModel* model, u8 mode);
    void level(s8 pwr, s8 spd, s8 rld, s8 blt);
    void move();
    void trans();
    void quit();
    void reset();
};

extern ItemExamine itemExam;
extern CAMERA itemCamera;
extern ITEM_EXAMINE_INFO exam_info_ext[2];
extern f32 cap_dist_min;
extern f32 cap_dist_max;
extern f32 cap_xrad_max;

#endif
