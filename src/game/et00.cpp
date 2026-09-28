// game/et00.cpp: room etc window models (Et00..Et65): one init function per etc id that
// loads the model and effect data from the etc archive and creates the cEmWindow.

#include "atari.h"
#include "light.h"
#include "event.h"
#include "emwindow.h"
#include "etc_model.h"
#include "db_log.h"
#include "esp.h"

int Et00_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et07_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et1d_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et25_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et29_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et2c_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et35_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et36_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et44_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et48_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et4a_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et50_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et51_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et52_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et53_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et54_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et55_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et56_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et57_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et58_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5a_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5b_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5c_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5d_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5e_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5f_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et60_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et64_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et65_init(void* arc, EtcSetData* d, cModel** out, int flag);

// Shared body of the Et*_init entries: fetches the model (bin/tpl) from the etc archive, creates the
// cEmWindow at the EtcSetData position/angle with its type, loads the window's effect data under
// effect owner effId, keeps it alive through suspends and, when the window is already flagged broken
// (ChkStatus bit 0, from the room flags), switches to the broken model. Returns 0 when SetWindow failed.
int EtXX_init(void* arc, EtcSetData* d, cModel** out, int flag, const char* bin, const char* tpl, const char* eff, int effId)
{
    void* pBin;
    cEmWindow* em;

    pBin = GetEtcAddr(arc, bin);
    em = SetWindow(pBin, GetEtcAddr(arc, tpl), &d->pos, &d->ang, flag, d->type, arc);
    if (em == 0) {
        pLog->err(0, 0, "Etxx_init : set failed");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, eff), effId, 0);
    em->setEff(effId);
    em->setNoSuspend(1);
    if (em->ChkStatus() & 1) {
        em->SetBreakModel();
    }
    *out = em;
    return 1;
}

// Etc id 0x00: window model et0000.bin/et0000.tpl with effect data et00.eff loaded as effect owner 0x54.
int Et00_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et0000.bin", "et0000.tpl", "et00.eff", 0x54);
}

// Etc id 0x07: window model et0700.bin/et0700.tpl with effect data et07.eff loaded as effect owner 0x5B.
int Et07_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et0700.bin", "et0700.tpl", "et07.eff", 0x5B);
}

// Etc id 0x1d: window model et1d00.bin/et1d00.tpl with effect data et1d.eff loaded as effect owner 0x71.
int Et1d_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et1d00.bin", "et1d00.tpl", "et1d.eff", 0x71);
}

// Etc id 0x25: window model et2500.bin/et2500.tpl with effect data et25.eff loaded as effect owner 0x79.
int Et25_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et2500.bin", "et2500.tpl", "et25.eff", 0x79);
}

// Etc id 0x29: window model et2900.bin/et2900.tpl with effect data et29.eff loaded as effect owner 0x7D.
int Et29_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et2900.bin", "et2900.tpl", "et29.eff", 0x7D);
}

// Etc id 0x2c: window model et2c00.bin/et2c00.tpl with effect data et2c.eff loaded as effect owner 0x80.
int Et2c_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et2c00.bin", "et2c00.tpl", "et2c.eff", 0x80);
}

// Etc id 0x35: window model et3500.bin/et3500.tpl with effect data et35.eff loaded as effect owner 0x89.
int Et35_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et3500.bin", "et3500.tpl", "et35.eff", 0x89);
}

// Etc id 0x36: window model et3600.bin/et3600.tpl with effect data et36.eff loaded as effect owner 0x8A.
int Et36_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et3600.bin", "et3600.tpl", "et36.eff", 0x8A);
}

// Etc id 0x44: window model et4400.bin/et4400.tpl with effect data et44.eff loaded as effect owner 0x98.
int Et44_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et4400.bin", "et4400.tpl", "et44.eff", 0x98);
}

// Etc id 0x48: window model et4800.bin/et4800.tpl with effect data et48.eff loaded as effect owner 0x9C.
int Et48_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et4800.bin", "et4800.tpl", "et48.eff", 0x9C);
}

// Etc id 0x4a: window model et4a00.bin/et4a00.tpl with effect data et4a.eff loaded as effect owner 0x9E.
int Et4a_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et4a00.bin", "et4a00.tpl", "et4a.eff", 0x9E);
}

