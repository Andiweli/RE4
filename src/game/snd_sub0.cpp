// game/snd_sub0: sound driver shared helpers: volume and filter tables, the system volumes, the
// random generator and the sound-test work reset.
#include "snd_drv.h"

s32 Snd_dls_vol_tbl[128] = {
    -904, -842, -721, -651, -601, -562, -530, -503,
    -480, -460, -442, -425, -410, -396, -383, -371,
    -360, -349, -339, -330, -321, -313, -305, -297,
    -289, -282, -276, -269, -263, -257, -251, -245,
    -239, -234, -229, -224, -219, -214, -210, -205,
    -201, -196, -192, -188, -184, -180, -176, -173,
    -169, -165, -162, -158, -155, -152, -149, -145,
    -142, -139, -136, -133, -130, -127, -125, -122,
    -119, -116, -114, -111, -109, -106, -103, -101,
    -99, -96, -94, -91, -89, -87, -85, -82,
    -80, -78, -76, -74, -72, -70, -68, -66,
    -64, -62, -60, -58, -56, -54, -52, -50,
    -49, -47, -45, -43, -42, -40, -38, -36,
    -35, -33, -31, -30, -28, -27, -25, -23,
    -22, -20, -19, -17, -16, -14, -13, -11,
    -10, -8, -7, -6, -4, -3, -1, 0,
};

SND_LPF Snd_lpf_tbl[24] = {
    {0x6A09, 0x15F6, "16000"},
    {0x6871, 0x178E, "12800"},
    {0x6463, 0x1B9C, "10240"},
    {0x5DB3, 0x224C, " 8000"},
    {0x5618, 0x29E7, " 6400"},
    {0x4D7A, 0x3285, " 5120"},
    {0x4367, 0x3C98, " 4000"},
    {0x3A5A, 0x45A5, " 3200"},
    {0x31C5, 0x4E3A, " 2560"},
    {0x2924, 0x56DB, " 2000"},
    {0x2244, 0x5DBB, " 1600"},
    {0x1C50, 0x63AF, " 1280"},
    {0x16C0, 0x693F, " 1000"},
    {0x1292, 0x6D6D, "  800"},
    {0x0F18, 0x70E7, "  640"},
    {0x0BF5, 0x740A, "  500"},
    {0x09A9, 0x7656, "  400"},
    {0x07CA, 0x7835, "  320"},
    {0x0646, 0x79B9, "  256"},
    {0x04ED, 0x7B12, "  200"},
    {0x03F5, 0x7C0A, "  160"},
    {0x032D, 0x7CD2, "  128"},
    {0x027D, 0x7D82, "  100"},
    {0x01FE, 0x7E01, "   80"},
};

// Non-zero while any SE, sequence or stream is still sounding / queued.
int Snd_pronounce_ck(void)
{
    int ret;

    ret = Snd_se_pronounce_ck_all();
    ret |= Snd_seq_pronounce_ck_type(3);
    ret |= Snd_str_pronounce_ck_type(3);
    return ret;
}

// Asks for a driver soft reset (reset_flag bit0; the audio-frame players pick it up).
void Snd_soft_reset_req(void)
{
    Snd_ctrl_work.reset_flag = 1;
}

// 0x8000 until all three players have acknowledged the reset (bits 0x70), then whatever is still
// sounding; 0 = reset complete (flag cleared).
int Snd_soft_reset_ck(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    int ret;

    if ((ctrl->reset_flag & 0x70) != 0x70) {
        return 0x8000;
    }
    ret = Snd_pronounce_ck();
    if (ret == 0) {
        ctrl->reset_flag = 0;
    }
    return ret;
}

// Output mode changed: every SE and stream recomputes its pan.
void Snd_reset_pan_all(void)
{
    Snd_ctrl_work.se_ctrl_flag |= 0x80;
    Snd_str_reset_pan_type(3);
}

// Master volume changed: every SE, stream and sequence recomputes its volume.
void Snd_reset_vol_all(void)
{
    Snd_ctrl_work.se_ctrl_flag |= 0x100;
    Snd_str_reset_vol_type(3);
    Snd_seq_reset_vol_type(3);
}

