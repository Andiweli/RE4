#ifndef ACT_BTN_H
#define ACT_BTN_H

#include "types.h"

// Action prompt kind (PS2 ACTION_TYPE): cActionButton::set `act_type` / ActBtnWork::kind, the message is
// kind + 0x16. ACT_WIRE is the PS2 grapple gun; the padding entries size the table.
enum ACTION_TYPE {
    ACT_TALK = 0,
    ACT_CHECK = 1,
    ACT_JUMP_OUT = 2,
    ACT_JUMP_IN = 3,
    ACT_JUMP_DOWN = 4,
    ACT_JUMP_OVER = 5,
    ACT_PUSH = 6,
    ACT_KICK = 7,
    ACT_GO_UP = 8,
    ACT_GET_DOWN = 9,
    ACT_KNOCK_DOWN = 10,
    ACT_STAND = 11,
    ACT_JUMP_AT = 12,
    ACT_LOOK = 13,
    ACT_PEEP = 14,
    ACT_SNIPING = 15,
    ACT_OPEN = 16,
    ACT_SWIM = 17,
    ACT_JUMP_BACK = 18,
    ACT_STOOP = 19,
    ACT_OPERATION = 20,
    ACT_RESCUE = 21,
    ACT_RIDE_SHOULDER = 22,
    ACT_SEARCH_ATTACK = 23,
    ACT_SPRINT = 24,
    ACT_CLIMB = 25,
    ACT_JUMP = 26,
    ACT_SLIDE_DOWN = 27,
    ACT_CATCH = 28,
    ACT_PULL_UP = 29,
    ACT_WAIT_OTHER = 30,
    ACT_SEPARATE_OTHER = 31,
    ACT_HIDE_OTHER = 32,
    ACT_FOLLOW_OTHER = 33,
    ACT_HELP_OTHER = 34,
    ACT_RIDE = 35,
    ACT_GET_OFF = 36,
    ACT_GUARD = 37,
    ACT_HIDE = 38,
    ACT_THROUGH = 39,
    ACT_PICK_UP = 40,
    ACT_QUICK_STICK = 41,
    ACT_ROTATE = 42,
    ACT_RESIST = 43,
    ACT_SUPLEX = 44,
    ACT_HELI_ORDER = 45,
    ACT_SHOOTING = 46,
    ACT_SAVE = 47,
    ACT_NO_DISP = 48,
    ACT_ACCELERATE = 49,
    ACT_ACCELE = 50,
    ACT_THROUGH_OTHER = 51,
    ACT_SIT_DOWN = 52,
    ACT_THROW_DOWN = 53,
    ACT_ANSWER = 54,
    ACT_SNEAK = 55,
    ACT_SENPUU = 56,
    ACT_BACKKICK = 57,
    ACT_POISON_NEEDLE = 58,
    ACT_EXECUTE = 59,
    ACT_PALM_SHOCK = 60,
    ACT_NERICHAGI = 61,
    ACT_JUMP_MOVE = 62,
    ACT_PUSH_SWITCH = 63,
    ACT_GET_DOWN_1M = 64,
    ACT_FIRE = 65,
    ACT_OPERATION2 = 66,
    ACT_WIRE = 67,
    ACT_padding_68 = 68,
    ACT_padding_69 = 69,
    ACT_padding_70 = 70,
    ACT_padding_71 = 71,
    ACT_padding_72 = 72,
    ACT_padding_73 = 73,
    ACT_padding_74 = 74,
    ACT_padding_75 = 75,
    ACT_padding_76 = 76,
    ACT_padding_77 = 77,
    ACT_padding_78 = 78,
    ACT_padding_79 = 79,
    ACT_padding_80 = 80,
    ACTION_TYPE_MAX = 81
};

// Prompt control bits (PS2 ACTCTR_FLAG): set `ctrl_flag` / ActBtnWork::flags.
enum ACTCTR_FLAG {
    ACTCTR_NONE = 0,
    ACTCTR_WEP_SET_IGNORE = 1,
    ACTCTR_ENFORCE_EXEC = 2,
    ACTCTR_NO_SUSPEND = 4,
    ACTCTR_NO_DISP = 8,
    ACTCTR_NO_TRG = 16,
    ACTCTR_NO_EXEC = 32,
    ACTCTR_EXACT_KEY = 64,
    ACTCTR_DOOR_COLOR = 128,
    ACTCTR_HOOKSHOT_COLOR = 256
};

