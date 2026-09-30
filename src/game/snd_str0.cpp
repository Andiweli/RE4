// game/snd_str0: game-side entry points of the sound driver's streamed audio (ADPCM from disc).
// They prepare a stream work and set request bits that the audio frame executes (snd_str1..4).
#include "snd_drv.h"

// Prepares stream `req_no` of stream block `blk_no` from file `name` in slot `no` (-1 = the RIT's
// default slot). Returns the sound id, 0 when the slot is busy or no voice is free.
u32 Snd_str_prepare(u16 blk_no, u16 req_no, char* name, s8 no)
{
    SND_RIT* rit;
    SND_SHD* shd;
    u32 ret;

    rit = Snd_get_rit_adrs(blk_no, req_no);
    shd = Snd_get_shd_adrs(blk_no, req_no);
    ret = Snd_str_init(shd, rit, shd->offset, name, no);
    return ret;
}

// Fills a stream work: opens the file, new sound id, type (surround by RIT flag), buffer / block
// sizes (mono 32 KB half-blocks, stereo 16 KB interleaved), read end rounded up, loop points from
// the header, 8 ARAM blocks (a stream that fits entirely is "short"), and acquires the AX voices.
u32 Snd_str_init(SND_SHD* shd, SND_RIT* rit, u32 aram, char* name, s8 no)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_STR* str;
    s8 idx;

    if (no >= 0) {
        idx = no;
    } else {
        idx = rit->pl_id;
    }
    str = &Snd_str_work[idx];
    if (str->be_flag != 0) {
        return 0;
    }
    DVDOpen(name, &str->info);
    ctrl->req_id++;
    if (ctrl->req_id == 0) {
        ctrl->req_id++;
    }
    str->snd_id = ctrl->req_id;
    if (rit->flag & 0x1) {
        str->str_type = 2;
    } else {
        str->str_type = 1;
    }
    str->update = 0;
    str->shd_adrs = shd;
    str->rit_adrs = rit;
    str->buff_ptr = Snd_str_buff[idx];
    str->dvd_status = 0;
    str->shd_flag = shd->flag;
    str->req_flag = 0;
    str->err_flag = 0;
    str->recv_type = 0;
    str->loop_flag = 0;
    str->short_flag = 0;
    str->pan = rit->pan;
    str->span = str_init_get_span(rit);
    str->vol = str_init_get_vol(rit);
    str->svol = 0;
    str->aux_a = rit->aux_a;
    str->aux_b = rit->aux_b;
    str->smp_rate = (f32) shd->rate;
    str->req_vol = 0;
    str->out_vol = 0;
    str->now_vol = 0;
    str->req_fade_time = 0;
    str->req_fade_end = 0;
    str->nml_fade_spd = 0;
    str->nml_fade_end = 0;
    str->sys_fade_spd = 0;
    str->sys_fade_end = 0;
    str->file_offset = aram;
    str->file_pos = 0;
    if (str->shd_flag & 0x1) {
        str->buff_size = 0x4000;
        str->buff_one = str->buff_size * 2;
        str->file_size = shd->nibbles / 2 * 2;
    } else {
        str->buff_size = 0x8000;
        str->buff_one = str->buff_size;
        str->file_size = shd->nibbles / 2;
    }
    if (str->file_size % str->buff_one != 0) {
        str->file_size = str->file_size / str->buff_one + 1;
        str->file_size = str->file_size * str->buff_one;
    }
    str->buff_size_nbl = str->buff_size * 2;
    str->aram_nbl = 0;
    str->play_nbl = 0;
    str->next_nbl = str->buff_size_nbl;
    str->lptop_nbl = shd->lptop_nbl;
    str->lpend_nbl = shd->lpend_nbl;
    str->now_play_idx = -1;
    str->old_play_idx = -1;
    str->ttl_play_idx = 0;
    str->dvd_busy = 0;
    str->dvd_comp = 0;
    str->dvd_req_num = 1;
    str->dvd_req_idx = 0;
    str->dvd_end_idx = -1;
    str->dvd_req_max = 1;
    str->dma_busy = 0;
    str->dma_req_num = 0;
    str->dma_req_idx = 0;
    str->dma_end_idx = -1;
    str->dma_req_max = 8;
    if (str->file_size <= str->buff_one * str->dma_req_max) {
        str->short_flag |= 0x1;
        if (str->shd_flag & 0x4) {
            str->short_flag |= 0x2;
        } else {
            str->short_flag |= 0x4;
        }
    }
    if (Snd_str_ax_voice_init(str, rit->ch, rit->poly) == 1) {
        return 0;
    }
    str->rno = 0;
    str->rno_sv = 0;
    str->be_flag = 1;
    return str->snd_id;
}