// Etc id 0x50: window model et5000.bin/et5000.tpl with effect data et50.eff loaded as effect owner 0xA4.
int Et50_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5000.bin", "et5000.tpl", "et50.eff", 0xA4);
}

// Etc id 0x51: window model et5100.bin/et5100.tpl with effect data et51.eff loaded as effect owner 0xA5.
int Et51_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5100.bin", "et5100.tpl", "et51.eff", 0xA5);
}

// Etc id 0x52: window model et5200.bin/et5200.tpl with effect data et52.eff loaded as effect owner 0xA6.
int Et52_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5200.bin", "et5200.tpl", "et52.eff", 0xA6);
}

// Etc id 0x53: window model et5300.bin/et5300.tpl with effect data et53.eff loaded as effect owner 0xA7.
int Et53_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5300.bin", "et5300.tpl", "et53.eff", 0xA7);
}

// Etc id 0x54: window model et5400.bin/et5400.tpl with effect data et54.eff loaded as effect owner 0xA8.
int Et54_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5400.bin", "et5400.tpl", "et54.eff", 0xA8);
}

// Etc id 0x55: window model et5500.bin/et5500.tpl with effect data et55.eff loaded as effect owner 0xA9.
int Et55_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5500.bin", "et5500.tpl", "et55.eff", 0xA9);
}

// Etc id 0x56: window model et5600.bin/et5600.tpl with effect data et56.eff loaded as effect owner 0xAA.
int Et56_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5600.bin", "et5600.tpl", "et56.eff", 0xAA);
}

// Etc id 0x57: window model et5700.bin/et5700.tpl with effect data et57.eff loaded as effect owner 0xAB.
int Et57_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5700.bin", "et5700.tpl", "et57.eff", 0xAB);
}

// Etc id 0x58: window model et5800.bin/et5800.tpl with effect data et58.eff loaded as effect owner 0xAC.
int Et58_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5800.bin", "et5800.tpl", "et58.eff", 0xAC);
}

// Etc id 0x5a: window model et5a00.bin/et5a00.tpl with effect data et5a.eff loaded as effect owner 0xAE.
int Et5a_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5a00.bin", "et5a00.tpl", "et5a.eff", 0xAE);
}

// Etc id 0x5b: window model et5b00.bin/et5b00.tpl with effect data et5b.eff loaded as effect owner 0xAF.
int Et5b_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5b00.bin", "et5b00.tpl", "et5b.eff", 0xAF);
}

// Etc id 0x5c: window model et5c00.bin/et5c00.tpl with effect data et5c.eff loaded as effect owner 0xB0.
int Et5c_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5c00.bin", "et5c00.tpl", "et5c.eff", 0xB0);
}

// Etc id 0x5d: window model et5d00.bin/et5d00.tpl with effect data et5d.eff loaded as effect owner 0xB1.
int Et5d_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5d00.bin", "et5d00.tpl", "et5d.eff", 0xB1);
}

// Etc id 0x5e: window model et5e00.bin/et5e00.tpl with effect data et5e.eff loaded as effect owner 0xB2.
int Et5e_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5e00.bin", "et5e00.tpl", "et5e.eff", 0xB2);
}

// Etc id 0x5f: window model et5f00.bin/et5f00.tpl with effect data et5f.eff loaded as effect owner 0xB3.
int Et5f_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et5f00.bin", "et5f00.tpl", "et5f.eff", 0xB3);
}

// Etc id 0x60: window model et6000.bin/et6000.tpl with effect data et60.eff loaded as effect owner 0xB4.
int Et60_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et6000.bin", "et6000.tpl", "et60.eff", 0xB4);
}

// Etc id 0x64: window model et6400.bin/et6400.tpl with effect data et64.eff loaded as effect owner 0xB8.
int Et64_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et6400.bin", "et6400.tpl", "et64.eff", 0xB8);
}

// Etc id 0x65: window model et6500.bin/et6500.tpl with effect data et65.eff loaded as effect owner 0xB9.
int Et65_init(void* arc, EtcSetData* d, cModel** out, int flag)
{
    return EtXX_init(arc, d, out, flag, "et6500.bin", "et6500.tpl", "et65.eff", 0xB9);
}
