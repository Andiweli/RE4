// game/snd_str2: sound driver stream data flow, from DVD reads into the MRAM ring buffer to DMAs into
// the ARAM ring, with the AX voices' loop / end addresses re-programmed as the ring advances.
#include "snd_drv.h"

// Issues the next asynchronous DVD read (read_cnt pending blocks) into the MRAM buffer slot
// read_blk; at the end of the data a looping stream (flag 4) rewinds to the block holding the
// loop start, else read_done.
void Snd_str_dvd_read_sub(SND_STR* str)
{
    u32 blks;
    u8* dst;

    if (str->dvd_req_num == 0) {
        return;
    }
    if (str->dvd_busy != 0) {
        return;
    }
    if (str->dvd_req_max == 1 && str->dma_busy != 0) {
        return;
    }
    str->dvd_req_num--;
    dst = str->buff_ptr;
    dst += str->dvd_req_idx * str->buff_one;
    DVDReadAsyncPrio(&str->info, dst, str->buff_one, str->file_offset + str->file_pos, cb_dvd_read_end, 0);
    str->dvd_busy = 1;
    str->file_pos += str->buff_one;
    if (str->file_pos < str->file_size) {
        return;
    }
    if (!(str->shd_flag & 0x4)) {
        str->dvd_comp = 1;
    } else {
        blks = (str->lptop_nbl >> 1) / str->buff_size;
        str->file_pos = blks * str->buff_one;
    }
}

// Issues the next MRAM -> ARAM DMA (dma_cnt pending): the block at dma_blk into ARAM ring slot
// dma_aram_blk (left and right halves for stereo); the first block's predictor bytes are kept for
// the ring wrap.
void Snd_str_aram_dma_sub(SND_STR* str)
{
    u8* src;
    u32 ofs;

    if (str->dma_req_num == 0) {
        return;
    }
    if (str->dma_busy != 0) {
        return;
    }
    if (str->dvd_req_max == 1 && str->dvd_busy != 0) {
        return;
    }
    str->dma_req_num--;
    src = str->buff_ptr;
    src += str->dvd_end_idx * str->buff_one;
    ofs = str->dma_req_idx * str->buff_size;
    if (str->shd_flag & 0x1) {
        ARQPostRequest(&str->arq_req_l, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->ar_buff_adrs_l + ofs,
                       str->buff_size, NULL);
        ARQPostRequest(&str->arq_req_r, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) (src + 0x4000),
                       str->ar_buff_adrs_r + ofs, str->buff_size, cb_aram_dma_end);
    } else {
        ARQPostRequest(&str->arq_req_l, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) src, str->ar_buff_adrs_l + ofs,
                       str->buff_size, cb_aram_dma_end);
    }
    str->dma_busy = 1;
    if (str->dma_req_idx != 0) {
        return;
    }
    str->ax_ps_l = src[0];
    if (str->shd_flag & 0x2) {
        return;
    }
    str->ax_ps_r = src[0x4000];
}

// DVD read callback: on success the block becomes the next DMA source and read_blk advances
// around the MRAM ring; errors / cancels leave the stream in the DVD-error state.
void cb_dvd_read_end(s32 result, DVDFileInfo* info)
{
    SND_STR* str;
    int i;

    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (&str->info == info) {
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
        str->dvd_end_idx = str->dvd_req_idx;
        str->dvd_req_idx++;
        if (str->dvd_req_idx == str->dvd_req_max) {
            str->dvd_req_idx = 0;
        }
        str->dma_req_num++;
    }
}

// ARQ callback: advances the ARAM ring slot; while buffering (before ready) the last slot filled
// marks the stream ready (status 2), else another read is requested.
void cb_aram_dma_end(u32 task)
{
    ARQRequest* req;
    SND_STR* str;
    int i;
    s8 last;

    req = (ARQRequest*) task;
    str = NULL;
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->shd_flag & 0x1) {
            if (&str->arq_req_r == req) {
                break;
            }
        } else {
            if (&str->arq_req_l == req) {
                break;
            }
        }
    }
    str->dma_busy = 0;
    str->dma_end_idx = str->dma_req_idx;
    str->dma_req_idx++;
    if (str->dma_req_idx == str->dma_req_max) {
        str->dma_req_idx = 0;
    }
    if (str->be_flag & 0x2) {
        return;
    }
    if (str->short_flag & 0x1) {
        last = str->file_size / str->buff_one - 1;
    } else {
        last = str->dma_req_max - 1;
    }
    if (str->dma_end_idx == last) {
        str->be_flag &= ~0x4;
        str->be_flag |= 0x2;
    } else {
        str->dvd_req_num++;
    }
}

// Reads the left voice's current ARAM nibble address into play_nbl / play_blk; when the voice moved
// into a new ring block the loop / end addresses are re-programmed (str_ax_voice_to_next_block);
// play_pos = position in the stream.
void Snd_str_get_now_play_nbl(SND_STR* str)
{
    u32 cur;

    if (str->now_play_idx == -1) {
        return;
    }
    cur = *(u32*) &str->ax_voice_l->pb.addr.currentAddressHi;
    str->aram_nbl = cur - str->ar_buff_nbl_l;
    str->old_play_idx = str->now_play_idx;
    str->now_play_idx = str->aram_nbl / str->buff_size_nbl;
    if (str->short_flag & 0x1) {
        str->play_nbl = str->aram_nbl;
        return;
    }
    if (str->now_play_idx != str->old_play_idx) {
        str_ax_voice_to_next_block(str);
    }
    str->play_nbl = str->ttl_play_idx * str->buff_size_nbl;
    str->play_nbl = str->play_nbl + str->aram_nbl % str->buff_size_nbl;
}

