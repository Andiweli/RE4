// game/snd_seq0: sound driver sequence (MIDI BGM) requests from the game side — fade / stop /
// volume requests are set as bits on the SND_SEQ for the audio frame (snd_seq1) to execute,
// plus the fade / end / sounding checks.
#include "snd_drv.h"

// Request on sequence `snd_id`: cmd bit0 fade to `vol` over `time` (5 ms steps), bit1 stop (quick
// fade), bit2 set volume `time` at once. Returns 1 when the sequence is unknown.
int Snd_seq_req(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = seq_req_sub(snd_id, cmd, time, vol);
    OSRestoreInterrupts(old);
    return ret;
}

// Stores the request bits / fade parameters on the sequence work.
int seq_req_sub(u32 snd_id, u32 cmd, u32 time, u32 vol)
{
    SND_SEQ* seq;

    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return 1;
    }
    seq->req_flag |= cmd;
    if (cmd & 0x1) {
        seq->req_fade_time = time;
        seq->req_fade_end = vol;
    }
    if (cmd & 0x4) {
        seq->req_vol = time;
    }
    return 0;
}

// Marks every sequence of `type` for a volume recomputation (master volume changed).
void Snd_seq_reset_vol_type(u8 type)
{
    seq_type_sub(type, 0, 0);
}

// Fades every sequence of `type` out over `time` (0 = stop at once).
void Snd_seq_fade_out_type(u8 type, s16 time)
{
    seq_type_sub(type, 1, time);
}

// For every running sequence whose type matches: mode 0 volume refresh, 1 fade-out / stop request.
void seq_type_sub(u8 type, int mode, s16 time)
{
    SND_SEQ* seq;
    int old;
    int i;

    old = OSDisableInterrupts();
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag == 0) {
            continue;
        }
        if (!(seq->seq_type & type)) {
            continue;
        }
        switch (mode) {
        case 0:
            seq->update |= 0x1;
            break;
        case 1:
            if (time == 0) {
                seq->req_flag |= 0x2;
            } else {
                seq->req_flag |= 0x1;
                seq->req_fade_time = time;
                seq->req_fade_end = 0;
            }
            break;
        }
    }
    OSRestoreInterrupts(old);
}

// 1 while the sequence is fading, 0 when steady, -1 when unknown.
int Snd_seq_fade_check(u32 snd_id)
{
    SND_SEQ* seq;

    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return -1;
    }
    if (seq->be_flag & 0x100) {
        return 1;
    } else {
        return 0;
    }
}

// -1 while the sequence is still queued, 1 while it plays, 0 when gone.
int Snd_seq_end_check(u32 snd_id)
{
    SND_REQ* req;
    SND_SEQ* seq;
    int old;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 3);
    OSRestoreInterrupts(old);
    if (req != NULL) {
        return -1;
    }
    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq == NULL) {
        return 0;
    }
    return 1;
}

// Non-zero while a sequence of `type` is queued (0x10) or playing (2).
int Snd_seq_pronounce_ck_type(u8 type)
{
    SND_CTRL_WORK* ctrl;
    int old;
    int ret;

    old = OSDisableInterrupts();
    ctrl = &Snd_ctrl_work;
    ret = 0;
    ret |= seq_pro_ck_req_work(ctrl->req_bank, type);
    ret |= seq_pro_ck_req_work(ctrl->req_bank_sub, type);
    OSRestoreInterrupts(old);
    old = OSDisableInterrupts();
    ret |= seq_pro_ck_seq_work(type);
    OSRestoreInterrupts(old);
    return ret;
}

// 0x10 when the request bank holds a pending sequence play of `type`.
int seq_pro_ck_req_work(int bank, u8 type)
{
    SND_REQ* req;
    int i;

    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[bank][i];
        if (req->be_flag == 0) {
            continue;
        }
        if (req->use_type & 0x4) {
            continue;
        }
        if (!(req->use_type & type)) {
            continue;
        }
        if (req->sit_ptr->flag & 0x4) {
            return 0x10;
        }
    }
    return 0;
}

// 2 when a sequence of `type` is active.
int seq_pro_ck_seq_work(u8 type)
{
    SND_SEQ* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag != 0 && (type & seq->seq_type)) {
            return 2;
        }
    }
    return 0;
}
