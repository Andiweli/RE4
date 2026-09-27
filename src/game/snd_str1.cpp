// game/snd_str1: sound driver stream player state machine, audio-frame side. Snd_stream_player
// runs every stream's state after checking the DVD status, then updates the AX voices.
#include "snd_drv.h"

typedef void (*SND_STR_PLAYER)(SND_STR_WORK*);

// Audio frame: for every active stream — aborting ones (status 0x3000) run the abort states; the
// others check the DVD result, handle requests, run their state function and push the AX updates.
// A driver reset flags every stream for cancel.
void Snd_stream_player(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_STR_WORK* str;
    u32 i;
    static SND_STR_PLAYER str_player_tbl[] = {
        str_player_idle,   str_player_ready, str_player_normal, str_player_noread, str_player_close,
        str_player_idle,   str_player_error, str_abort_init,    str_abort_wait,
    };

    if (ctrl->dvd_err != -1) {
        ctrl->dvd_err = 0;
    }
    if (ctrl->reset_flag & 0x1) {
        ctrl->reset_flag |= 0x20;
    }
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->status != 0) {
            if (str->status & 0x3000) {
                if (str->status & 0x4000) {
                    str->state = 7;
                    str->status &= ~0x4000;
                }
                str_player_tbl[str->state](str);
            } else {
                Snd_str_get_dvd_status(str);
                str_req_check(str);
                str_player_tbl[str->state](str);
                Snd_str_player_update(str);
            }
        }
    }
}

// State 0: nothing (a reset request is honoured).
void str_player_idle(SND_STR_WORK* str)
{
    if (str->state != 0) {
        return;
    }
    str_reset_check(str);
}

