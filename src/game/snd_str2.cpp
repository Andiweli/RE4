// game/snd_str2: sound driver stream data flow, from DVD reads into the MRAM ring buffer to DMAs into
// the ARAM ring, with the AX voices' loop / end addresses re-programmed as the ring advances.
#include "snd_drv.h"

// Issues the next asynchronous DVD read (read_cnt pending blocks) into the MRAM buffer slot
// read_blk; at the end of the data a looping stream (flag 4) rewinds to the block holding the
// loop start, else read_done.
void Snd_str_dvd_read_sub(SND_STR_WORK* str)
{
    u32 blks;
    u8* dst;

    if (str->read_cnt == 0) {
        return;
    }
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->buff_blks == 1 && str->dma_busy != 0) {
        return;
    }
    str->read_cnt--;
    dst = str->buff;
    dst += str->read_blk * str->read_size;
    DVDReadAsyncPrio(&str->dvd, dst, str->read_size, str->aram + str->read_ofs, cb_dvd_read_end, 0);
    str->dvd_busy = 1;
    str->read_ofs += str->read_size;
    if (str->read_ofs < str->read_end) {
        return;
    }
    if (!(str->flag & 0x4)) {
        str->read_done = 1;
    } else {
        blks = (str->loop_start >> 1) / str->blk_half;
        str->read_ofs = blks * str->read_size;
    }
}

// Issues the next MRAM -> ARAM DMA (dma_cnt pending): the block at dma_blk into ARAM ring slot
// dma_aram_blk (left and right halves for stereo); the first block's predictor bytes are kept for
// the ring wrap.
void Snd_str_aram_dma_sub(SND_STR_WORK* str)
{
    u8* src;
    u32 ofs;

    if (str->dma_cnt == 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    if (str->buff_blks == 1 && str->dvd_busy != 0) {
        return;
    }
    str->dma_cnt--;
    src = str->buff;
    src += str->dma_blk * str->read_size;
    ofs = str->dma_aram_blk * str->blk_half;
    if (str->flag & 0x1) {
        ARQPostRequest(&str->arqL, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->aram_L + ofs,
                       str->blk_half, NULL);
        ARQPostRequest(&str->arqR, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) (src + 0x4000),
                       str->aram_R + ofs, str->blk_half, cb_aram_dma_end);
    } else {
        ARQPostRequest(&str->arqL, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->aram_L + ofs,
                       str->blk_half, cb_aram_dma_end);
    }
    str->dma_busy = 1;
    if (str->dma_aram_blk != 0) {
        return;
    }
    str->pred_L = src[0];
    if (str->flag & 0x2) {
        return;
    }
    str->pred_R = src[0x4000];
}

// DVD read callback: on success the block becomes the next DMA source and read_blk advances
// around the MRAM ring; errors / cancels leave the stream in the DVD-error state.
void cb_dvd_read_end(s32 result, DVDFileInfo* info)
{
    SND_STR_WORK* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (&str->dvd == info) {
            break;
        }
    }
    str->dvd_busy = 0;
    switch (result) {
    case -1:
    case -2:
    case -3:
        return;
    default:
        str->dma_blk = str->read_blk;
        str->read_blk++;
        if (str->read_blk == str->buff_blks) {
            str->read_blk = 0;
        }
        str->dma_cnt++;
    }
}

// ARQ callback: advances the ARAM ring slot; while buffering (before ready) the last slot filled
// marks the stream ready (status 2), else another read is requested.
void cb_aram_dma_end(u32 task)
{
    ARQRequest* req;
    SND_STR_WORK* str;
    int i;
    s8 last;

    req = (ARQRequest*) task;
    str = NULL;
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->flag & 0x1) {
            if (&str->arqR == req) {
                break;
            }
        } else {
            if (&str->arqL == req) {
                break;
            }
        }
    }
    str->dma_busy = 0;
    str->dma_last_blk = str->dma_aram_blk;
    str->dma_aram_blk++;
    if (str->dma_aram_blk == str->aram_blks) {
        str->dma_aram_blk = 0;
    }
    if (str->status & 0x2) {
        return;
    }
    if (str->shortflag & 0x1) {
        last = str->read_end / str->read_size - 1;
    } else {
        last = str->aram_blks - 1;
    }
    if (str->dma_last_blk == last) {
        str->status &= ~0x4;
        str->status |= 0x2;
    } else {
        str->read_cnt++;
    }
}

// Reads the left voice's current ARAM nibble address into play_nbl / play_blk; when the voice moved
// into a new ring block the loop / end addresses are re-programmed (str_ax_voice_to_next_block);
// play_pos = position in the stream.
void Snd_str_get_now_play_nbl(SND_STR_WORK* str)
{
    u32 cur;

    if (str->play_blk == -1) {
        return;
    }
    cur = *(u32*) &str->voiceL->pb.addr.currentAddressHi;
    str->play_nbl = cur - str->aram_L_nbl;
    str->prev_blk = str->play_blk;
    str->play_blk = str->play_nbl / str->blk_size;
    if (str->shortflag & 0x1) {
        str->play_pos = str->play_nbl;
        return;
    }
    if (str->play_blk != str->prev_blk) {
        str_ax_voice_to_next_block(str);
    }
    str->play_pos = str->blk_cnt * str->blk_size;
    str->play_pos = str->play_pos + str->play_nbl % str->blk_size;
}

