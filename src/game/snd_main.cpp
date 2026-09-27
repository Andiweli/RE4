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
    SND_CTRL_WORK* ctrl;
    u32 i;
    u8* p;

    p = (u8*) &Snd_ctrl_work;
    for (i = 0; i < sizeof(SND_CTRL_WORK); i++) {
        *p++ = 0;
    }
    ctrl = &Snd_ctrl_work;
    ctrl->aram_base = ARGetBaseAddress();
    ctrl->aram_free = ctrl->aram_base + 0x100;
    OSReport("A-RAM ADDRESS : %08XH\n", ctrl->aram_free);
    zero_buff_clear();
    ctrl->req_bank = 0;
    ctrl->req_bank_sub = 1;
    ctrl->rnd = 0xD37;
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
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    memclr_asm(zero_tbl, sizeof(zero_tbl));
    DCFlushRange(zero_tbl, sizeof(zero_tbl));
    ctrl->dma_busy = 1;
    ARQPostRequest(&ctrl->arq, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32) zero_tbl, ctrl->aram_base,
                   sizeof(zero_tbl), cb_dma_end);
    while (1) {
        if (ctrl->dma_busy == 0) {
            break;
        }
    }
}

// ARQ callback: the zero-table upload finished.
void cb_dma_end(u32 task)
{
    Snd_ctrl_work.dma_busy = 0;
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
    Snd_ctrl_work.frame++;
    OSRestoreInterrupts(old);
}

// Output mode from the console setting (0 mono, 1 stereo).
void Snd_sound_mode_init(void)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    if (OSGetSoundMode() == 0) {
        ctrl->sound_mode = 0;
    } else {
        ctrl->sound_mode = 1;
    }
    snd_mode_set_ax_mix(ctrl);
}

// Applies the saved output mode: 2 (DPL2) only when the console is set to stereo. Returns the mode
// in effect.
u32 Snd_sound_mode_init_load(u32 mode)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    if (ctrl->sound_mode == 1 && mode == 2) {
        ctrl->sound_mode = 2;
    }
    snd_mode_set_ax_mix(ctrl);
    return ctrl->sound_mode;
}

// Current output mode (0 mono, 1 stereo, 2 DPL2).
u32 Snd_get_sound_mode(void)
{
    return Snd_ctrl_work.sound_mode;
}

// Sets the output mode and the console setting to match.
void Snd_set_sound_mode(u32 mode)
{
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;

    ctrl->sound_mode = mode;
    if (mode == 0) {
        OSSetSoundMode(0);
    } else {
        OSSetSoundMode(1);
    }
    snd_mode_set_ax_mix(ctrl);
}

// AX / MIX modes for the output mode (DPL2 = AX mode 2, MIX 3).
void snd_mode_set_ax_mix(SND_CTRL_WORK* ctrl)
{
    switch (ctrl->sound_mode) {
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
    SND_CTRL_WORK* ctrl = &Snd_ctrl_work;
    SND_SEQ_WORK* seq;
    int i;

    ctrl->dsp_cycles_max = AXGetMaxDspCycles();
    ctrl->dsp_cycles = AXGetDspCycles();
    if (ctrl->dsp_cycles >= ctrl->dsp_cycles_peak) {
        ctrl->dsp_cycles_peak = ctrl->dsp_cycles;
    }
    ctrl->voice_num = 0;
    for (i = 0; i < SND_VOICE_MAX; i++) {
        if (Snd_voice_work[i].status != 0) {
            ctrl->voice_num++;
        }
    }
    if (ctrl->voice_num >= ctrl->voice_peak) {
        ctrl->voice_peak = ctrl->voice_num;
    }
    ctrl->axv_num = 0;
    for (i = 0; i < SND_AXV_MAX; i++) {
        if (Snd_axv_work[i].status != 0) {
            ctrl->axv_num++;
        }
    }
    if (ctrl->axv_num >= ctrl->axv_peak) {
        ctrl->axv_peak = ctrl->axv_num;
    }
    ctrl->str_num = 0;
    for (i = 0; i < SND_STR_MAX; i++) {
        if (Snd_str_work[i].voiceL != NULL) {
            ctrl->str_num++;
        }
        if (Snd_str_work[i].voiceR != NULL) {
            ctrl->str_num++;
        }
    }
    if (ctrl->str_num >= ctrl->str_peak) {
        ctrl->str_peak = ctrl->str_num;
    }
    ctrl->seq_num = 0;
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->aram != 0) {
            ctrl->seq_num += SYNGetActiveNotes(&seq->synth);
        }
    }
    if (ctrl->seq_num >= ctrl->seq_peak) {
        ctrl->seq_peak = ctrl->seq_num;
    }
    ctrl->total_num = ctrl->axv_num + ctrl->str_num + ctrl->seq_num;
    if (ctrl->total_num >= ctrl->total_peak) {
        ctrl->total_peak = ctrl->total_num;
    }
}
