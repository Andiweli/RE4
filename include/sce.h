#ifndef SCE_H
#define SCE_H

#include "types.h"
#include "vec.h"
#include "scheduler.h"

class cEm;

// Scenario helpers (game/sce_com.cpp / sce_sys.cpp).
void SceEventStart(int mode);
void SceEventEnd(int mode);
void SceSleep(int ctr);
void SceUpCutStart();
void SceUpCutEnd();
int SceCheckEventStart();
void SceSetRoomExitFunc(TaskFunc pFunc, void* param);
void SetFree(int no, u32 val);
u32 GetFree(int no);
void SceMesSet(int no, u32 flags, int sel, int x, int y);
void SceMesCamSndSet(int mes_no, int cam_no, int se_no, int attr);
enum UP_CUT_ATTR {
    UP_CUT_ATTR_NONE = 0,
    UP_CUT_ATTR_MES_COMMON = 1,
    UP_CUT_ATTR_SE_COMMON = 2,
    UP_CUT_ATTR_CUT_FIX = 4
};

void SceUpCut(int mes_no, int cam_no, int se_no, int attr);
int SceMesGetSelection();
void SceMesWait();
void SceSndCallThunder();
int SceCheckEmAlive(cEm* pEm);
int SceCountEmAlive(int em_id, int em_id_end);
void SceDestroyEm(int em_id, int em_id_end);
void SceInitItemEvent();
void SceSetItemEvent(int atNo, int itemNo, int flagNo, int cut, TaskFunc func, TaskFunc doneFunc, void* arg, int enable);
void getChapterSection(int no, int* chap, int* sect);
void SceChapterEnd();
enum CHAPTER_NO {
    CHAPTER_1_1 = 0,
    CHAPTER_1_2 = 1,
    CHAPTER_1_3 = 2,
    CHAPTER_2_1 = 3,
    CHAPTER_2_2 = 4,
    CHAPTER_2_3 = 5,
    CHAPTER_3_1 = 6,
    CHAPTER_3_2 = 7,
    CHAPTER_3_3 = 8,
    CHAPTER_3_4 = 9,
    CHAPTER_4_1 = 10,
    CHAPTER_4_2 = 11,
    CHAPTER_4_3 = 12,
    CHAPTER_4_4 = 13,
    CHAPTER_5_1 = 14,
    CHAPTER_5_2 = 15,
    CHAPTER_5_3 = 16,
    CHAPTER_5_4 = 17,
    CHAPTER_FINAL = 18,
    CHAPTER_END = 19,
    ADA_MISSION_1 = 20,
    ADA_MISSION_2 = 21,
    ADA_MISSION_3 = 22,
    ADA_MISSION_4 = 23,
    ADA_MISSION_5 = 24
};

void SceSetChapterEnd(int ChapterNo, int door_at_no);
void SceCamMove(Vec* pCamPos, Vec* pTarget, f32 fovy);
enum OpenBoxType {
    OpenBoxLR = 0,
    OpenBoxL = 1,
    OpenBoxR = 2,
    OpenBoxUpXP = 3,
    OpenBoxUpXM = 4,
    OpenBoxUpZP = 5,
    OpenBoxUpZM = 6,
    OpenBoxPartsUpXP = 7,
    OpenBoxPartsUpXM = 8,
    OpenBoxPartsUpZP = 9,
    OpenBoxPartsUpZM = 10,
    OpenBoxDwXP = 11,
    OpenBoxDwXM = 12,
    OpenBoxDwZP = 13,
    OpenBoxDwZM = 14,
    OpenBoxPosXP500 = 15,
    OpenBoxPosXM500 = 16,
    OpenBoxPosZP500 = 17,
    OpenBoxPosZM500 = 18,
    OpenBoxHandleL = 19,
    OpenBoxHandleR = 20,
    OpenBoxFall = 21,
    OpenBoxFallNoRot = 22,
    OpenBoxLR2 = 23,
    OpenBoxL2 = 24,
    OpenBoxR2 = 25,
    OpenBoxNone = 26
};

void OpenBoxMain(int type, int mode, int se, u32 id1, u32 id2, int itemNo);
void SceDebugDisp(const char* fmt, ...);
// sce_com.cpp: the elevator task; every room that has one defines its own ElevatorParam copy.
struct ElevatorParam;
void SceElevator(ElevatorParam* d);

// sce_com.cpp: debug trigger check, always 0 (title's mercenaries unlock-all). C++ linkage.
int DebugTrg(int no);

#endif
