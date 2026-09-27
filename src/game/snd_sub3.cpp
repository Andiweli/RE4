// game/snd_sub3: sound driver accessors for the ISS (SE bank) and stream data blocks. A SIT volume or
// pan below 0 means "take the DLS default".
#include "snd_drv.h"

// Registers an ISS block from its file header (count, DLS, SIT and sequence offsets).
void Snd_iss_blk_init(u32 blk_no, void* data)
{
    SND_ISS_BLK* blk;
    u32* p;

    blk = &Snd_iss_blk[blk_no];
    p = (u32*) data;
    blk->num = *p++;
    blk->dls = (u8*) data + *p++;
    blk->sit = (SND_SIT*) ((u8*) data + *p++);
    blk->seq = (u8*) data + *p++;
}

// Registers a stream block from its file header (count, stream headers, RIT offsets).
void Snd_str_blk_init(u32 blk_no, void* data)
{
    SND_STR_BLK* blk;
    u32* p;

    blk = &Snd_str_blk[blk_no];
    p = (u32*) data;
    blk->num = *p++;
    blk->shd = (u8*) data + *p++;
    blk->rit = (SND_RIT*) ((u8*) data + *p++);
}

// The ISS block `blk_no`.
SND_ISS_BLK* Snd_get_blk_adrs(u16 blk_no, u16 req_no)
{
    return &Snd_iss_blk[blk_no];
}

// SIT entry `req_no` of ISS block `blk_no`.
SND_SIT* Snd_get_sit_adrs(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_iss_blk[blk_no].sit;
    sit += req_no;
    return sit;
}

// RIT entry `req_no` of stream block `blk_no`.
SND_RIT* Snd_get_rit_adrs(u16 blk_no, u16 req_no)
{
    SND_RIT* rit;

    rit = Snd_str_blk[blk_no].rit;
    rit += req_no;
    return rit;
}

// Stream header the RIT entry points at (shd_no through the header offset table).
SND_SHD* Snd_get_shd_adrs(u16 blk_no, u16 req_no)
{
    SND_RIT* rit;
    u8* shd;

    rit = Snd_get_rit_adrs(blk_no, req_no);
    shd = Snd_str_blk[blk_no].shd;
    shd += ((u32*) shd)[rit->str_no];
    return (SND_SHD*) shd;
}

// SIT type: 0x8000 dummy, else the low 3 flag bits (1 one-shot, 4 sequence).
u16 Snd_iss_get_sit_type(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->flag & 0x8000) {
        return 0x8000;
    }
    return sit->flag & 0x7;
}

// The SIT's volume, or the DLS region's when the SIT says < 0.
s8 Snd_iss_get_sit_vol(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 vol;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->vol < 0) {
        vol = get_dls_vol_pan(blk_no, req_no, 0);
        return vol;
    } else {
        return sit->vol;
    }
}

// The SIT's surround volume, or its volume when unset.
s8 Snd_iss_get_sit_svol(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 vol;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->svol < 0) {
        vol = Snd_iss_get_sit_vol(blk_no, req_no);
        return vol;
    } else {
        return sit->svol;
    }
}

// The SIT's pan: -1 = positional (game computes it), other negatives = the DLS articulation pan.
s8 Snd_iss_get_sit_pan(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;
    s8 pan;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->pan == -1) {
        return -1;
    }
    if (sit->pan < 0) {
        pan = get_dls_vol_pan(blk_no, req_no, 1);
        return pan;
    } else {
        return sit->pan;
    }
}

// The SIT's surround pan: -1 = positional, other negatives = 0x7F.
s8 Snd_iss_get_sit_span(u16 blk_no, u16 req_no)
{
    SND_SIT* sit;

    sit = Snd_get_sit_adrs(blk_no, req_no);
    if (sit->span == -1) {
        return -1;
    }
    if (sit->span < 0) {
        return 0x7F;
    } else {
        return sit->span;
    }
}

// DLS default for the SIT's program: mode 0 the region attenuation as a 0..127 volume, 1 the
// articulation pan.
s8 get_dls_vol_pan(u16 blk_no, u16 req_no, int mode)
{
    SND_ISS_BLK* blk;
    SND_SIT* sit;
    SND_WT_HDR* hdr;
    WTINST* inst;
    WTREGION* rgn;
    WTART* art;
    s32 vol;

    blk = Snd_get_blk_adrs(blk_no, req_no);
    sit = Snd_get_sit_adrs(blk_no, req_no);
    hdr = (SND_WT_HDR*) blk->dls;
    inst = (WTINST*) (blk->dls + hdr->inst_ofs);
    inst += (u16) (sit->note >> 8);
    rgn = (WTREGION*) (blk->dls + hdr->rgn_ofs);
    rgn += inst->keyRegion[sit->note & 0xFF];
    art = (WTART*) (blk->dls + hdr->art_ofs);
    art += rgn->articulationIndex;
    if (mode == 0) {
        vol = rgn->attn / 0x10000;
        return Snd_vol_ax_to_syn(vol);
    } else {
        return art->pan;
    }
}
