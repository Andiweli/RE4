#ifndef EM_MOD_H
#define EM_MOD_H

// Declarations for the enemy REL modules (files/em/emXX.rel: src/emXX/emXX.cpp and the Ganado
// per-enemy objects src/emXX/emXX_set.cpp). Included by the module units only, never by the DOL
// units that define these symbols (game/em.cpp, game/read.cpp): a declaration of EmInitFunc ahead
// of its definition changes how GCC 2.95 addresses it there.

#include "em.h"

class cEm10;

// game/em.cpp: the module's _prolog stores its Init function here; EmCreate calls it for the read-table enemy.
extern void (*EmInitFunc)(cEm* em);

// em10/em10.cpp: picks the Ganado voice table for the model type (the *_set.cpp Set functions call it).
void Em10SetSeTbl(cEm10* em, int type);

#endif
