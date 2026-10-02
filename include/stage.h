#ifndef STAGE_H
#define STAGE_H

#include "types.h"

// game/stage.cpp: enemy list (ESL) selection and the sub-missions.
int checkEmListNo(u16 room_no);
const char* getEmListName(u32 no);
const char* getEmListDbgName(int no);
int getEmListNum();
void StageSet();
void readEmList(int proc);
int checkSubMissionTarget(int stage_no, int target_no);
void SubMissionCheck();

#endif