// The voice entered the next ring block: advances blk_cnt / blk_end (after a loop jump, from the
// loop start); when the end block is reached sets the end address and either loops back to the
// stream's loop point or lets it run out; at the last ring slot points the loop at the ring top;
// requests another read while data remains.
void str_ax_voice_to_next_block(SND_STR_WORK* str)
{
    u32 ofs;

    if (str->loop_top != 0) {
        str->loop_top = 0;
        str->blk_cnt = str->loop_start / str->blk_size;
        str->blk_end = str->blk_cnt * str->blk_size;
    } else {
        str->blk_cnt++;
    }
    str->blk_end += str->blk_size;
    if (str->blk_end >= str->loop_end) {
        ofs = str->play_blk * str->blk_size;
        ofs += str->loop_end % str->blk_size;
        str->end_L = str->aram_L_nbl + ofs;
        str->end_R = str->aram_R_nbl + ofs;
        if (str->flag & 0x4) {
            str->read_cnt++;
            str_ax_voice_loop_to_top(str);
        } else {
            str_ax_voice_loop_to_end(str);
        }
        return;
    }
    if (str->play_blk == str->aram_blks - 1) {
        str_ax_voice_last_to_top(str);
    }
    if (str->read_done == 0) {
        str->read_cnt++;
    }
}

// End of a looping stream in the ring: the voices loop from end_L/R to the ring slot that holds
// the loop start (ADPCM loop state from the header), non-looping type until then.
void str_ax_voice_loop_to_top(SND_STR_WORK* str)
{
    SND_SHD* shd;
    AXPBADPCMLOOP loop;
    u32 ofs;
    s16 blk;

    shd = str->shd;
    str->loop_top = 1;
    blk = str->play_blk + 1;
    if (blk == str->aram_blks) {
        blk = 0;
    }
    ofs = blk * str->blk_size;
    ofs += str->loop_start % str->blk_size;
    str->loop_L = str->aram_L_nbl + ofs;
    str->loop_R = str->aram_R_nbl + ofs;
    loop.loop_pred_scale = shd->lps[0];
    loop.loop_yn1 = shd->lyn1[0];
    loop.loop_yn2 = shd->lyn2[0];
    AXSetVoiceAdpcmLoop(str->voiceL, &loop);
    AXSetVoiceType(str->voiceL, 0);
    AXSetVoiceLoopAddr(str->voiceL, str->loop_L);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = shd->lps[1];
    loop.loop_yn1 = shd->lyn1[1];
    loop.loop_yn2 = shd->lyn2[1];
    AXSetVoiceAdpcmLoop(str->voiceR, &loop);
    AXSetVoiceType(str->voiceR, 0);
    AXSetVoiceLoopAddr(str->voiceR, str->loop_R);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}

// End of a non-looping stream: voices become one-shot ending at end_L/R (state 3 no-read).
void str_ax_voice_loop_to_end(SND_STR_WORK* str)
{
    u32 zero;

    zero = Snd_ctrl_work.aram_base * 2 + 2;
    str->state = 3;
    AXSetVoiceLoop(str->voiceL, 0);
    AXSetVoiceLoopAddr(str->voiceL, zero);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    AXSetVoiceLoop(str->voiceR, 0);
    AXSetVoiceLoopAddr(str->voiceR, zero);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}

// The voice plays the last ring slot: loop back to the ring top with the saved predictor bytes,
// end at the ring's last nibble.
void str_ax_voice_last_to_top(SND_STR_WORK* str)
{
    AXPBADPCMLOOP loop;
    u32 len;

    str->loop_L = str->aram_L_nbl + 2;
    str->loop_R = str->aram_R_nbl + 2;
    if (str->flag & 0x1) {
        len = 0x3FFFF;
    } else {
        len = 0x7FFFF;
    }
    str->end_L = str->aram_L_nbl + len;
    str->end_R = str->aram_R_nbl + len;
    loop.loop_pred_scale = str->pred_L;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->voiceL, &loop);
    AXSetVoiceType(str->voiceL, 1);
    AXSetVoiceLoopAddr(str->voiceL, str->loop_L);
    AXSetVoiceEndAddr(str->voiceL, str->end_L);
    if (str->flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = str->pred_R;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->voiceR, &loop);
    AXSetVoiceType(str->voiceR, 1);
    AXSetVoiceLoopAddr(str->voiceR, str->loop_R);
    AXSetVoiceEndAddr(str->voiceR, str->end_R);
}