// Surround pan from the RIT (0x7F when unset).
s8 str_init_get_span(SND_RIT* rit)
{
    if (rit->span >= 0) {
        return rit->span;
    } else {
        return 0x7F;
    }
}

// Volume from the RIT: the surround volume in DPL2 mode when the RIT has one.
s8 str_init_get_vol(SND_RIT* rit)
{
    if (Snd_get_sound_mode() == 2 && rit->svol >= 0) {
        return rit->svol;
    }
    return rit->vol;
}

// Request on stream `snd_id`: cmd 1 start buffering, 2 play, 4 fade to `vol` over `time`, 8 stop,
// 0x10 volume `time` at once. Always returns 0.
int Snd_str_req(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = str_req_sub(snd_id, cmd, time, vol);
    OSRestoreInterrupts(old);
    return 0;
}

// Stores the request bits / parameters on the stream work; 1 when the id is unknown.
int str_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    SND_STR* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 1;
    }
    str->req_flag |= cmd;
    if (cmd & 0x6) {
        str->req_fade_time = time;
        str->req_fade_end = vol;
    }
    if (cmd & 0x10) {
        str->req_vol = time;
    }
    return 0;
}

// Marks every stream of `type` for a volume recomputation.
void Snd_str_reset_vol_type(u8 type)
{
    str_type_sub(type, 0, 0);
}

// Marks every stream of `type` for a pan recomputation.
void Snd_str_reset_pan_type(u8 type)
{
    str_type_sub(type, 1, 0);
}

// Fades every stream of `type` out over `time` (0 = stop).
void Snd_str_fade_out_type(u8 type, s16 time)
{
    str_type_sub(type, 2, time);
}

// For every active stream of `type`: mode 0 volume refresh, 1 pan refresh, 2 fade-out / stop.
void str_type_sub(u8 type, u32 mode, s16 time)
{
    SND_STR* str;
    int old;
    int i;

    old = OSDisableInterrupts();
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->be_flag == 0) {
            continue;
        }
        if (!(str->str_type & type)) {
            continue;
        }
        switch (mode) {
        case 1:
            str->update |= 0x2;
            break;
        case 0:
            str->update |= 0x1;
            break;
        case 2:
            if (time == 0) {
                str->req_flag |= 0x8;
            } else {
                str->req_flag |= 0x4;
                str->req_fade_time = time;
                str->req_fade_end = 0;
            }
            break;
        }
    }
    OSRestoreInterrupts(old);
}

// The stream's status word (bit0 in use, 1 ready, 4 buffering, 0x10 playing, 0x20 MIX channel,
// 0x100 fading, 0x8000 DVD error...), -1 when unknown.
int Snd_str_get_status(u32 snd_id)
{
    SND_STR* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return -1;
    }
    return (s16) str->be_flag;
}

// 1 while the stream work exists, 0 when gone.
int Snd_str_end_check(u32 snd_id)
{
    SND_STR* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 0;
    } else {
        return 1;
    }
}

// 4 while a stream of `type` is active.
int Snd_str_pronounce_ck_type(u8 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = str_pro_ck_str_work(type);
    OSRestoreInterrupts(old);
    return ret;
}

// 4 when any stream work of `type` is in use.
int str_pro_ck_str_work(u8 type)
{
    SND_STR* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->be_flag != 0 && (type & str->str_type)) {
            return 4;
        }
    }
    return 0;
}

// ARAM buffers of stream slot `no`: left at `adr`, right 128 KB above (and their nibble addresses).
void Snd_str_aram_adrs_set(int no, u32 adr)
{
    SND_STR* str;

    str = &Snd_str_work[no];
    str->ar_buff_adrs_l = adr;
    str->ar_buff_adrs_r = str->ar_buff_adrs_l + 0x20000;
    str->ar_buff_nbl_l = str->ar_buff_adrs_l * 2;
    str->ar_buff_nbl_r = str->ar_buff_adrs_r * 2;
}