// Button shown by the prompt (PS2 DISP_FLAG): set `button_type` / ActBtnWork::btn (checkButton).
enum DISP_FLAG {
    DISP_OFF = 0,
    DISP_A_NORMAL = 1,
    DISP_A_RAPID = 2,
    DISP_L_R = 3,
    DISP_A_B = 4,
    DISP_B = 5,
    DISP_X = 6,
    DISP_Y = 7,
    DISP_Z = 8,
    DISP_L = 9,
    DISP_R = 10,
    DISP_GACHA = 11,
    DISP_ROTATE = 12,
    DISP_B_RAPID = 13,
    DISP_A_ACCENT = 14,
    DISP_STICK_A = 15,
    DISP_STICK_UP = 16
};

// How the prompt runs its function (PS2 ACTION_FUNC_TYPE): set `func_type` / ActBtnWork::type.
enum ACTION_FUNC_TYPE {
    ACT_FUNC_NORMAL = 0,
    ACT_FUNC_SCE = 1,
    ACT_FUNC_SCE_AT = 2
};

// One action button prompt (0x18 bytes), linked into cActionButton::ot by slot.
struct ActBtnWork {
    u32 tag;       // 0x00  OTag link
    void* func;    // 0x04  void (*)(int arg, int d): the action (NULL = none)
    void* arg;     // 0x08  first argument (type 2: the SCE_AT_DATA*)
    u8 kind;       // 0x0C  ACTION_TYPE: prompt message kind + 0x16 (clamped to 0x41)
    u8 slot;       // 0x0D  ot slot / SceExec priority
    u8 type;       // 0x0E  ACTION_FUNC_TYPE: 0 call func, 1 SceExec(0x12, func...), 2 SceAt area action
    u8 btn;        // 0x0F  DISP_FLAG: button kind (checkButton)
    int d;         // 0x10  second argument
    u32 flags;     // 0x14  ACTCTR_FLAG bits: WEP_SET_IGNORE sets pG->flags_500C 0x200000, ENFORCE_EXEC no actCheck /
                   //       trigger, NO_SUSPEND exec flag 2, NO_DISP no prompt, NO_TRG hold, NO_EXEC skip,
                   //       EXACT_KEY exclusive, DOOR_COLOR message colour 7
};

// Action button prompt manager (game/act_btn.cpp `ActBtn`, 0x104 bytes).
class cActionButton {
private:
    u32 m_ot[16];          // 0x00
    u8 m_num;              // 0x40  works pulled this frame
    u8 m_stop_flag_old;             // 0x41  pG->flags_170 bit8 at init: prompts disabled
    u8 m_active_flag;           // 0x42  a prompt was shown this frame
    u8 pad_43;
    ActBtnWork m_task[8];  // 0x44

    void disp(ActBtnWork* work);
    int checkButton(ActBtnWork* work);
    int checkPLStatus(ActBtnWork* work);
    ActBtnWork* pullWork();

public:
    cActionButton() {}
    ~cActionButton() {}
    void init();
    void move();
    // set(kind, slot, func, arg, flags, btn, type, d): pulls a work, fills it and adds the prim.
    // PS2: set(ACTION_TYPE act_type, SCE_PRIORITY priority, func, param, ctrl_flag, DISP_FLAG button_type,
    // ACTION_FUNC_TYPE func_type, model); the ints are fixed by the mangled name.
    void set(int kind, int slot, void* func, void* arg, int flags, int btn, int type, void* d);
    void setStopFlagOld(int flag) { m_stop_flag_old = flag; }
    void setActive(int on) {
        if (on) {
            m_active_flag = 1;
        } else {
            m_active_flag = 0;
        }
    }
    int isActive() { return m_active_flag == 1; }
};

extern cActionButton ActBtn;

#endif
