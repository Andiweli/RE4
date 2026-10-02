// game/snd_sub1: sound driver voice works (SND_VOICE, 64 slots shared by SEs, sequence notes
// and streams). Without a free slot in its SIT range, a sound steals the oldest lowest-priority voice.
#include "snd_drv.h"

// Clears the 64 voice works (numbered, no sequence note).
void Snd_voice_work_clear(void)
{
    SND_VOICE* voice;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        p = (u8*) voice;
        for (j = 0; j < sizeof(SND_VOICE); j++) {
            *p++ = 0;
        }
        voice->work_id = i;
        voice->use_type = 0;
        voice->play_type = 0;
        voice->srd_type = 0;
        voice->seq_id = -1;
        voice->track = -1;
        voice->note = -1;
    }
}

// Stops the sound on a voice work: a sequence note gets a note-off on its synth, a SE a release
// on its AX voice.
void Snd_stop_voice_work(SND_VOICE* voice)
{
    SND_SEQ* seq;

    if (voice->play_type == 2) {
        seq = &Snd_seq_work[voice->seq_id];
        Snd_send_midi(&seq->synth, voice->track | (s8) 0x90, voice->note, 0);
        voice->be_flag = 0;
        voice->axv_ptr = NULL;
    } else {
        Snd_axv_work_note_off(voice->axv_ptr, voice->adsr_rel);
    }
}

// Takes voice work slot `no` for a stream channel (type 3, priority 0x7F); NULL when busy.
SND_VOICE* Snd_open_voice_work_str(SND_STR* str, s8 no)
{
    SND_VOICE* voice;

    voice = &Snd_voice_work[no];
    if (voice->be_flag != 0) {
        return NULL;
    }
    voice->snd_id = str->snd_id;
    if (str->str_type & 0x2) {
        voice->use_type = 2;
    } else {
        voice->use_type = 1;
    }
    voice->play_type = 3;
    voice->srd_type = 0;
    voice->timer = 0;
    voice->axv_ptr = NULL;
    voice->blk_no = -1;
    voice->req_no = -1;
    voice->vprio = 0x7F;
    voice->be_flag = 1;
    return voice;
}

// Takes a voice work for a sequence note in the sequence SIT's slot range at `prio` (type 2).
SND_VOICE* Snd_open_voice_work_seq(SND_SEQ* seq, s8 prio)
{
    SND_VOICE* voice;

    voice = Snd_voice_work_open_ck(seq->sit_ptr, prio);
    if (voice == NULL) {
        return NULL;
    }
    voice->snd_id = seq->snd_id;
    if (seq->seq_type == 2) {
        voice->use_type = 2;
    } else {
        voice->use_type = 1;
    }
    voice->play_type = 2;
    voice->srd_type = 0;
    voice->timer = 0;
    voice->axv_ptr = NULL;
    voice->blk_no = -1;
    voice->req_no = -1;
    voice->be_flag = 1;
    return voice;
}

// A voice work in the SIT's slot range: a free one, else the oldest of the lowest-priority voices
// is stopped and reused when its priority is below `prio` (equal only with SIT flag 0x4000), it is
// of the same kind (sequence / SE) and not already stopping. NULL when none may be taken.
SND_VOICE* Snd_voice_work_open_ck(SND_SIT* info, s8 prio)
{
    SND_VOICE* voice;
    SND_VOICE* found;
    int i;
    u32 count;
    s8 min_prio;

    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (voice->be_flag == 0) {
            return voice;
        }
    }
    min_prio = 0x7F;
    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (min_prio >= voice->vprio) {
            min_prio = voice->vprio;
        }
    }
    if (min_prio > prio) {
        return NULL;
    }
    if (min_prio == prio && !(info->flag & 0x4000)) {
        return NULL;
    }
    found = NULL;
    count = 0;
    for (i = 0; i <= info->voice_num; i++) {
        voice = &Snd_voice_work[info->voice_start + i];
        if (min_prio != voice->vprio) {
            continue;
        }
        if (count <= voice->timer) {
            found = voice;
            count = voice->timer;
        }
    }
    if (found == NULL) {
        return NULL;
    }
    if (info->flag & 0x4) {
        if (found->play_type != 2) {
            return NULL;
        }
    } else {
        if (found->play_type != 1) {
            return NULL;
        }
    }
    if (found->be_flag & 0x2) {
        return NULL;
    }
    Snd_stop_voice_work(found);
    return found;
}

// The SE voice work with sound id `snd_id`, or NULL.
SND_VOICE* Snd_search_voice_work_snd_id(u32 snd_id)
{
    SND_VOICE* voice;
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        if (voice->be_flag == 0) {
            continue;
        }
        if (voice->play_type != 1) {
            continue;
        }
        if (voice->snd_id == snd_id) {
            return voice;
        }
    }
    return NULL;
}

// The voice work of a sequence's note on channel `ch`, or NULL.
SND_VOICE* Snd_search_voice_work_seq(SND_SEQ* seq, u8 ch, u8 note)
{
    SND_VOICE* voice;
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        if (voice->be_flag == 0) {
            continue;
        }
        if (voice->play_type != 2) {
            continue;
        }
        if ((s16) voice->seq_id != seq->work_id) {
            continue;
        }
        if (voice->track != ch) {
            continue;
        }
        if (voice->note != note) {
            continue;
        }
        return voice;
    }
    return NULL;
}
