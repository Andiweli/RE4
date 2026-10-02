// game/snd_main: sound driver core (Capcom sound library, -O0). Its audio-frame callback runs the ISS
// voice manager, stream player and MIDI sequencer every 5 ms.
#include "snd_drv.h"

u8 zero_tbl[0x100] __attribute__((aligned(32)));

// Driver start: AI / AX / mixer / articulation / synth / sequencer libraries, the work areas, the
// output mode from the OS setting, and the 5 ms audio-frame callback.
void Snd_system_init(void)
{
    AIInit(NULL);
    AXInitEx(1);
    MIXInit();
    AXARTInit();
    SYNInit();
    SEQInit();
    snd_work_clear();
    Snd_sound_mode_init();
    AXRegisterCallback(cb_audio_frame);
}

// Clears the driver control work: ARAM base (+0x100 for the zero table), request banks, random
// seed, compressor on, and all sub-works (requests, effects, AX voices, voices, sequences, streams, test).
void snd_work_clear(void)
{
    SND_CTRL* ctrl;
    u32 i;
    u8* p;

    p = (u8*) &Snd_ctrl_work;
    for (i = 0; i < sizeof(SND_CTRL); i++) {
        *p++ = 0;
    }
    ctrl = &Snd_ctrl_work;
    ctrl->zero_adrs = ARGetBaseAddress();
    ctrl->aram_adrs = ctrl->zero_adrs + 0x100;
    OSReport("A-RAM ADDRESS : %08XH\n", ctrl->aram_adrs);
    zero_buff_clear();
    ctrl->req_push_idx = 0;
    ctrl->req_exec_idx = 1;
    ctrl->random = 0xD37;
    AXSetCompressor(1);
    Snd_req_work_clear();
    Snd_efx_work_clear();
    Snd_axv_work_clear();
    Snd_voice_work_clear();
    Snd_seq_work_clear();
    Snd_str_work_clear();
    Snd_test_work_clear();
}

// t_movie/snd_test defines its own static cb_dma_end; the header must not declare this one.
void cb_dma_end(u32 task);

// Uploads 256 zero bytes to the ARAM base (the silent sample every idle voice points at) and
// waits for the DMA.
void zero_buff_clear(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    memclr_asm(zero_tbl, sizeof(zero_tbl));
    DCFlushRange(zero_tbl, sizeof(zero_tbl));
    ctrl->arq_flag = 1;
    ARQPostRequest(&ctrl->arq_req, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) zero_tbl, ctrl->zero_adrs,
                   sizeof(zero_tbl), cb_dma_end);
    while (1) {
        if (ctrl->arq_flag == 0) {
            break;
        }
    }
}

// ARQ callback: the zero-table upload finished.
void cb_dma_end(u32 task)
{
    Snd_ctrl_work.arq_flag = 0;
}

// AX audio-frame callback (every 5 ms): voice manager, stream refill, MIDI sequencer, then the
// SDK per-frame services; counts Snd_ctrl_work.frame.
void cb_audio_frame(void)
{
    int old;

    old = OSEnableInterrupts();
    Snd_iss_manager();
    Snd_stream_player();
    Snd_midi_sequencer();
    SEQRunAudioFrame();
    SYNRunAudioFrame();
    AXARTServiceSounds();
    MIXUpdateSettings();
    Snd_ctrl_work.audio_frame++;
    OSRestoreInterrupts(old);
}

// Output mode from the console setting (0 mono, 1 stereo).
void Snd_sound_mode_init(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    if (OSGetSoundMode() == 0) {
        ctrl->snd_mode = 0;
    } else {
        ctrl->snd_mode = 1;
    }
    snd_mode_set_ax_mix(ctrl);
}

// Applies the saved output mode: 2 (DPL2) only when the console is set to stereo. Returns the mode
// in effect.
u32 Snd_sound_mode_init_load(u32 mode)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    if (ctrl->snd_mode == 1 && mode == 2) {
        ctrl->snd_mode = 2;
    }
    snd_mode_set_ax_mix(ctrl);
    return ctrl->snd_mode;
}

// Current output mode (0 mono, 1 stereo, 2 DPL2).
u32 Snd_get_sound_mode(void)
{
    return Snd_ctrl_work.snd_mode;
}

// Sets the output mode and the console setting to match.
void Snd_set_sound_mode(u32 mode)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    ctrl->snd_mode = mode;
    if (mode == 0) {
        OSSetSoundMode(0);
    } else {
        OSSetSoundMode(1);
    }
    snd_mode_set_ax_mix(ctrl);
}

// AX / MIX modes for the output mode (DPL2 = AX mode 2, MIX 3).
void snd_mode_set_ax_mix(SND_CTRL* ctrl)
{
    switch (ctrl->snd_mode) {
    case 0:
        AXSetMode(0);
        MIXSetSoundMode(0);
        break;
    case 1:
        AXSetMode(0);
        MIXSetSoundMode(1);
        break;
    case 2:
        AXSetMode(2);
        MIXSetSoundMode(3);
        break;
    }
}

// Game-frame tick (from SndWatcher): random step, closes finished sequences / streams, load stats.
void Snd_iss_control(void)
{
    Snd_rnd();
    Snd_seq_work_close_check();
    Snd_str_work_close_check();
    Snd_dev_voice_ck();
}

// Statistics: DSP cycles, active voices / AX voices / stream channels / synth notes with their peaks.
void Snd_dev_voice_ck(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_SEQ* seq;
    int i;

    ctrl->dsp_cyc = AXGetMaxDspCycles();
    ctrl->now_cyc = AXGetDspCycles();
    if (ctrl->now_cyc >= ctrl->max_cyc) {
        ctrl->max_cyc = ctrl->now_cyc;
    }
    ctrl->now_voice = 0;
    for (i = 0; i < SND_VOICE_MAX; i++) {
        if (Snd_voice_work[i].be_flag != 0) {
            ctrl->now_voice++;
        }
    }
    if (ctrl->now_voice >= ctrl->max_voice) {
        ctrl->max_voice = ctrl->now_voice;
    }
    ctrl->now_axv_vo = 0;
    for (i = 0; i < SND_AXV_MAX; i++) {
        if (Snd_axv_work[i].be_flag != 0) {
            ctrl->now_axv_vo++;
        }
    }
    if (ctrl->now_axv_vo >= ctrl->max_axv_vo) {
        ctrl->max_axv_vo = ctrl->now_axv_vo;
    }
    ctrl->now_str_vo = 0;
    for (i = 0; i < SND_STR_MAX; i++) {
        if (Snd_str_work[i].ax_voice_l != NULL) {
            ctrl->now_str_vo++;
        }
        if (Snd_str_work[i].ax_voice_r != NULL) {
            ctrl->now_str_vo++;
        }
    }
    if (ctrl->now_str_vo >= ctrl->max_str_vo) {
        ctrl->max_str_vo = ctrl->now_str_vo;
    }
    ctrl->now_syn_vo = 0;
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->pcm_adrs != 0) {
            ctrl->now_syn_vo += SYNGetActiveNotes(&seq->synth);
        }
    }
    if (ctrl->now_syn_vo >= ctrl->max_syn_vo) {
        ctrl->max_syn_vo = ctrl->now_syn_vo;
    }
    ctrl->now_total_vo = ctrl->now_axv_vo + ctrl->now_str_vo + ctrl->now_syn_vo;
    if (ctrl->now_total_vo >= ctrl->max_total_vo) {
        ctrl->max_total_vo = ctrl->now_total_vo;
    }
}
