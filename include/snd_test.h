#ifndef SND_TEST_H
#define SND_TEST_H

#include "types.h"
// the driver header declares the work under its own type; this header owns the tool's view
#ifndef SND_SDK_NO_GX
#define SND_SDK_NO_GX
#endif
#define Snd_test_work Snd_test_work_drv_view
#include "snd_drv.h"
#undef Snd_test_work

// Sound test of the t_movie REL (t_movie/snd_test.cpp: Snd_test_*/test_*/disp_*/aram_*; the entry
// SoundTest and the file-list helper live in t_movie.cpp). The work is the sound driver's
// Snd_test_work (snd_ram.c, 0x7C0 bytes; snd_drv.h's SND_TEST is the driver-side view).


extern SND_TEST Snd_test_work;

int Snd_test_mode();
const char* Snd_test_get_str_name(int type, u16 no);

#endif
