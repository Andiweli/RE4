#ifndef CINESCO_H
#define CINESCO_H

#include "types.h"

// Cinema-scope letterbox bars (game/cinesco.cpp).
typedef struct {
    u8 rno0;    // 0x00  index into cine_tbl (0 poll, 1 fade in, 2 fade out)
    u8 alpha;   // 0x01
    s8 old_flag;   // 0x02  last seen pG->flags_500C bit 24
    u8 pad;     // 0x03
    f32 timer0;  // 0x04
} CineWork;     // 0x08

#ifdef __cplusplus
#endif
extern CineWork cine_work;

void CinescoMove(void);
void Draw_cinesco(void);
void CinescoInit(void);
#ifdef __cplusplus
#endif

#endif
