#ifndef EPRINTF_H
#define EPRINTF_H

#include "types.h"

extern int eprintf_init;   // 1 once the font is loaded (dvd.cpp waits for it before printing)

// Debug text output (game/eprintf.cpp).
void eprintf(int x, int y, int color, int p, const char* fmt, ...);
// binary-coded nibble -> hex digit helper used by the flag editor
int BtoX(int b);
void eprintf2(int x, int y, int a, int b, int c, int p, const char* fmt, ...);

// Init and the per-frame flush of the queued text (main.cpp, dvd.cpp, exception.cpp).
void EprintfInit();
void EprintfFlush();
int EprintfSetCurrentNo(int no);

#endif
