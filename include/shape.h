#ifndef SHAPE_H
#define SHAPE_H

#include "types.h"
#include "model.h"

// Vertex-morph ("shape") animation of a model part (game/shape.cpp): the face morphs of the player /
// partner and the mouth / eye shapes of the enemies.

// Advances the shape animations of a parts list (per frame, from the model trans).
int ShapeMove(cModelInfo* i_pModelInfo);
// Clears every part's shape state of a model.
void ClrShape(cModel* pMod);
// Restores the unmorphed vertices of a part into `dst`.
void ResetShape(cModelInfo* info, void* dst);
// Adds the weighted vertex deltas of `data` onto the vertex buffer `dst`.
void CalculateShape_new(cModelInfo* info, f32 rate, ShapeData* data, u8* dst);

// C++ linkage (pl_mod.h declares the same). Starts a shape animation on a part; returns 0 when the
// data has no frames.
int ShapeSet(void* work, int frame, void* data, int flags);
void ShapeEnd(void* pModelInfo);

#endif
