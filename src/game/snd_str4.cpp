// game/snd_str4: sound driver stream works (SND_STR), with their volume, pan and DVD error
// recovery, plus the setters the game uses from snd.cpp SndStrReq.
#include "snd_drv.h"

// Clears the 4 stream works (numbered).
void Snd_str_work_clear(void)
{
    SND_STR* str;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        p = (u8*) str;
        for (j = 0; j < sizeof(SND_STR); j++) {
            *p++ = 0;
        }
        str->work_id = i;
    }
}

// The active stream work with sound id `snd_id`, or NULL.
SND_STR* Snd_search_str_work_snd_id(u32 snd_id)
{
    SND_STR* str;
    int i;

    if (snd_id == 0) {
        return NULL;
    }
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->be_flag == 0) {
            continue;
        }
        if (str->snd_id != snd_id) {
            continue;
        }
        return str;
    }
    return NULL;
}

// Game-frame tick: frees streams that reached the closed state (5).
void Snd_str_work_close_check(void)
{
    SND_STR* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->be_flag == 0) {
            continue;
        }
        if (str->rno == 5) {
            str->be_flag = 0;
        }
    }
}

// AX volume from the system BGM (type 2) or SE volume x the stream master x the 8.8 volume.
void Snd_str_work_calc_ax_vol(SND_STR* str)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    if (str->str_type == 2) {
        str->out_vol = ctrl->vol_mas_se / 127 * (ctrl->vol_str_se >> 8);
    } else {
        str->out_vol = ctrl->vol_mas_bgm / 127 * (ctrl->vol_str_bgm >> 8);
    }
    str->out_vol = str->out_vol / 127 * (str->now_vol >> 8);
    str->ax_vol = Snd_vol_syn_to_ax((s16) (str->out_vol >> 8));
}

// Surround pan sent to MIX: the stream's in DPL2 mode, else 0x7F.
void Snd_str_work_choice_out_span(SND_STR* str)
{
    if (Snd_get_sound_mode() == 2) {
        str->out_span = str->srd_span;
    } else {
        str->out_span = 0x7F;
    }
}

// Reads the DVD command state: fatal (err 1), no disc / cover / wrong disc / retry (2), or more
// than 6 reads outstanding (4); any error sets status 0x8000 and reports the drive state in
// ctrl->dvd_err (the game shows the disc error screen).
void Snd_str_get_dvd_status(SND_STR* str)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    str->err_flag = 0;
    str->dvd_status = DVDGetCommandBlockStatus(&str->info.cb);
    switch (str->dvd_status) {
    case DVD_STATE_FATAL_ERROR:
        str->err_flag |= 0x1;
        break;
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
    case DVD_STATE_RETRY:
        str->err_flag |= 0x2;
        break;
    }
    if (str->dvd_req_num > 6) {
        str->err_flag |= 0x4;
    }
    if (str->err_flag == 0) {
        return;
    }
    str->be_flag |= 0x8000;
    if (ctrl->dvd_err_flag != -1) {
        switch (str->dvd_status) {
        case DVD_STATE_FATAL_ERROR:
        case DVD_STATE_NO_DISK:
        case DVD_STATE_COVER_OPEN:
        case DVD_STATE_WRONG_DISK:
        case DVD_STATE_RETRY:
            ctrl->dvd_err_flag = str->dvd_status;
            break;
        }
    }
}

// Playing with a DVD error: mutes, and once reads are pending stops the voices and enters the
// error state (6) with an error fade back to the current volume prepared for the recovery.
void Snd_str_err_check(SND_STR* str)
{
    s32 vol;

    vol = Snd_vol_syn_to_ax(0);
    if (str->ax_vol != vol) {
        str->ax_vol = vol;
        str->update |= 0x5;
    }
    if (str->dvd_req_num == 0) {
        return;
    }
    if ((str->be_flag & 0x200) == 0) {
        str->sys_fade_end = str->now_vol;
    }
    str->sys_fade_spd = str->sys_fade_end / 100;
    Snd_str_ax_voice_stop(str);
    str->rno_sv = str->rno;
    str->rno = 6;
}

// Pushes upd bits to MIX: 1 volume (recomputed unless 4), 2 pan / surround pan (stereo hard L / R).
void Snd_str_player_update(SND_STR* str)
{
    if (str->ax_voice_l == NULL) {
        str->update = 0;
        return;
    }
    if (str->update & 0x1) {
        if ((str->update & 0x4) == 0) {
            Snd_str_work_calc_ax_vol(str);
        }
        MIXSetInput(str->ax_voice_l, str->ax_vol);
        if (str->shd_flag & 0x1) {
            MIXSetInput(str->ax_voice_r, str->ax_vol);
        }
    }
    if (str->update & 0x2) {
        Snd_str_work_choice_out_span(str);
        if (str->shd_flag & 0x1) {
            MIXSetPan(str->ax_voice_l, 0);
            MIXSetPan(str->ax_voice_r, 0x7F);
            MIXSetSPan(str->ax_voice_l, str->out_span);
            MIXSetSPan(str->ax_voice_r, str->out_span);
        } else {
            MIXSetPan(str->ax_voice_l, str->pan);
            MIXSetSPan(str->ax_voice_l, str->out_span);
        }
    }
    str->update = 0;
}

// Before play: sets pan (flag 2), surround pan (4), volume (8), AUX A (0x20) / B (0x40) or the
// cancel mode (0x1000) of a prepared stream. 1 when unknown.
int Snd_str_init_para(u32 snd_id, s16 flag, s16 val)
{
    SND_STR* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 1;
    }
    if (flag & 0x2) {
        str->pan = val;
    }
    if (flag & 0x4) {
        str->span = val;
    }
    if (flag & 0x8) {
        str->vol = val;
    }
    if (flag & 0x20) {
        str->aux_a = val;
    }
    if (flag & 0x40) {
        str->aux_b = val;
    }
    if (flag & 0x1000) {
        str->recv_type = val;
    }
    return 0;
}

// Before play: start at block `pos` (read offset and play position moved).
int Snd_str_init_pos(u32 snd_id, u32 pos)
{
    SND_STR* str;

    str = Snd_search_str_work_snd_id(snd_id);
    if (str == NULL) {
        return 1;
    }
    str->file_pos = str->buff_one * pos;
    str->play_nbl = str->buff_size_nbl * pos;
    str->next_nbl = str->play_nbl + str->buff_size_nbl;
    str->ttl_play_idx = str->play_nbl / str->buff_size_nbl;
    return 0;
}

// Samples per ring block of a stream (32 KB stereo / 64 KB mono, 14 samples per 16 bytes) — used to
// turn a start time into a block number.
u32 Snd_str_get_buff_smp(u16 blk_no, u16 req_no)
{
    SND_SHD* shd;
    u32 size;
    u32 smp;

    shd = Snd_get_shd_adrs(blk_no, req_no);
    if (shd->flag & 0x1) {
        size = 0x8000;
    } else {
        size = 0x10000;
    }
    smp = size / 16 * 14;
    return smp;
}