// State 1 (buffering before play): reads and DMAs blocks; a DVD error waits for recovery or
// cancels; a stop request cancels.
void str_player_ready(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str->status & 0x8000) {
        if (str->cancel == 1) {
            str_play_cancel(str);
            return;
        }
        str_recovery_check(str);
    }
    if (str->req & 0x8) {
        str_play_cancel(str);
        return;
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

// State 2 (playing): fade step, tracks the play position, keeps reading / DMAing; on a DVD error
// checks for underrun or cancels.
void str_player_normal(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str_fade_check(str) != 0) {
        return;
    }
    Snd_str_get_now_play_nbl(str);
    if (str->status & 0x8000) {
        if (str->cancel == 1) {
            str_play_cancel(str);
        } else {
            Snd_str_err_check(str);
        }
        return;
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

// State 3 (short stream, all data resident): fade step, ends when the left voice stops.
void str_player_noread(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    if (str_fade_check(str) != 0) {
        return;
    }
    if (str->voiceL->pb.state == 0) {
        str_play_end(str);
        return;
    }
    Snd_str_get_now_play_nbl(str);
}

// State 4: once DVD and DMA are idle, closes the file and frees the AX voices (state 5 = done).
void str_player_close(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    DVDClose(&str->dvd);
    Snd_str_ax_voice_free(str);
    str->state = 5;
    str->upd = 0;
}

// State 6 (DVD error): waits for the drive to recover, then restores the voices and the previous
// state (or cancels when asked); keeps reading / DMAing.
void str_player_error(SND_STR_WORK* str)
{
    if (str_reset_check(str) != 0) {
        return;
    }
    str_recovery_check(str);
    if (!(str->status & 0x8000)) {
        if (str->cancel == 2) {
            str_play_cancel(str);
            return;
        }
        str->state = str->prev_state;
        Snd_str_ax_voice_recovery(str);
        Snd_str_get_now_play_nbl(str);
    }
    Snd_str_dvd_read_sub(str);
    Snd_str_aram_dma_sub(str);
}

// Abort step 1: cancels the DVD read and frees the voices / voice works.
void str_abort_init(SND_STR_WORK* str)
{
    SND_VOICE_WORK* vw;

    str->state++;
    if (str->dvd_busy != 0) {
        DVDCancelAsync(&str->dvd.cb, NULL);
    }
    if (!(str->status & 0x1000)) {
        if (str->status & 0x20) {
            MIXReleaseChannel(str->voiceL);
        }
        AXFreeVoice(str->voiceL);
        str->voiceL = NULL;
        vw = str->vwL;
        vw->status = 0;
    }
    if (str->flag & 0x2) {
        return;
    }
    if (!(str->status & 0x2000)) {
        if (str->status & 0x20) {
            MIXReleaseChannel(str->voiceR);
        }
        AXFreeVoice(str->voiceR);
        str->voiceR = NULL;
        vw = str->vwR;
        vw->status = 0;
    }
}

// Abort step 2: once DVD and DMA are idle, closes the file and clears the work.
void str_abort_wait(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    DVDClose(&str->dvd);
    str->status = 0;
    str->snd_id = 0;
    str->state = 0;
    str->prev_state = 0;
    str->upd = 0;
    str->vwL = NULL;
    str->vwR = NULL;
    str->voiceL = NULL;
    str->voiceR = NULL;
}

// Executes the stream's pending request in order: 1 to ready, 2 play (once ready), then while
// playing 0x10 set volume, 4 fade, 8 stop (50-step fade). Not during a reset or DVD error.
void str_req_check(SND_STR_WORK* str)
{
    if (Snd_ctrl_work.reset_flag & 0x20) {
        return;
    }
    if (str->status & 0x8000) {
        return;
    }
    if (str->req == 0) {
        return;
    }
    if (str->req & 0x1) {
        str_req_to_ready(str);
        return;
    }
    if ((str->status & 0x2) == 0) {
        return;
    }
    if (str->req & 0x2) {
        str_req_to_play(str);
        return;
    }
    if ((str->status & 0x10) == 0) {
        return;
    }
    if (str->req & 0x10) {
        str->req &= ~0x10;
        str_req_vol_set(str);
        return;
    }
    if (str->req & 0x4) {
        str->req &= ~0x4;
        str_req_nml_fade_set(str, str->fade_time, str->fade_vol);
        return;
    }
    if (str->req & 0x8) {
        str->req &= ~0x8;
        str_req_nml_fade_set(str, 50, 0);
    }
}

// Request 1: start buffering (state 1, status bit2).
void str_req_to_ready(SND_STR_WORK* str)
{
    str->req &= ~0x1;
    str->status |= 0x4;
    str->state = 1;
}

// Request 2: starts the AX voices on block 0; state 2, or 3 for a fully buffered short stream.
void str_req_to_play(SND_STR_WORK* str)
{
    str->req &= ~0x2;
    str->play_blk = 0;
    Snd_str_ax_voice_play(str);
    if (str->shortflag & 0x1) {
        str->state = 3;
    } else {
        str->state = 2;
    }
    str->status |= 0x30;
}

// Request 0x10: volume at once.
void str_req_vol_set(SND_STR_WORK* str)
{
    str->vol2 = str->req_vol << 8;
    str->upd |= 0x1;
}

// Starts a fade of the 8.8 volume to `vol` in `time` steps (cancels an error fade); status 0x100.
void str_req_nml_fade_set(SND_STR_WORK* str, s16 time, s16 vol)
{
    s16 diff;

    if (str->status & 0x200) {
        str->status &= ~0x200;
    }
    str->fade_target = vol << 8;
    diff = str->fade_target - str->vol2;
    str->fade_step = diff / time;
    if (str->fade_step == 0) {
        if (diff > 0) {
            str->fade_step = 1;
        } else {
            str->fade_step = -1;
        }
    }
    str->status |= 0x100;
}

// One step of the error fade (status 0x200) or the normal fade (0x100); a fade to 0 ends the
// stream (returns 1).
int str_fade_check(SND_STR_WORK* str)
{
    if (str->status & 0x200) {
        if (str->vol2 == str->err_target) {
            str->status &= ~0x200;
            if (str->vol2 == 0) {
                str_play_end(str);
                return 1;
            }
        } else {
            str_fade_new_vol_set(str, str->err_step, str->err_target);
        }
        return 0;
    }
    if (str->status & 0x100) {
        if (str->vol2 == str->fade_target) {
            str->status &= ~0x100;
            if (str->vol2 == 0) {
                str_play_end(str);
                return 1;
            }
        } else {
            str_fade_new_vol_set(str, str->fade_step, str->fade_target);
        }
    }
    return 0;
}

// Moves vol2 by `step` toward `target`, flags the volume update.
void str_fade_new_vol_set(SND_STR_WORK* str, s16 step, s16 target)
{
    int v;

    v = str->vol2 + step;
    if (step > 0) {
        if (v > target) {
            v = target;
        }
    } else {
        if (v < target) {
            v = target;
        }
    }
    str->vol2 = v;
    str->upd |= 0x1;
}

// During a driver reset the stream is cancelled (returns 1).
int str_reset_check(SND_STR_WORK* str)
{
    if (Snd_ctrl_work.reset_flag & 0x20) {
        str_play_cancel(str);
        return 1;
    } else {
        return 0;
    }
}

// Cancels a pending DVD read and ends the stream.
void str_play_cancel(SND_STR_WORK* str)
{
    if (str->dvd_busy != 0) {
        DVDCancelAsync(&str->dvd.cb, NULL);
    }
    str_play_end(str);
}

// Stops the AX voices; state 4 (close), playing bit off.
void str_play_end(SND_STR_WORK* str)
{
    Snd_str_ax_voice_stop(str);
    str->status &= ~0x10;
    str->state = 4;
}

// Clears the DVD error bit once the drive is idle and the failed read has been re-issued.
void str_recovery_check(SND_STR_WORK* str)
{
    if (str->err != 0) {
        return;
    }
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->read_cnt != 0) {
        return;
    }
    str->status &= 0x7FFF;
}
