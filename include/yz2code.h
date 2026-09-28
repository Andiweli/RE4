#ifndef YZ2CODE_H
#define YZ2CODE_H

#include "types.h"

// Yaz0 ("Yz2") decompressor (game/yz2code.cpp): read.cpp unpacks the room archives with it.

// Primes the decoder on the compressed image `buf`; returns the decoded size.
u32 Yz2DecodeSet(char* str, void* buf);
// Decodes into `dst`.
void Yz2DecodeExec(void* dst);

#endif
