#ifndef EM_SET_H
#define EM_SET_H

#include "types.h"
#include "em.h"
#include "global.h"


extern cEm* errEm;   // returned by EmSetFromList2 when no enemy was created

extern "C" {
int checkListId(int no);                    // 0 when an alive enemy already carries list entry `no`
void EmSetFromList();                       // create every enemy of the current room from the list
cEm* EmSetFromList2(int no, int flag);   // create list entry `no`; errEm on failure
cEm* GetEmPtrFromList(int no);              // alive enemy created from list entry `no`
EM_LIST* GetListPtrFromEm(cEm* em);
u32 GetEmIdFromList(u32 no);
void EmListSetAlive(int no, int on_off);
void EmSetDie(cEm* em);                     // remember the death of `em` in pG->Em_flg
void EmSetDieCnt(cEm* pEm);
void EmSetRoomInit();                       // clear the "set" bit of every entry
void EmListWaitDelete();
}

// Creates an enemy from a list record built by the caller (C++ linkage; sce_at, the stage rooms).
cEm* EmSetEvent(EM_LIST* pData);

#endif
