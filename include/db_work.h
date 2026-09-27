#ifndef DB_WORK_H
#define DB_WORK_H

#include "types.h"
#include "model.h"

// Debug page 11: model work viewer (game/db_work.cpp), `DbWork` in game.cpp.
class cDbWork {
private:
    int wkType;  // 0x00  0 = enemies, 1 = objects, 2 = lights
    u32 wkNo;    // 0x04  work index shown

public:
    cDbWork();
    void move();
    void dispEm();
    void dispObj();
    void dispModel(cModel* pMod, int x, int y);  // x in 8-pixel columns, y in 14-pixel lines
    void dispLit();
};

#endif