// System volumes (0..127, kept 8.8): type bits 1 / 2 SE (two output kinds), 4 / 8 sequence,
// 0x10 / 0x20 stream.
void Snd_set_system_vol(s16 type, u16 vol)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    u16 v;

    v = vol << 8;
    if (type & 0x1) {
        ctrl->vol_mas_bgm = v;
    }
    if (type & 0x2) {
        ctrl->vol_mas_se = v;
    }
    if (type & 0x4) {
        ctrl->vol_iss_bgm = v;
    }
    if (type & 0x8) {
        ctrl->vol_iss_se = v;
    }
    if (type & 0x10) {
        ctrl->vol_str_bgm = v;
    }
    if (type & 0x20) {
        ctrl->vol_str_se = v;
    }
}

// One system volume (see Snd_set_system_vol) as 0..127.
s16 Snd_get_system_vol(s16 type)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    s16 v = 0;

    if (type == 1) {
        v = ctrl->vol_mas_bgm;
    }
    if (type == 2) {
        v = ctrl->vol_mas_se;
    }
    if (type == 4) {
        v = ctrl->vol_iss_bgm;
    }
    if (type == 8) {
        v = ctrl->vol_iss_se;
    }
    if (type == 16) {
        v = ctrl->vol_str_bgm;
    }
    if (type == 32) {
        v = ctrl->vol_str_se;
    }
    return v >> 8;
}

// 0..127 volume -> AX attenuation (table, 0 = -904).
s32 Snd_vol_syn_to_ax(s32 vol)
{
    return Snd_dls_vol_tbl[vol];
}

// AX attenuation -> the smallest 0..127 volume at least as loud; -1 on an out-of-range value.
s32 Snd_vol_ax_to_syn(s32 vol)
{
    int i;

    if (vol <= -904) {
        return 0;
    }
    for (i = 1; i < 128; i++) {
        if (vol <= Snd_dls_vol_tbl[i]) {
            return i;
        }
    }
    OSReport("SND Volume Error !!\n");
    return -1;
}

// Driver random byte (16-bit generator in Snd_ctrl_work.rnd).
u8 Snd_rnd(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_RND r;
    SND_RND r2;

    r.w = ctrl->random;
    r2.w = r.w * 3;
    r.b[1] += r2.b[0];
    r.b[0] = r2.b[0];
    if (ctrl->random == r.w) {
        ctrl->random++;
    } else {
        ctrl->random = r.w;
    }
    return r.b[1];
}

// Random pitch within the SIT's pitch_l..pitch_hi (in 1/16 steps when the range is wide);
// remembered in rnd_pitch.
s16 Snd_get_rnd_pitch(SND_SIT* sit)
{
    s16 lo;
    s16 hi;
    s16 range;
    s16 r;
    SND_CTRL* ctrl = &Snd_ctrl_work;

    lo = sit->pitch_l;
    hi = sit->pitch_h;
    if (lo == hi) {
        ctrl->rnd_pitch = lo;
        return lo;
    }
    range = hi - lo;
    if (range <= 16) {
        r = Snd_rnd() % (range + 1);
    } else {
        r = Snd_rnd() % 17;
        r *= range / 16;
    }
    r += lo;
    ctrl->rnd_pitch = r;
    return r;
}

// Resets the sound-test (debug menu) work: 14 SE blocks / 2 stream blocks, paths "/", display flags.
void Snd_test_work_clear(void)
{
    SND_TEST_WORK* test;
    u32 i;
    u8* p;

    p = (u8*) &Snd_test_work;
    for (i = 0; i < sizeof(SND_TEST_WORK); i++) {
        *p++ = 0;
    }
    test = &Snd_test_work;
    test->tbl = 0;
    test->aux = 0;
    test->menu = 1;
    test->dispFlag = 7;
    test->blkMax[0] = 0xE;
    test->blkMax[1] = 2;
    strcpy(test->path0, "/");
    strcpy(test->path1, "/");
    for (i = 0; i < 14; i++) {
        test->sitData[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        test->ritData[i] = 0;
    }
    test->aram_base = Snd_ctrl_work.zero_adrs;
}
