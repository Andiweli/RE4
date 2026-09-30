#ifndef T_EVENT_H
#define T_EVENT_H

#include "types.h"
#include "joy.h"
#include "hermite.h"

// Event tool (t_event REL, D:/Bio4/Prog/t_event.cpp): the ToolEvt task object, its preview state
// machine and the fog / focus (Hermite curve) and message (cDbgToolMain) sub tools.

class Event;
class cLightTool;
struct _DB_SCTRL;
typedef struct _DB_SCTRL DB_SCTRL;

struct DatFog {
    HERMITE_1_FIX ScurveStart;    // 0x000
    HERMITE_1_FIX ScurveEnd;      // 0x404
};

struct DatFocus {
    HERMITE_1_FIX ScurveNear;    // 0x000
    HERMITE_1_FIX ScurveFar;     // 0x404
    f32 LevelNear;     // 0x808
    f32 LevelFar;      // 0x80C
};

// Message list of the event (the tool's work, 0x964 bytes inside a 1,000,000-byte block): the
// cDbgEditWindow<EventMessageData::MessElem> rows.
struct EventMessageData {
    struct MessElem {
        int be_flag;      // 0x00  bit 0: in use
        int No;        // 0x04  row number
        int CutNo;     // 0x08
        int Frame;     // 0x0C
        int MessNo;    // 0x10  -1: continue the previous message
        int Timer;     // 0x14
    };
    MessElem elem[100];  // 0x000
    int num;             // 0x960
};

// The first 0x40 bytes of the event file (copied into the tool and into EvtDebug at load).
struct EvtHdrCopy {
    u32 w[16];
};

// ToolEvt::EtcFlag bit numbers (PS2 TefFlag): FlagOnVar numbers them from bit 31 down, so the mask of flag f is
// TefBit(f); SubToolIn takes the number.
enum TefFlag {
    TefExit = 0,
    TefStop = 1,
    TefStopOnReq = 2,
    TefStopOffReq = 3,
    TefRev = 4,
    TefEvtCancel = 5,
    TefPrevSubMenu = 6,
    TefCaptureReq = 7,
    TefCaptureFullSize = 8,
    TefCaptureRun = 9,
    TefCaptureEnd = 10,
    TefDispPrint = 11,
    TefRemainEnd = 12,
    TefToolLight = 13,
    TefToolFog = 14,
    TefToolFocus = 15,
    TefToolMess = 16,
    TefTaskSuspend = 17
};

#define TefBit(f) (0x80000000 >> (f))

class ToolEvt {
private:
    s16 r_no_0;             // 0x00  main routine (0 menu, 1 preview, 2 exit)
    s16 r_no_1;             // 0x02  preview step
    s16 r_no_2;              // 0x04
    s16 r_no_3;              // 0x06
    s16 r_no_0_sub;          // 0x08  preview sub routine (0 menu, 1 fog, 2 focus)
    s16 r_no_1_sub;              // 0x0A
    s16 r_no_2_sub;              // 0x0C
    s16 r_no_3_sub;              // 0x0E
    s16 FFTimer;         // 0x10  frames the start button is held before the stop toggles
    s16 StopTimer;        // 0x12  frames before the event starts
    u32 EtcFlag;            // 0x14  TefBit(TefFlag) bits
    u32 ListCur;              // 0x18
    u32 ListBase;              // 0x1C
    int CaptureTimer;           // 0x20  capture frame counter
    u8 CurveNo;           // 0x24  edited curve (0 start / near, 1 end / far)
    s8 CursolMain;           // 0x25  main menu cursor
    s8 CursolSub;            // 0x26  preview menu cursor
    s8 CursolFog;            // 0x27  fog menu cursor
    s8 CursolFocus;          // 0x28  focus menu cursor
public:
    u8 pad_29[3];
private:
    void* PFil;           // 0x2C  event file (8,000,000 bytes)
    u8 DebugCameraFlag;           // 0x30  debug camera on
    u8 DebugCameraTimer;            // 0x31
    char ToolFileName[0x22];  // 0x32  selected file name
public:
    EvtHdrCopy hdr;       // 0x54
    u8 pad_94[4];
private:
    cLightTool* pTl;  // 0x98
    JOY* pJoy1;           // 0x9C  &Joy[0] (&Joy[2] while a sub tool runs)
    JOY* pJoy2;           // 0xA0  &Joy[1] (&Joy[3])
    DatFog DatFogWk;       // 0xA4
    DatFocus DatFocusWk;   // 0x8AC
    DB_SCTRL* PDatDbSctrl;  // 0x10BC  (1,000,000 bytes)
public:
    u32 x10C0[8];         // 0x10C0
    u8 pad_10E0[0x28];
    u32 x1108;            // 0x1108
    u8 pad_110C[4];
    EventMessageData* PMesDat;  // 0x1110  (1,000,000 bytes)
    char roomNo[0x10];      // 0x1114  "r10b" of the file name
    char eventNo[0x10];        // 0x1124  "s10"
    u8 pad_1134[4];

    ToolEvt();
    ~ToolEvt();
private:
    void EvtTaskSuspend(int task);
    void EvtTaskSignal(int task);
public:
    void Run();
private:
    void RunStop(ToolEvt* t, Event* ev);
public:
    static void MainMenu(ToolEvt* t);
    static void MainPreview(ToolEvt* t);
    static void MainExit(ToolEvt* t);
private:
    void EventDel(Event* ev);
public:
    static void SubMenuMain(ToolEvt* t, Event* ev);
    static void SubMenuFog(ToolEvt* t, Event* ev);
    static void SubMenuFocus(ToolEvt* t, Event* ev);
private:
    int SubMenuSelectYesNo(ToolEvt* t, const char* s1, const char* s2);
    int SubMenuEditFocusLevel(ToolEvt* t, Event* ev, const char* name, f32* level);
    int SubToolCameraMove(ToolEvt* t);
    void SubToolLightInit(ToolEvt* t, int sw);
    void SubToolLightMove(ToolEvt* t);
    int SubToolFogWkInit(ToolEvt* t, Event* ev);
    void SubToolFogInit(ToolEvt* t, int sw, Event* ev, int which);
    void SubToolFogMove(ToolEvt* t, Event* ev);
    void SubToolFocusWkInit(ToolEvt* t, Event* ev);
    void SubToolFocusInit(ToolEvt* t, int sw, Event* ev, int which);
    void SubToolFocusMove(ToolEvt* t, Event* ev);
    void SubToolMessInit(ToolEvt* t, int sw);
    void SubToolMessMove(ToolEvt* t, Event* ev);
    void SubToolIn(ToolEvt* t, int sw, int bit);
    void SctrlToolInit(ToolEvt* t, HERMITE_1_PTR* curve, f32 xMax, f32 yMax);
};

void ToolEvent();

#endif