// The voice entered the next ring block: advances blk_cnt / blk_end (after a loop jump, from the
// loop start); when the end block is reached sets the end address and either loops back to the
// stream's loop point or lets it run out; at the last ring slot points the loop at the ring top;
// requests another read while data remains.
void str_ax_voice_to_next_block(SND_STR* str)
{
    u32 ofs;

    if (str->loop_flag != 0) {
        str->loop_flag = 0;
        str->ttl_play_idx = str->lptop_nbl / str->buff_size_nbl;
        str->next_nbl = str->ttl_play_idx * str->buff_size_nbl;
    } else {
        str->ttl_play_idx++;
    }
    str->next_nbl += str->buff_size_nbl;
    if (str->next_nbl >= str->lpend_nbl) {
        ofs = str->now_play_idx * str->buff_size_nbl;
        ofs += str->lpend_nbl % str->buff_size_nbl;
        str->ax_lpend_l = str->ar_buff_nbl_l + ofs;
        str->ax_lpend_r = str->ar_buff_nbl_r + ofs;
        if (str->shd_flag & 0x4) {
            str->dvd_req_num++;
            str_ax_voice_loop_to_top(str);
        } else {
            str_ax_voice_loop_to_end(str);
        }
        return;
    }
    if (str->now_play_idx == str->dma_req_max - 1) {
        str_ax_voice_last_to_top(str);
    }
    if (str->dvd_comp == 0) {
        str->dvd_req_num++;
    }
}

// End of a looping stream in the ring: the voices loop from end_L/R to the ring slot that holds
// the loop start (ADPCM loop state from the header), non-looping type until then.
void str_ax_voice_loop_to_top(SND_STR* str)
{
    SND_SHD* shd;
    AXPBADPCMLOOP loop;
    u32 ofs;
    s16 blk;

    shd = str->shd_adrs;
    str->loop_flag = 1;
    blk = str->now_play_idx + 1;
    if (blk == str->dma_req_max) {
        blk = 0;
    }
    ofs = blk * str->buff_size_nbl;
    ofs += str->lptop_nbl % str->buff_size_nbl;
    str->ax_lptop_l = str->ar_buff_nbl_l + ofs;
    str->ax_lptop_r = str->ar_buff_nbl_r + ofs;
    loop.loop_pred_scale = shd->lps[0];
    loop.loop_yn1 = shd->lyn1[0];
    loop.loop_yn2 = shd->lyn2[0];
    AXSetVoiceAdpcmLoop(str->ax_voice_l, &loop);
    AXSetVoiceType(str->ax_voice_l, 0);
    AXSetVoiceLoopAddr(str->ax_voice_l, str->ax_lptop_l);
    AXSetVoiceEndAddr(str->ax_voice_l, str->ax_lpend_l);
    if (str->shd_flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = shd->lps[1];
    loop.loop_yn1 = shd->lyn1[1];
    loop.loop_yn2 = shd->lyn2[1];
    AXSetVoiceAdpcmLoop(str->ax_voice_r, &loop);
    AXSetVoiceType(str->ax_voice_r, 0);
    AXSetVoiceLoopAddr(str->ax_voice_r, str->ax_lptop_r);
    AXSetVoiceEndAddr(str->ax_voice_r, str->ax_lpend_r);
}

// End of a non-looping stream: voices become one-shot ending at end_L/R (state 3 no-read).
void str_ax_voice_loop_to_end(SND_STR* str)
{
    u32 zero;

    zero = Snd_ctrl_work.zero_adrs * 2 + 2;
    str->rno = 3;
    AXSetVoiceLoop(str->ax_voice_l, 0);
    AXSetVoiceLoopAddr(str->ax_voice_l, zero);
    AXSetVoiceEndAddr(str->ax_voice_l, str->ax_lpend_l);
    if (str->shd_flag & 0x2) {
        return;
    }
    AXSetVoiceLoop(str->ax_voice_r, 0);
    AXSetVoiceLoopAddr(str->ax_voice_r, zero);
    AXSetVoiceEndAddr(str->ax_voice_r, str->ax_lpend_r);
}

// The voice plays the last ring slot: loop back to the ring top with the saved predictor bytes,
// end at the ring's last nibble.
void str_ax_voice_last_to_top(SND_STR* str)
{
    AXPBADPCMLOOP loop;
    u32 len;

    str->ax_lptop_l = str->ar_buff_nbl_l + 2;
    str->ax_lptop_r = str->ar_buff_nbl_r + 2;
    if (str->shd_flag & 0x1) {
        len = 0x3FFFF;
    } else {
        len = 0x7FFFF;
    }
    str->ax_lpend_l = str->ar_buff_nbl_l + len;
    str->ax_lpend_r = str->ar_buff_nbl_r + len;
    loop.loop_pred_scale = str->ax_ps_l;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->ax_voice_l, &loop);
    AXSetVoiceType(str->ax_voice_l, 1);
    AXSetVoiceLoopAddr(str->ax_voice_l, str->ax_lptop_l);
    AXSetVoiceEndAddr(str->ax_voice_l, str->ax_lpend_l);
    if (str->shd_flag & 0x2) {
        return;
    }
    loop.loop_pred_scale = str->ax_ps_r;
    loop.loop_yn1 = 0;
    loop.loop_yn2 = 0;
    AXSetVoiceAdpcmLoop(str->ax_voice_r, &loop);
    AXSetVoiceType(str->ax_voice_r, 1);
    AXSetVoiceLoopAddr(str->ax_voice_r, str->ax_lptop_r);
    AXSetVoiceEndAddr(str->ax_voice_r, str->ax_lpend_r);
}
