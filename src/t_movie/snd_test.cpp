#include "types.h"
#include "snd_test.h"
#include "eprintf.h"
#include <string.h>

// Sound test of the t_movie REL (snd_test.cpp; the file name is not in the binary). Plays ISS / stream
// requests, edits their SIT/RIT parameters and the AUX effects, shows the driver state and dumps ARAM.
// Every function works on the driver's Snd_test_work (snd_test.h).

// One editable SIT/RIT parameter.
struct TestPara {
    void* ptr;   // 0x00  the field inside Snd_test_work.sit / .rit
    s8 type;     // 0x04  0 s8, 1 s16, 3..7 flag bits, 8 s8 with max 3 (stream number)
    s8 kind;     // 0x05  move_type_tbl index
    s16 min;     // 0x06
    s16 max;     // 0x08
    s16 step;    // 0x0A
    s16 fast;    // 0x0C  step with the R trigger held
    s16 pad_E;
};

// One editable AUX effect parameter (float / integer versions share the layout).
struct EfxParaF {
    f32 min;
    f32 max;
    f32 step;
    f32 fast;
    f32* ptr[2];  // 0x10  AUX A / AUX B
};

struct EfxParaU {
    u32 min;
    u32 max;
    u32 step;
    u32 fast;
    u32* ptr[2];
};

static TestPara test_para_sit[] = {
    {&Snd_test_work.sit.note, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.note, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.curve_no, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.vol, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.sit.svol, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.sit.aux_a, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.aux_b, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.pitch_l, 1, 0, -2400, 2400, 1, 100},
    {&Snd_test_work.sit.pitch_h, 1, 0, -2400, 2400, 1, 100},
    {&Snd_test_work.sit.voice_start, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.voice_num, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.prio, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.flag, 4, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.pan, 0, 0, -2, 0x7F, 1, 10},
    {&Snd_test_work.sit.span, 0, 0, -2, 0x7F, 1, 10},
    {&Snd_test_work.sit.srd_type, 0, 0, 0, 3, 1, 1},
    {&Snd_test_work.sit.flag, 3, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.rnd_no, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.se_flag, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.wall_vol, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.inner_vol, 0, 0, -128, 0x7F, 1, 10},
    {&Snd_test_work.sit.xF, 0, 0, -128, 0x7F, 1, 10},
};

static TestPara test_para_midi[] = {
    {&Snd_test_work.sit.note, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.sit.curve_no, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.vol, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.sit.voice_start, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.voice_num, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.sit.flag, 5, 3, 0, 0, 0, 0},
    {&Snd_test_work.sit.flag, 6, 3, 0, 0, 0, 0},
};

static TestPara test_para_rit[] = {
    {&Snd_test_work.rit.str_no, 1, 4, 0, 0, 0, 0},
    {&Snd_test_work.rit.pad_6[0], 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.vol, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pad_B[0], 0, 4, 0, 0, 0, 0},
    {&Snd_test_work.rit.flag, 7, 3, 0, 0, 0, 0},
    {&Snd_test_work.rit.ch, 0, 0, 0, 0x3F, 1, 10},
    {&Snd_test_work.rit.pl_id, 8, 0, 0, 0, 1, 1},
    {&Snd_test_work.rit.aux_a, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.aux_b, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pad_6[1], 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.pan, 0, 0, 0, 0x7F, 1, 10},
    {&Snd_test_work.rit.span, 0, 0, -1, 0x7F, 1, 10},
    {&Snd_test_work.rit.svol, 0, 0, -1, 0x7F, 1, 10},
};

static void move_type_num(SND_TEST* w, TestPara* para);
static void move_type_note(SND_TEST* w, TestPara* para);
static void move_type_ext(SND_TEST* w, TestPara* para);
static void move_type_flag(SND_TEST* w, TestPara* para);
static void move_type_nop(SND_TEST* w, TestPara* para);

// parameters per SIT type (0 dummy, 1 normal, 2 ADSR, 3 MIDI)
static s8 test_para_max[4] = {0, 0x16, 0x16, 7};
static void (*move_type_tbl[5])(SND_TEST*, TestPara*) = {move_type_num, move_type_note, move_type_ext,
                                                            move_type_flag, move_type_nop};

static EfxParaF efx_para_rev_hi[6] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].rev_hi.preDelay, &Snd_efx_work[1].rev_hi.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].rev_hi.time, &Snd_efx_work[1].rev_hi.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_hi.coloration, &Snd_efx_work[1].rev_hi.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_hi.damping, &Snd_efx_work[1].rev_hi.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_hi.crosstalk, &Snd_efx_work[1].rev_hi.crosstalk}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_hi.mix, &Snd_efx_work[1].rev_hi.mix}},
};
static EfxParaF efx_para_rev_std[5] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].rev_std.preDelay, &Snd_efx_work[1].rev_std.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].rev_std.time, &Snd_efx_work[1].rev_std.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_std.coloration, &Snd_efx_work[1].rev_std.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_std.damping, &Snd_efx_work[1].rev_std.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_std.mix, &Snd_efx_work[1].rev_std.mix}},
};
static EfxParaU efx_para_chorus[3] = {
    {5, 15, 1, 5, {&Snd_efx_work[0].chorus.baseDelay, &Snd_efx_work[1].chorus.baseDelay}},
    {0, 5, 1, 1, {&Snd_efx_work[0].chorus.variation, &Snd_efx_work[1].chorus.variation}},
    {500, 10000, 1, 1000, {&Snd_efx_work[0].chorus.period, &Snd_efx_work[1].chorus.period}},
};
static EfxParaU efx_para_delay[9] = {
    {10, 5000, 1, 100, {&Snd_efx_work[0].delay.delay[0], &Snd_efx_work[1].delay.delay[0]}},
    {10, 5000, 1, 100, {&Snd_efx_work[0].delay.delay[1], &Snd_efx_work[1].delay.delay[1]}},
    {10, 5000, 1, 100, {&Snd_efx_work[0].delay.delay[2], &Snd_efx_work[1].delay.delay[2]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.feedback[0], &Snd_efx_work[1].delay.feedback[0]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.feedback[1], &Snd_efx_work[1].delay.feedback[1]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.feedback[2], &Snd_efx_work[1].delay.feedback[2]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.output[0], &Snd_efx_work[1].delay.output[0]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.output[1], &Snd_efx_work[1].delay.output[1]}},
    {0, 100, 1, 10, {&Snd_efx_work[0].delay.output[2], &Snd_efx_work[1].delay.output[2]}},
};
static EfxParaF efx_para_rev_dpl2[5] = {
    {0.0f, 0.1f, 0.05f, 0.05f, {&Snd_efx_work[0].rev_dpl2.preDelay, &Snd_efx_work[1].rev_dpl2.preDelay}},
    {0.0f, 10.0f, 0.125f, 1.0f, {&Snd_efx_work[0].rev_dpl2.time, &Snd_efx_work[1].rev_dpl2.time}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_dpl2.coloration, &Snd_efx_work[1].rev_dpl2.coloration}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_dpl2.damping, &Snd_efx_work[1].rev_dpl2.damping}},
    {0.0f, 1.0f, 0.125f, 0.25f, {&Snd_efx_work[0].rev_dpl2.mix, &Snd_efx_work[1].rev_dpl2.mix}},
};

static int test_move_epara_f32(SND_TEST* w);
static int test_move_epara_u32(SND_TEST* w);

static int (*test_move_epara_tbl[6])(SND_TEST*) = {NULL, test_move_epara_f32, test_move_epara_f32,
                                                      test_move_epara_u32, test_move_epara_u32, test_move_epara_f32};
// parameters per effect type
static s8 test_aux_para_num[6] = {0, 6, 5, 3, 9, 5};
static s16* vol_ptr_tbl[6] = {&Snd_ctrl_work.vol_mas_bgm, &Snd_ctrl_work.vol_mas_se, &Snd_ctrl_work.vol_iss_bgm,
                              &Snd_ctrl_work.vol_iss_se, &Snd_ctrl_work.vol_str_bgm, &Snd_ctrl_work.vol_str_se};

int Snd_test_mode();
void test_mode_menu(SND_TEST* w);
void test_mode_move(SND_TEST* w);
int test_sit_init(SND_TEST* w);
int test_rit_init(SND_TEST* w);
int test_sit_or_rit(SND_TEST* w);
void test_tbl_now_check(SND_TEST* w, int dir, int max);
TestPara* test_get_tpara_adrs(SND_TEST* w);
int test_tbl_para_select(SND_TEST* w, int max);
s16 test_para_ck_s16(SND_TEST* w, TestPara* para, s16 val);
int test_play_or_stop(SND_TEST* w);
int test_req_no_select(SND_TEST* w, s16 max);
void test_blk_no_select(SND_TEST* w);
int test_blk_enable_ck(SND_TEST* w, int dir);
void Snd_test_efx_init(SND_TEST* w);
int Snd_test_efx_main(SND_TEST* w);
int test_efx_type_select(SND_TEST* w, SND_EFX* efx);
int test_efx_on_or_off(SND_TEST* w, SND_EFX* efx);
int test_aux_para_select(SND_TEST* w);
void test_tbl_aux_ck(SND_TEST* w);
int Snd_test_volume(SND_TEST* w);
void Snd_test_disp_move(SND_TEST* w);
static void snd_test_disp_sit();
void disp_sit_type(SND_TEST* w, SND_SIT* sit);

void disp_sit_normal(SND_IBLK* blk, SND_SIT* sit, int x, int y);
void disp_sit_midi(SND_IBLK* blk, SND_SIT* sit, int x, int y);
void disp_seq_volume(SND_SEQ* seq);
static void snd_test_disp_rit();
int str_get_player_id();
void disp_cursor(SND_TEST* w, int x, int y);
void disp_str_status(SND_TEST* w, int x, int y);
void disp_sequencer();
void disp_se_wt_data(SND_TEST* w, SND_IBLK* blk, SND_SIT* sit);
void get_wt_ptr(SND_TEST* w, SND_IBLK* blk, SND_SIT* sit);
void disp_adsr_para(SND_TEST* w);
void get_axv_ptr(SND_TEST* w);
void Snd_test_disp_menu(SND_TEST* w);
void Snd_test_disp_basic(SND_TEST* w);
void Snd_test_disp_voice(SND_TEST* w);
void Snd_test_disp_req_para(SND_TEST* w);
void Snd_test_disp_aux(SND_TEST* w);
void Snd_test_disp_efx();
static void test_disp_efx_rev_hi(SND_TEST* w, SND_EFX* efx, int x, int y);
static void test_disp_efx_rev_std(SND_TEST* w, SND_EFX* efx, int x, int y);
static void test_disp_efx_chorus(SND_TEST* w, SND_EFX* efx, int x, int y);
static void test_disp_efx_delay(SND_TEST* w, SND_EFX* efx, int x, int y);
static void test_disp_efx_rev_dpl2(SND_TEST* w, SND_EFX* efx, int x, int y);
void Snd_test_disp_vol();
void Snd_test_load_init(SND_TEST* w);
void Snd_test_mode_load(SND_TEST* w);
int blk_no_check(SND_TEST* w);
void load_select(SND_TEST* w);
void load_sit_data(SND_TEST* w, char* name);
void load_rit_data(SND_TEST* w, char* name);
void directory_open(SND_TEST* w);
void dir_entry_read(SND_TEST* w, int dirs);
void directory_disp(SND_TEST* w);
void cursor_disp(SND_TEST* w);
void blk_file_disp(SND_TEST* w);
void dir_name_up(SND_TEST* w);
void change_to_cap(char* s);
char* get_file_ext(char* s);
int get_dir_level(char* s);
void Snd_test_dump_init(SND_TEST* w);
void Snd_test_aram_dump(SND_TEST* w);
void aram_blk_no_select(SND_TEST* w);
void blk_enable_ck(SND_TEST* w, int dir);
void aram_dump_dma(SND_TEST* w);
static void cb_dma_end(u32 task);
void aram_dump_disp(SND_TEST* w);

// Sound test frame: START / Z (0x1010) leave (returns 1); the Y menu (test_mode_menu) picks the
// mode, otherwise the mode runs: 5 ARAM dump, 6/7 SIT / RIT file load, else test_mode_move (SIT,
// RIT, AUX A/B, VOL).
int Snd_test_mode()
{
    SND_TEST* w = &Snd_test_work;

    if (w->joy_trg & 0x1010) {
        return 1;
    }
    if (w->menu_flag != 0) {
        test_mode_menu(w);
    } else if (w->joy_trg & 0x800) {
        w->menu_flag = 1;
    } else {
        switch (w->rno00) {
        case 5:
            Snd_test_aram_dump(w);
            break;
        case 6:
        case 7:
            Snd_test_mode_load(w);
            break;
        default:
            test_mode_move(w);
            break;
        }
    }
    Snd_test_disp_basic(w);
    w->timer++;
    return 0;
}

// Mode menu: up/down pick SIT / RIT / AUX A / AUX B / VOL / DUMP (wrapping), A / X enter it
// (loads / dumps initialised), Y closes.
void test_mode_menu(SND_TEST* w)
{
    if (w->joy_trg & 0x900) {
        w->menu_flag = 0;
        switch (w->rno00) {
        case 0:
            w->tbl_type = 0;
            break;
        case 1:
            w->tbl_type = 1;
            break;
        case 2:
            w->aux_type = 0;
            Snd_test_efx_init(w);
            break;
        case 3:
            w->aux_type = 1;
            Snd_test_efx_init(w);
            break;
        case 5:
            Snd_test_dump_init(w);
            break;
        case 6:
            w->load_type = 0;
            Snd_test_load_init(w);
            return;
        case 7:
            w->load_type = 1;
            Snd_test_load_init(w);
            return;
        }
        if (test_blk_enable_ck(w, 1) == 1) {
            w->menu_flag = 1;
        }
    } else {
        if (w->joy_rep & 8) {
            w->rno00--;
        }
        if (w->joy_rep & 4) {
            w->rno00++;
        }
        if (w->rno00 & 0x80) {
            w->rno00 = 5;
        }
        if (w->rno00 > 5) {
            w->rno00 = 0;
        }
        Snd_test_disp_menu(w);
    }
}

// Runs the current mode: SIT / RIT parameter editing (test_sit_or_rit), effect editing, volume
// editing, then play / stop and the display; block selection with the C stick.
void test_mode_move(SND_TEST* w)
{
    int ret;

    if (w->tbl_type == 0) {
        ret = test_sit_init(w);
    } else {
        ret = test_rit_init(w);
    }
    w->tbl_now = w->tbl_idx[w->tbl_type];
    w->efx_now = w->efx_idx[w->aux_type];
    w->aux_now = w->aux_idx[w->aux_type];
    if (ret == 0) {
        switch (w->rno00) {
        case 0:
        case 1:
            ret = test_sit_or_rit(w);
            break;
        case 2:
        case 3:
            ret = Snd_test_efx_main(w);
            break;
        case 4:
            ret = Snd_test_volume(w);
            break;
        }
        if (ret == 0) {
            ret = test_play_or_stop(w);
        }
    }
    Snd_test_disp_move(w);
    w->req_no[w->tbl_type] = w->req_now;
    if (w->tbl_type == 0) {
        *w->stp = w->sit;
    } else {
        *w->rip = w->rit;
    }
    w->tbl_idx[w->tbl_type] = w->tbl_now;
    w->efx_idx[w->aux_type] = w->efx_now;
    w->aux_idx[w->aux_type] = w->aux_now;
    if (ret == 0) {
        test_blk_no_select(w);
    }
}

// Copies the current ISS block's request (blkNo[0] / reqNo[0]) SIT into the edit copy; 1 when the
// block is loaded.
int test_sit_init(SND_TEST* w)
{
    SND_IBLK* blk = &Snd_iss_blk[w->blk_no[0]];
    int ret;

    ret = test_req_no_select(w, blk->sit_num);
    w->stp = &blk->sit_adrs[w->req_now];
    w->sit = *w->stp;
    if (w->stp->flag & 0x8000) {
        w->sit_type = 0;
    }
    if (w->stp->flag & 0x1) {
        w->sit_type = 1;
    }
    if (w->stp->flag & 0x2) {
        w->sit_type = 2;
    }
    if (w->stp->flag & 0x4) {
        w->sit_type = 3;
    }
    return ret;
}

// Copies the current stream block's request RIT into the edit copy; 1 when loaded.
int test_rit_init(SND_TEST* w)
{
    SND_RBLK* blk = &Snd_str_blk[w->blk_no[1]];
    int ret;

    ret = test_req_no_select(w, blk->rit_num);
    w->rip = &blk->rit_adrs[w->req_now];
    w->rit = *w->rip;
    return ret;
}

// SIT / RIT editing: L/R (0x00F00000 stick) switch the request number, the d-pad moves the
// parameter cursor and changes the value through the parameter's move_type; 1 when something
// changed (the edit copy is written back to the block).
int test_sit_or_rit(SND_TEST* w)
{
    s8 max;
    TestPara* para;

    if (w->tbl_type == 0) {
        s8 type = w->sit_type;

        if (type == 0) {
            return 1;
        }
        if (w->joy_trg & 0x00F00000) {
            if (type == 3) {
                w->disp_midi ^= 1;
            } else {
                w->disp_ax ^= 1;
            }
            return 1;
        }
        max = test_para_max[type];
    } else {
        if (w->rit.flag & 0x8000) {
            return 1;
        }
        max = 0xD;
    }
    test_tbl_now_check(w, 1, max);
    if (test_tbl_para_select(w, max) != 0) {
        return 1;
    }
    if (!(w->joy_rep & 0xF)) {
        return 0;
    }
    if (w->tbl_type == 1) {
        Snd_str_fade_out_type(3, 0);
    }
    para = test_get_tpara_adrs(w);
    move_type_tbl[para->kind](w, para);
    return 1;
}

// skips the parameters the table marks as unselectable (kind 4) in direction `dir`
void test_tbl_now_check(SND_TEST* w, int dir, int max)
{
    for (;;) {
        if (w->tbl_now & 0x80) {
            w->tbl_now = max - 1;
        }
        if (w->tbl_now >= max) {
            w->tbl_now = 0;
        }
        if (test_get_tpara_adrs(w)->kind != 4) {
            break;
        }
        {
            // the byte in an int local (zero-extended read): a direct `w->cur += dir` is a paradoxical
            // SUBREG of the QI load, which cse's fold_rtx always puts first (`add cur,dir`)
            int c = (u8) w->tbl_now;
            w->tbl_now = dir + c;
        }
    }
}

// The parameter table of the current table / SIT type (sit_para_tbl[type] or rit_para_tbl).
TestPara* test_get_tpara_adrs(SND_TEST* w)
{
    // integer form: `add r3, idx, tbl` operand order; one result variable: the index temp is not
    // tied to r3 by local-alloc (the result pseudo is global), so it lands in r0
    TestPara* p;

    if (w->tbl_type == 0) {
        if (w->sit_type == 3) {
            p = (TestPara*) (w->tbl_now * sizeof(TestPara) + (u32) test_para_midi);
        } else {
            p = (TestPara*) (w->tbl_now * sizeof(TestPara) + (u32) test_para_sit);
        }
    } else {
        p = (TestPara*) (w->tbl_now * sizeof(TestPara) + (u32) test_para_rit);
    }
    return p;
}

// Up/down (repeat, not while X is held) move the parameter cursor 0..max-1; 1 when it moved.
int test_tbl_para_select(SND_TEST* w, int max)
{
    s8 old;

    if (w->joy_now & 0x400) {
        return 0;
    }
    old = w->tbl_now;
    if (w->joy_rep & 8) {
        w->tbl_now--;
        test_tbl_now_check(w, -1, max);
    }
    if (w->joy_rep & 4) {
        w->tbl_now++;
        test_tbl_now_check(w, 1, max);
    }
    if (w->tbl_now != old) {
        return 1;
    }
    return 0;
}

// Left/right change a numeric parameter by step (fast with R) within min..max, s8 / s16 by type.
static void move_type_num(SND_TEST* w, TestPara* para)
{
    if (para->type == 0) {
        *(s8*) para->ptr = test_para_ck_s16(w, para, *(s8*) para->ptr);
    } else if (para->type == 1) {
        *(s16*) para->ptr = test_para_ck_s16(w, para, *(s16*) para->ptr);
    } else if (para->type == 8) {
        para->max = 3;
        *(s8*) para->ptr = test_para_ck_s16(w, para, *(s8*) para->ptr);
    }
}

// bank (high byte) / program (low byte) of the SIT note, selected by the cursor column
static void move_type_note(SND_TEST* w, TestPara* para)
{
    s16 v = *(s16*) para->ptr;
    s16 val;

    if (w->tbl_now != 1) {
        val = v >> 8;
        v &= 0xFF;
    } else {
        val = v & 0xFF;
        v &= ~0xFF;
    }
    val = test_para_ck_s16(w, para, val);
    if (w->tbl_now != 1) {
        v = (val << 8) | v;
    } else {
        v |= val & 0xFF;
    }
    *(s16*) para->ptr = v;
}

// Unused parameter kind (no-op).
static void move_type_ext(SND_TEST* w, TestPara* para)
{
}

// Left/right toggle the flag bit of the parameter (type 3..7 = bit number).
static void move_type_flag(SND_TEST* w, TestPara* para)
{
    u16* p = (u16*) para->ptr;
    s8 type = para->type;
    u16 bit = (type == 3) ? 0x2000 : 0;
    u16 v = *p;

    if (type == 4) {
        bit = 0x4000;
    }
    if (type == 5) {
        bit = 0x4000;
    }
    if (type == 6) {
        bit = 0x100;
    }
    if (type == 7) {
        bit = 1;
    }
    if (w->joy_rep & 3) {
        v ^= bit;
    }
    *p = v;
}

// Read-only parameter.
static void move_type_nop(SND_TEST* w, TestPara* para)
{
}

// steps `val` by the pad (left/right; with X held: min/max/zero) and wraps it into the range
s16 test_para_ck_s16(SND_TEST* w, TestPara* para, s16 val)
{
    s16 step;

    if (w->joy_now & 0x20) {
        step = para->fast;
    } else {
        step = para->step;
    }
    if (w->joy_now & 0x400) {
        if (w->joy_now & 1) {
            val = para->min;
        }
        if (w->joy_now & 2) {
            val = para->max;
        }
        if (w->joy_now & 8) {
            val = 0;
        }
        if (w->joy_now & 4) {
            val = 0;
        }
    } else {
        if (w->joy_rep & 1) {
            val -= step;
        }
        if (w->joy_rep & 2) {
            val += step;
        }
    }
    if (val < para->min) {
        val = para->max;
    }
    if (val > para->max) {
        val = para->min;
    }
    return val;
}

// B stops everything (SE / sequence / stream fade outs), A plays the current request: an ISS
// request (Snd_iss_req_para, the id kept in sndId) or the stream by name; 1 when pressed.
int test_play_or_stop(SND_TEST* w)
{
    if (w->joy_trg & 0x200) {
        if (w->tbl_type == 0) {
            Snd_se_fade_out_all(0);
            Snd_seq_fade_out_type(3, 0);
        } else {
            Snd_str_fade_out_type(3, 0);
        }
        return 1;
    }
    if (!(w->joy_trg & 0x100)) {
        return 0;
    }
    {
        s8 tbl = w->tbl_type;
        u16 blk = w->blk_no[tbl];
        u32 id;

        if (tbl == 0) {
            SND_CTRL* ctrl = &Snd_ctrl_work;
            SND_SIT* sit = Snd_get_sit_adrs(blk, w->req_now);

            if (sit->srd_type == 1) {
                ctrl->srd_type = 1;
            } else {
                ctrl->srd_type = 0;
            }
            ctrl->para_flag = 0x100;
            id = Snd_iss_req_para(blk, w->req_now, NULL);
            if (id) {
                w->snd_id = id;
            }
        } else {
            char* name = (char*) Snd_test_get_str_name(blk, w->req_now);
            id = Snd_str_prepare(blk, w->req_now, name, -1);
            if (id) {
                w->snd_id = id;
                Snd_str_req(id, 3, 0, 0);
            }
        }
    }
    return 1;
}

// R held: stick left/right step the request number 0..max-1 (R + X by 16); 1 when it changed.
int test_req_no_select(SND_TEST* w, s16 max)
{
    int step = 1;
    s8 tbl = w->tbl_type;
    s16 old;

    w->req_now = w->req_no[tbl];
    w->req_max[tbl] = max;
    old = w->req_now;
    if (w->joy_now & 0x20) {
        step = 10;
    }
    if (w->joy_now & 0x400) {
        w->req_now = 0;
    } else {
        if (w->joy_rep & 0x10000) {
            w->req_now -= step;
        }
        if (w->joy_rep & 0x20000) {
            {
                // same as test_tbl_now_check: the halfword in an int local, promoted variable first
                int c = (u16) w->req_now;
                w->req_now = step + c;
            }
        }
    }
    if (w->req_now < 0) {
        w->req_now = max - 1;
    }
    if (w->req_now >= max) {
        w->req_now = 0;
    }
    if (w->req_now == old) {
        return 0;
    }
    return 1;
}

// C stick up/down step the block number of the current table over the loaded blocks (request
// reset to 0).
void test_blk_no_select(SND_TEST* w)
{
    if (w->joy_trg & 0x80000) {
        test_blk_enable_ck(w, -1);
    }
    if (w->joy_trg & 0x40000) {
        test_blk_enable_ck(w, 1);
    }
}

// steps to the next loaded block; 0 when one was found
int test_blk_enable_ck(SND_TEST* w, int dir)
{
    s16 i;
    s16 max = w->blk_max[w->tbl_type];
    s16 no = w->blk_no[w->tbl_type];

    for (i = 0; i < max; i++) {
        no += dir;
        if (no < 0) {
            no = max - 1;
        }
        if (no >= max) {
            no = 0;
        }
        // one test per table arm (jump2 cross-jumps the identical cmpwi/beq tails: the two
        // block-local num qtys take r9 next to the r0 index temp instead of one global r0)
        if (w->tbl_type == 0) {
            if (Snd_iss_blk[no].sit_num == 0) {
                continue;
            }
        } else {
            if (Snd_str_blk[no].rit_num == 0) {
                continue;
            }
        }
        w->blk_no[w->tbl_type] = no;
        w->req_no[w->tbl_type] = 0;
        return 0;
    }
    return 1;
}

// AUX mode entry: reads the slot's current effect type / running state into the work.
void Snd_test_efx_init(SND_TEST* w)
{
    SND_EFX* efx = &Snd_efx_work[w->aux_type];

    if (efx->be_flag & 1) {
        w->efx_ok[w->aux_type] = 1;
    } else {
        w->efx_ok[w->aux_type] = -1;
    }
    if (efx->efx_type == 6) {
        w->efx_idx[w->aux_type] = 0;
    } else {
        w->efx_idx[w->aux_type] = efx->efx_type;
    }
}

// AUX A/B editing: effect type select (L), on/off (A / Z), parameter cursor and value edits
// (float / u32 tables); 1 when the display must refresh.
int Snd_test_efx_main(SND_TEST* w)
{
    SND_EFX* efx = &Snd_efx_work[w->aux_type];

    if (test_efx_type_select(w, efx) != 0) {
        return 1;
    }
    if (w->efx_now == 0) {
        return 0;
    }
    test_tbl_aux_ck(w);
    if (test_efx_on_or_off(w, efx) != 0) {
        return 1;
    }
    if (test_aux_para_select(w) != 0) {
        return 1;
    }
    if (test_move_epara_tbl[w->efx_now](w) != 0) {
        return 1;
    }
    return 0;
}

// L cycles the slot's effect type (reverb hi / std, chorus, delay, DPL2 reverb); the effect is
// stopped for the change.
int test_efx_type_select(SND_TEST* w, SND_EFX* efx)
{
    if (!(w->joy_trg & 0x40)) {
        return 0;
    }
    w->efx_now++;
    if (w->efx_now == 6) {
        w->efx_now = 0;
    }
    if (Snd_get_sound_mode() != 2 && w->efx_now == 5) {
        w->efx_now = 0;
    }
    w->efx_ok[w->aux_type] = -1;
    Snd_efx_req(w->aux_type, 6);
    w->aux_now = 0;
    return 1;
}

// A starts the effect with the edited parameters, Z stops it.
int test_efx_on_or_off(SND_TEST* w, SND_EFX* efx)
{
    if (w->joy_trg & 0x100) {
        if (w->efx_ok[w->aux_type] <= 0) {
            if (Snd_efx_req(w->aux_type, w->efx_now) == 0) {
                w->efx_ok[w->aux_type] = 1;
            }
            return 1;
        }
    }
    if (w->joy_trg & 0x10) {
        w->efx_ok[w->aux_type] = -1;
        Snd_efx_req(w->aux_type, 6);
        return 1;
    }
    return 0;
}

// Up/down move the effect parameter cursor (not while X is held); 1 when it moved.
int test_aux_para_select(SND_TEST* w)
{
    s8 max;
    s8 old;

    if (w->joy_now & 0x400) {
        return 0;
    }
    max = test_aux_para_num[w->efx_now];
    old = w->aux_now;
    if (w->joy_rep & 8) {
        w->aux_now--;
    }
    if (w->joy_rep & 4) {
        w->aux_now++;
    }
    if (w->aux_now & 0x80) {
        w->aux_now = max - 1;
    }
    if (w->aux_now >= max) {
        w->aux_now = 0;
    }
    if (w->aux_now != old) {
        return 1;
    }
    return 0;
}

// Left/right change the cursor's float effect parameter by its step (R fast) within its range.
static int test_move_epara_f32(SND_TEST* w)
{
    EfxParaF* p = NULL;
    f32* v;
    f32 step;
    f32 val;
    f32 old;

    switch (w->efx_now) {
    case 1:
        p = &efx_para_rev_hi[w->aux_now];
        break;
    case 2:
        p = &efx_para_rev_std[w->aux_now];
        break;
    case 5:
        p = &efx_para_rev_dpl2[w->aux_now];
        break;
    }
    if (w->aux_type == 0) {
        v = p->ptr[0];
    } else {
        v = p->ptr[1];
    }
    if (w->joy_now & 0x20) {
        step = p->fast;
    } else {
        step = p->step;
    }
    val = *v;
    old = val;
    if (w->joy_now & 0x400) {
        if (w->joy_now & 1) {
            val = p->min;
        }
        if (w->joy_now & 2) {
            val = p->max;
        }
        if (w->joy_now & 8) {
            val = 0.0f;
        }
        if (w->joy_now & 4) {
            val = 0.0f;
        }
    } else {
        if (w->joy_rep & 1) {
            val -= step;
        }
        if (w->joy_rep & 2) {
            val += step;
        }
    }
    if (val < p->min) {
        val = p->max;
    }
    if (val > p->max) {
        val = p->min;
    }
    if (w->aux_now == 1 && val == 0.0f) {
        val = 0.01f;
    }
    if (val == old) {
        return 0;
    }
    w->efx_ok[w->aux_type] = 0;
    *v = val;
    return 1;
}

// Left/right change the cursor's integer effect parameter (masks / shifts of the packed word).
static int test_move_epara_u32(SND_TEST* w)
{
    EfxParaU* p;
    u32* v;
    u32 step;
    u32 val;
    u32 old;

    if (w->efx_now == 3) {
        p = &efx_para_chorus[w->aux_now];
    } else {
        p = &efx_para_delay[w->aux_now];
    }
    if (w->aux_type == 0) {
        v = p->ptr[0];
    } else {
        v = p->ptr[1];
    }
    if (w->joy_now & 0x20) {
        step = p->fast;
    } else {
        step = p->step;
    }
    val = *v;
    old = val;
    if (w->joy_now & 0x400) {
        if (w->joy_now & 1) {
            val = p->min;
        }
        if (w->joy_now & 2) {
            val = p->max;
        }
        if (w->joy_now & 8) {
            val = 0;
        }
        if (w->joy_now & 4) {
            val = 0;
        }
    } else {
        if (w->joy_rep & 1) {
            val -= step;
        }
        if (w->joy_rep & 2) {
            val += step;
        }
    }
    if ((s32) val < (s32) p->min) {
        val = p->max;
    }
    if ((s32) val > (s32) p->max) {
        val = p->min;
    }
    if (val == old) {
        return 0;
    }
    w->efx_ok[w->aux_type] = 0;
    *v = val;
    return 1;
}

// AUX send level of the request being edited (sub stick)
void test_tbl_aux_ck(SND_TEST* w)
{
    s8* p;
    s8 v;
    int step;

    if (w->tbl_type == 0) {
        p = &w->sit.aux_b;
        if (w->aux_type == 0) {
            p = &w->sit.aux_a;
        }
    } else {
        p = &w->rit.aux_b;
        if (w->aux_type == 0) {
            p = &w->rit.aux_a;
        }
    }
    v = *p;
    step = 1;
    if (w->joy_now & 0x20) {
        step = 10;
    }
    if (w->joy_rep & 0x100000) {
        v -= step;
    }
    if (w->joy_rep & 0x200000) {
        v += step;
    }
    if (w->joy_rep & 0x800000) {
        v = 0;
    }
    if (w->joy_rep & 0x400000) {
        v = 0x7F;
    }
    v &= 0x7F;
    *p = v;
}

// VOL mode: up/down pick one of the six system volumes (the six vol_* fields of Snd_ctrl_work), left/right change
// it; 1 when something changed.
int Snd_test_volume(SND_TEST* w)
{
    s8 old = w->vol_idx;
    s16* p;
    int step;
    s8 cur;
    s8 v;

    if (!(w->joy_now & 0x400)) {
        if (w->joy_rep & 8) {
            w->vol_idx--;
        }
        if (w->joy_rep & 4) {
            w->vol_idx++;
        }
        if (w->vol_idx & 0x80) {
            w->vol_idx = 5;
        }
        if (w->vol_idx > 5) {
            w->vol_idx = 0;
        }
    }
    if (w->vol_idx != old) {
        return 1;
    }
    p = vol_ptr_tbl[w->vol_idx];
    cur = *p >> 8;
    v = cur;
    step = 1;
    if (w->joy_now & 0x20) {
        step = 10;
    }
    if (w->joy_now & 0x400) {
        if (w->joy_now & 1) {
            v = 0;
        }
        if (w->joy_now & 2) {
            v = 0x7F;
        }
        if (w->joy_now & 8) {
            v = 0;
        }
        if (w->joy_now & 4) {
            v = 0;
        }
    } else {
        if (w->joy_rep & 1) {
            v -= step;
        }
        if (w->joy_rep & 2) {
            v += step;
        }
    }
    v &= 0x7F;
    if (v == cur) {
        return 0;
    }
    *p = v << 8;
    Snd_reset_vol_all();
    return 1;
}

static void (*disp_tbl[5])(SND_TEST*) = {(void (*)(SND_TEST*)) snd_test_disp_sit,
                                            (void (*)(SND_TEST*)) snd_test_disp_rit,
                                            (void (*)(SND_TEST*)) Snd_test_disp_efx,
                                            (void (*)(SND_TEST*)) Snd_test_disp_efx,
                                            (void (*)(SND_TEST*)) Snd_test_disp_vol};

// Draws the current mode's screen (SIT / RIT / AUX / VOL).
void Snd_test_disp_move(SND_TEST* w)
{
    disp_tbl[w->rno00](w);
}

// SIT screen: block / request numbers, the SIT by type (or the wavetable data / sequencer view
// when toggled) and the cursor.
static void snd_test_disp_sit()
{
    SND_TEST* w = &Snd_test_work;
    SND_IBLK* blk = &Snd_iss_blk[w->blk_no[w->tbl_type]];
    SND_SIT* sit = blk->sit_adrs;

    sit += w->req_now;
    switch (w->sit_type) {
    case 0:
        disp_sit_type(w, sit);
        break;
    case 1:
    case 2:
        disp_sit_type(w, sit);
        if (w->disp_ax == 0) {
            disp_sit_normal(blk, sit, 0xB8, 0x54);
            disp_cursor(w, 0x18, 0x54);
        } else {
            disp_se_wt_data(w, blk, sit);
        }
        break;
    case 3:
        if (w->disp_midi == 0) {
            disp_sit_type(w, sit);
            disp_sit_midi(blk, sit, 0xB8, 0x54);
            disp_cursor(w, 0x18, 0x54);
        } else {
            disp_sequencer();
        }
        break;
    }
}

static char* sit_type_name[4] = {"TYPE      : DUMMY", "TYPE      :   NML", "TYPE      :  ADSR", "TYPE      :  MIDI"};

// Prints the SIT type name (dummy / normal / ADSR / MIDI).
void disp_sit_type(SND_TEST* w, SND_SIT* sit)
{
    eprintf(0x18, 0x54, 0, 1, sit_type_name[w->sit_type]);
    eprintf(0x18, 0x70, 0, 1, "NOTE      : %04XH", sit->note);
    eprintf(0x18, 0x7E, 0, 1, "FLAG      : %04XH", sit->flag);
}

static char* srd_type_name[4] = {"   OFF", "DIRECT", "UPDATE", "  ONCE"};
static char* on_off_name[2] = {"OFF", " ON"};

// Prints a normal / ADSR SIT: PATCH / NOTE, VTBL_NO, VOL / SVOL, AUX_A/B, pan, pitch, flags, the
// surround type; ADSR SITs add their envelope values.
void disp_sit_normal(SND_IBLK* blk, SND_SIT* sit, int x, int y)
{
    u8* wt = blk->wt_adrs;
    WTFILEHEADER* hdr = (WTFILEHEADER*) wt;
    WTINST* inst = (WTINST*) (wt + hdr->offsetMelodicInst);
    WTREGION* rgn = (WTREGION*) (wt + hdr->offsetRegions);
    WTART* art = (WTART*) (wt + hdr->offsetArticulations);
    s32 dlsVol;
    s32 synVol;
    s32 axVol;

    // the three table pointers are advanced in place (the base pseudo is the final pointer:
    // `add r26,r26,r0`), like rit in snd_test_disp_rit
    inst += sit->note >> 8;
    rgn += inst->keyRegion[sit->note & 0xFF];
    art += rgn->articulationIndex;
    eprintf(x, y, 0, 1, "PATCH     : %5d", sit->note >> 8);
    eprintf(x, y + 0xE, 0, 1, "NOTE      : %5d", sit->note & 0xFF);
    eprintf(x, y + 0x1C, 0, 1, "VTBL_NO   : %5d", sit->curve_no);
    eprintf(x, y + 0x2A, 0, 1, "VOL       : %5d", sit->vol);
    eprintf(x, y + 0x38, 0, 1, "SVOL      : %5d", sit->svol);
    eprintf(x, y + 0x46, 0, 1, "AUX_A     : %5d", sit->aux_a);
    eprintf(x, y + 0x54, 0, 1, "AUX_B     : %5d", sit->aux_b);
    eprintf(x, y + 0x62, 0, 1, "PITCH_L   : %5d", (s16) sit->pitch_l);
    eprintf(x, y + 0x70, 0, 1, "PITCH_H   : %5d", (s16) sit->pitch_h);
    eprintf(x, y + 0x7E, 0, 1, "CH_NO     : %5d", sit->voice_start);
    eprintf(x, y + 0x8C, 0, 1, "MONOPOLY  : %5d", sit->voice_num);
    eprintf(x, y + 0x9A, 0, 1, "PRIO_NO   : %5d", sit->prio);
    if (sit->flag & 0x4000) {
        eprintf(x, y + 0xA8, 0, 1, "PRIO_MODE :   BCK");
    } else {
        eprintf(x, y + 0xA8, 0, 1, "PRIO_MODE :   FWD");
    }
    eprintf(x, y + 0xB6, 0, 1, "PAN       : %5d", sit->pan);
    eprintf(x, y + 0xC4, 0, 1, "SPAN      : %5d", sit->span);
    x += 0xA8;
    eprintf(x, y, 0, 1, "SRD_TYPE : %s", srd_type_name[sit->srd_type]);
    if (sit->flag & 0x2000) {
        eprintf(x, y + 0xE, 0, 1, "LINK     :     ON");
    } else {
        eprintf(x, y + 0xE, 0, 1, "LINK     :    OFF");
    }
    eprintf(x, y + 0x1C, 0, 1, "RTBL_NO  :  %5d", sit->rnd_no);
    eprintf(x, y + 0x2A, 0, 1, "CTRL     :  %5d", (s8) sit->se_flag);
    eprintf(x, y + 0x38, 0, 1, "FREE3    :  %5d", (s8) sit->wall_vol);
    eprintf(x, y + 0x46, 0, 1, "FREE4    :  %5d", (s8) sit->inner_vol);
    eprintf(x, y + 0x54, 0, 1, "FREE5    :  %5d", (s8) sit->xF);
    eprintf(x, y + 0x70, 0, 1, "NO FADEOUT :  %s", on_off_name[(sit->se_flag & 1) ? 1 : 0]);
    eprintf(x, y + 0x7E, 0, 1, "NO PAUSE   :  %s", on_off_name[(sit->se_flag & 2) ? 1 : 0]);
    eprintf(x, y + 0x8C, 0, 1, "NO VDOWN   :  %s", on_off_name[(sit->se_flag & 4) ? 1 : 0]);
    eprintf(x, y + 0x9A, 0, 1, "EVENT OK   :  %s", on_off_name[(sit->se_flag & 8) ? 1 : 0]);
    eprintf(x, y + 0xA8, 0, 1, "WALL CHECK :  %s", on_off_name[(sit->se_flag & 0x10) ? 1 : 0]);
    eprintf(x, y + 0xB6, 0, 1, "AT CHECK   :  %s", on_off_name[(sit->se_flag & 0x20) ? 1 : 0]);
    // the driver's iss_ax_set_vol shape: `/ 0x10000` is the branchy signed division (BRANCH_COST 0),
    // and the two-arm if/else is what keeps the FREE4/FREE5 row y out of cse1's path so that gcse PRE
    // copies them for the VOL(DLS)/VOL(SYN) rows (`mr r21,r29` / `mr r22,r30`); the then-arm's
    // `mr r31,r3` is cross-jumped with the call arm's
    dlsVol = rgn->attn / 0x10000;
    if (sit->vol >= 0) {
        synVol = sit->vol;
    } else {
        synVol = Snd_vol_ax_to_syn(dlsVol);
    }
    axVol = Snd_vol_syn_to_ax(synVol);
    eprintf(0x18, y + 0x46, 0, 1, "VOL(DLS)  : %5d", dlsVol);
    eprintf(0x18, y + 0x54, 0, 1, "VOL(SYN)  : %5d", synVol);
    eprintf(0x18, y + 0x62, 0, 1, "VOL(AX)   : %5d", axVol);
    eprintf(0x18, y + 0x7E, 0, 1, "PAN(DLS)  : %5d", art->pan);
}

// Prints a MIDI SIT: MIDI_NO, VOL_FLAG, VOL, CH_NO and the sequence work's state.
void disp_sit_midi(SND_IBLK* blk, SND_SIT* sit, int x, int y)
{
    SND_SEQ* seq;

    eprintf(x, y, 0, 1, "MIDI_NO   : %5d", sit->note >> 8);
    eprintf(x, y + 0xE, 0, 1, "VOL_FLAG  : %5d", sit->curve_no);
    eprintf(x, y + 0x1C, 0, 1, "VOL       : %5d", sit->vol);
    eprintf(x, y + 0x2A, 0, 1, "CH_NO     : %5d", sit->voice_start);
    eprintf(x, y + 0x38, 0, 1, "MONOPOLY  : %5d", sit->voice_num);
    if (sit->flag & 0x4000) {
        eprintf(x, y + 0x46, 0, 1, "PRIO_MODE :   BCK");
    } else {
        eprintf(x, y + 0x46, 0, 1, "PRIO_MODE :   FWD");
    }
    if (sit->flag & 0x100) {
        eprintf(x, y + 0x54, 0, 1, "MIDI_TYPE :   BGM");
    } else {
        eprintf(x, y + 0x54, 0, 1, "MIDI_TYPE :    SE");
    }
    eprintf(x, y + 0x70, 0, 1, "VOL 0     : %5d", sit->rnd_no);
    eprintf(x, y + 0x7E, 0, 1, "VOL 1     : %5d", (s8) sit->se_flag);
    eprintf(x, y + 0x8C, 0, 1, "VOL 2     : %5d", (s8) sit->wall_vol);
    seq = Snd_search_seq_work_snd_id(Snd_test_work.snd_id);
    if (seq == NULL) {
        seq = &Snd_seq_work[0];
    }
    eprintf(0x18, y + 0x54, 0, 1, "BE_FLAG : %04X", seq->be_flag);
    eprintf(0x18, y + 0x62, 0, 1, "WORK_ID : %04X", seq->work_id);
    eprintf(0x18, y + 0x7E, 0, 1, "TOP SEQ : %08X", seq->top_seq_ptr);
    eprintf(0x18, y + 0x8C, 0, 1, "NOW SEQ : %08X", seq->now_seq_ptr);
    eprintf(0x18, y + 0x9A, 0, 1, "LOP SEQ : %08X", seq->lop_seq_ptr);
    eprintf(0x18, y + 0xA8, 0, 1, "TEMPO   : %8d", seq->tempo);
    eprintf(0x18, y + 0xB6, 0, 1, "TPM     : %8d", seq->tpm);
    eprintf(0x18, y + 0xC4, 0, 1, "D TIME  : %8d", seq->time);
    disp_seq_volume(seq);
}

// Prints the sequence work's volume.
void disp_seq_volume(SND_SEQ* seq)
{
    eprintf(0xD0, 0x16C, 0, 1, "OUT : %04XH", seq->out_vol);
    eprintf(0xD0, 0x17A, 0, 1, "NOW : %04XH", seq->now_vol);
    eprintf(0xD0, 0x196, 0, 1, "FDE : %04XH", seq->nml_fade_end);
    eprintf(0xD0, 0x1A4, 0, 1, "SPD : %0d", seq->nml_fade_spd);
}

// RIT screen: stream block / request numbers, the RIT fields and the stream status.
static void snd_test_disp_rit()
{
    SND_TEST* w = &Snd_test_work;
    SND_RBLK* blk = &Snd_str_blk[w->blk_no[w->tbl_type]];
    SND_RIT* rit = blk->rit_adrs;
    SND_SHD* shd;
    SND_STR* str;
    // The MONOPOLY row's y is `li r4,84; addi r4,r4,84` in the original: a single-set constant pseudo
    // (REG_EQUIV 84) whose init local-alloc's update_equiv_regs moves in front of its only use, because
    // substituting 84 into `y0 + 0x54` fails: validate_replace_rtx's "constant last" swap of
    // `(plus 84 84)` is a no-op (rtx_equal_p), so the PLUS is never folded (both constants must be equal).
    int y0 = 0x54;

    // rit is advanced in place: cse cannot rewrite the first rit-> load's address as
    // base+offset (the base register was overwritten), so combine forms the lhzux update load
    rit += w->req_now;
    {
        // one `blk->shd` load shared by the table index and the base (two loads let cse/local-alloc
        // tie the block-0 temporaries differently: r8/r9/r10/r11 rotated by one)
        u8* base = blk->sbh_adrs;
        shd = (SND_SHD*) (base + ((u32*) base)[rit->str_no]);
    }

    disp_cursor(w, 0x18, 0x54);
    str = Snd_search_str_work_snd_id(w->snd_id);
    if (str == NULL) {
        str = &Snd_str_work[str_get_player_id()];
    }
    eprintf(0x18, 0x54, 0, 1, "BE_FLAG : %04XH %02XH", str->be_flag, Snd_ctrl_work.dvd_err_flag);
    eprintf(0x18, 0x62, 0, 1, "RNO     : %02X %02X", str->rno, str->rno_sv);
    eprintf(0x18, 0x70, 0, 1, "DVD E/S : %02X %02X %2d", str->err_flag, str->dvd_status, str->dvd_comp);
    eprintf(0x18, 0x7E, 0, 1, "DVD NIE : %2d %2d %2d", str->dvd_req_num, str->dvd_req_idx, str->dvd_end_idx);
    eprintf(0x18, 0x8C, 0, 1, "DMA NIE : %2d %2d %2d", str->dma_req_num, str->dma_req_idx, str->dma_end_idx);
    eprintf(0x18, 0x9A, 0, 1, "PLY IDX : %2d %2d %03X", str->now_play_idx, str->old_play_idx, str->ttl_play_idx);
    eprintf(0x18, 0xB6, 0, 1, "TOP NBL : %08XH", str->lptop_nbl);
    eprintf(0x18, 0xC4, 0, 1, "END NBL : %08XH", str->lpend_nbl);
    eprintf(0x18, 0xE0, 0, 1, "ARM NBL : %08XH", str->aram_nbl);
    eprintf(0x18, 0xEE, 0, 1, "PLY NBL : %08XH", str->play_nbl);
    eprintf(0x18, 0xFC, 0, 1, "NXT NBL : %08XH", str->next_nbl);
    eprintf(0x18, 0x118, 0, 1, "ST SIZE : %08XH", str->file_size);
    eprintf(0x18, 0x126, 0, 1, "ST POS  : %08XH", str->file_pos);
    eprintf(0xD0, 0x54, 0, 1, "STR_NO    : %5d", rit->str_no);
    eprintf(0xD0, 0x62, 0, 1, "VOL_FLAG  : %5d", (s8) rit->pad_6[0]);
    eprintf(0xD0, 0x70, 0, 1, "VOL       : %5d", rit->vol);
    eprintf(0xD0, 0x7E, 0, 1, "STEREO    : %5d", (s8) rit->pad_B[0]);
    if (rit->flag & 1) {
        eprintf(0xD0, 0x8C, 0, 1, "STR_TYPE  :    SE");
    } else {
        eprintf(0xD0, 0x8C, 0, 1, "STR_TYPE  :   BGM");
    }
    eprintf(0xD0, 0x9A, 0, 1, "CH_NO     : %5d", rit->ch);
    eprintf(0xD0, y0 + 0x54, 0, 1, "MONOPOLY  : %5d", rit->poly);
    eprintf(0xD0, 0xB6, 0, 1, "PLAYER_ID : %5d", rit->pl_id);
    eprintf(0xD0, 0xC4, 0, 1, "AUX_A     : %5d", rit->aux_a);
    eprintf(0xD0, 0xD2, 0, 1, "AUX_B     : %5d", rit->aux_b);
    eprintf(0xD0, 0xE0, 0, 1, "PAN_FLAG  : %5d", (s8) rit->pad_6[1]);
    eprintf(0xD0, 0xEE, 0, 1, "PAN       : %5d", rit->pan);
    eprintf(0xD0, 0xFC, 0, 1, "SPAN      : %5d", rit->span);
    eprintf(0xD0, 0x10A, 0, 1, "SVOL      : %5d", rit->svol);
    eprintf(0xD0, 0x118, 0, 1, "SMP RATE  : %d", shd->rate);
    if (shd->flag & 4) {
        eprintf(0xD0, 0x126, 0, 1, "STR LOOP  :    ON");
    } else {
        eprintf(0xD0, 0x126, 0, 1, "STR LOOP  :   OFF");
    }
    disp_str_status(w, 0xD0, 0x54);
    eprintf(0xD0, 0x16C, 0, 1, "OUT : %04XH", str->out_vol);
    eprintf(0xD0, 0x17A, 0, 1, "NOW : %04XH", str->now_vol);
    eprintf(0xD0, 0x196, 0, 1, "FDE : %04XH", str->nml_fade_end);
    eprintf(0xD0, 0x1A4, 0, 1, "SPD : %0d", str->nml_fade_spd);
}

// first stream player in use (0 when none)
int str_get_player_id()
{
    int i;

    for (i = 0; i < 4; i++) {
        if (Snd_str_work[i].be_flag != 0) {
            return i;
        }
    }
    return 0;
}

// Blinking ">" at the parameter cursor row.
void disp_cursor(SND_TEST* w, int x, int y)
{
    u16 flag;
    int cur;
    int col;

    if (w->tbl_type == 1) {  // sic: the original tests the SIT copy's flag for the RIT table
        flag = w->sit.flag;
    } else {
        flag = w->rit.flag;
    }
    if (flag & 0x8000) {
        return;
    }
    cur = w->tbl_now;
    if (cur > 0xE) {
        cur -= 0xF;
        col = 0x27;
    } else {
        col = 0x12;
    }
    if (w->tbl_type == 1) {
        col += 3;
    }
    x += col * 8;
// COMPILER-DIFF: candidate #10 - the original build keeps separate .rodata copies of 1-char string
// literals per function group (3x ">", 2x "<", 2x "D" in this unit) while ours merges them; the
// explicit "\0" padding forces distinct constants with identical bytes.
    y += cur * 14;
    eprintf(x, y, 4, 1, ">\0");
    eprintf(x + 0xA0, y, 4, 1, "<\0");
}

static char* str_state_name[7] = {"WAIT   ", "READY  ", "PLAY   ", "NO READ", "CLOSE  ", "IDLE   ", "ERROR  "};
static u8 str_state_col[8] = {0, 0, 4, 0, 7, 7, 2, 0};

// Prints the stream channels' state (playing / block / volume).
void disp_str_status(SND_TEST* w, int x, int y)
{
    int i;

    x += 0xA8;
    for (i = 0; i < 4; i++) {
        SND_STR* str = &Snd_str_work[i];

        if (str->be_flag == 0) {
            eprintf(x, y, 7, 1, "%02d : NO MOVE\n", i);
        } else {
            eprintf(x, y, str_state_col[str->rno], 1, "%02d : %s\n", i, str_state_name[str->rno]);
        }
        y += 0xE;
    }
}

// sequencer voices playing MIDI channel `ch`. `work` is a reference to the caller's `const` pointer
// local: taking the address of a folded const scalar (`&voices`, DECL_RTL never made) forces its
// initializer into the constant pool (expr.c ADDR_EXPR -> force_const_mem), which is the `.4byte
// Snd_voice_work` word at the end of disp_sequencer's pool that the loop preheader reads with
// `lis; addi; lwz 0()`.
static inline int seq_note_count(int ch, SND_VOICE* const& work)
{
    SND_VOICE* vw = work;
    int notes = 0;
    int i;

    for (i = 0; i < 64; i++) {
        if (vw->be_flag != 0 && vw->play_type == 2 && vw->track == ch) {
            notes++;
        }
        vw++;
    }
    return notes;
}

// Prints the sequencer channels (program, note, volume per channel).
void disp_sequencer()
{
    SND_SEQ* seq;
    int ch;
    int y0;

    seq = Snd_search_seq_work_snd_id(Snd_test_work.snd_id);
    if (seq == NULL) {
        seq = &Snd_seq_work[0];
    }
    eprintf(0x18, 0x54, 0, 1, "TRACK    :");
    eprintf(0x18, 0x62, 0, 1, "NOTE ON  :");
    eprintf(0x18, 0x70, 0, 1, "PRIO     :");
    eprintf(0x18, 0x7E, 0, 1, "PROG NO  :");
    eprintf(0x18, 0x8C, 0, 1, "CH_VOL   :");
    eprintf(0x18, 0x9A, 0, 1, "EXPRESS  :");
    eprintf(0x18, 0xA8, 0, 1, "PAN      :");
    eprintf(0x18, 0xB6, 0, 1, "PITCH M  :");
    eprintf(0x18, 0xC4, 0, 1, "PITCH L  :");
    eprintf(0x18, 0xD2, 0, 1, "P SEN M  :");
    eprintf(0x18, 0xE0, 0, 1, "P SEN L  :");
    eprintf(0x18, 0xEE, 0, 1, "MODULATE :");
    eprintf(0x18, 0xFC, 0, 1, "HOLD     :");
    eprintf(0x18, 0x10A, 0, 1, "AUX A    :");
    eprintf(0x18, 0x118, 0, 1, "AUX B    :");
    // `y0`: the PAN row's y is `li r9,84; addi r27,r9,84` at the top of the join block in the
    // original: a single-set constant pseudo (REG_EQUIV 84, rematerialised at its use) set
    // before the loop, added after the D diamond's join label (cse cannot fold across it). Its
    // `li` is also the first free filler of the preheader (LUID before `ch = 0`), which is what
    // puts the two spilled row pointers (ch_flag/ch_prio) one header eprintf later.
    y0 = 0x54;
    for (ch = 0; ch < 16; ch++) {
        int x = 0x70 + ch * 0x18;
        SND_VOICE* const voices = Snd_voice_work;
        int y;

        // `ch + 1`: the target passes r10 = ch+1 to the label eprintf2 and keeps that value
        // (`mr r26,r10`) as the loop's next ch
        eprintf2(6, 13, x, 0x54, 0, 1, "%03d", ch + 1);
        if (seq->flag[ch] & 1) {
            eprintf2(6, 13, x, 0x54, 4, 1, "D");
        }
        y = y0 + 0x54;
        eprintf2(6, 13, x, 0x62, 0, 1, "%3d", seq_note_count(ch, voices));
        eprintf2(6, 13, x, 0x70, 0, 1, "%3d", seq->prio[ch]);
        eprintf2(6, 13, x, 0x7E, 0, 1, "%3d", seq->prog[ch] + 1);
        eprintf2(6, 13, x, 0x8C, 0, 1, "%3d", (s8) seq->vol[ch]);
        eprintf2(6, 13, x, 0x9A, 0, 1, "%3d", (s8) seq->exp[ch]);
        eprintf2(6, 13, x, y, 0, 1, "%3d", seq->pan[ch]);
        eprintf2(6, 13, x, 0xB6, 0, 1, "%3d", seq->pitch_m[ch]);
        eprintf2(6, 13, x, 0xC4, 0, 1, "%3d", seq->pitch_l[ch]);
        eprintf2(6, 13, x, 0xD2, 0, 1, "%3d", seq->pitch_sen_m[ch]);
        eprintf2(6, 13, x, 0xE0, 0, 1, "%3d", seq->pitch_sen_l[ch]);
        eprintf2(6, 13, x, 0xEE, 0, 1, "%3d", (s8) seq->modulation[ch]);
        eprintf2(6, 13, x, 0xFC, 0, 1, "%3d", (s8) seq->hold[ch]);
        eprintf2(6, 13, x, 0x10A, 0, 1, "%3d", (s8) seq->aux_a[ch]);
        eprintf2(6, 13, x, 0x118, 0, 1, "%3d", (s8) seq->aux_b[ch]);
    }
    disp_seq_volume(seq);
}

// Prints the wavetable instrument / region / articulation / sample data of the SIT's program.
void disp_se_wt_data(SND_TEST* w, SND_IBLK* blk, SND_SIT* sit)
{
    u32 ofs;
    u32 len;

    get_wt_ptr(w, blk, sit);
    eprintf(0x18, 0xA8, 0, 1, "- WTREGION - ");
    eprintf(0x18, 0xB6, 0, 1, "ART    IDX : %d", w->rgp->articulationIndex);
    eprintf(0x18, 0xC4, 0, 1, "SAMPLE IDX : %d", w->rgp->sampleIndex);
    eprintf(0x18, 0xD2, 0, 1, "unityNote  : %d", w->rgp->unityNote);
    eprintf(0x18, 0xE0, 0, 1, "keyGroup   : %d", w->rgp->keyGroup);
    eprintf(0x18, 0xEE, 0, 1, "fineTune   : %d", w->rgp->fineTune);
    eprintf(0x18, 0xFC, 0, 1, "attn       : %08XH", w->rgp->attn);
    eprintf(0x18, 0x10A, 0, 1, "             %d", (s16) (w->rgp->attn >> 16));
    eprintf(0x18, 0x118, 0, 1, "loopStart  : %08XH", w->rgp->loopStart);
    eprintf(0x18, 0x126, 0, 1, "loopLength : %08XH", w->rgp->loopLength);
    eprintf(0xB8, 0x54, 0, 1, "- WTART - ");
    eprintf(0xB8, 0x62, 0, 1, "Attack     : %08XH", w->atp->eg1Attack);
    eprintf(0xB8, 0x70, 0, 1, "Decay      : %08XH", w->atp->eg1Decay);
    eprintf(0xB8, 0x7E, 0, 1, "Sustain    : %08XH", w->atp->eg1Sustain);
    eprintf(0xB8, 0x8C, 0, 1, "Release    : %08XH", w->atp->eg1Release);
    eprintf(0xB8, 0x9A, 0, 1, "Vel2Attack : %08XH", w->atp->eg1Vel2Attack);
    eprintf(0xB8, 0xA8, 0, 1, "Key2Decay  : %08XH", w->atp->eg1Key2Decay);
    eprintf(0xB8, 0xB6, 0, 1, "pan        : %08XH", w->atp->pan);
    ofs = w->smp->offset / 2;
    len = (w->smp->length / 2 / 7) * 16 + w->smp->length % 14;
    len /= 2;
    eprintf(0xD8, 0xEE, 0, 1, "- WTSAMPLE - ");
    eprintf(0xD8, 0xFC, 0, 1, "sampleRate : %d Hz", w->smp->sampleRate);
    eprintf(0xD8, 0x10A, 0, 1, "offset     : %08XH (%08XH)", ofs, w->smp->offset);
    eprintf(0xD8, 0x118, 0, 1, "length     : %08XH (%08XH)", len, w->smp->length);
    eprintf(0xD8, 0x126, 0, 1, "ADPCM IDX  : %d", w->smp->adpcmIndex);
    disp_adsr_para(w);
}

// Resolves the SIT's program to its WT instrument / region / art / sample / ADPCM pointers.
void get_wt_ptr(SND_TEST* w, SND_IBLK* blk, SND_SIT* sit)
{
    WTFILEHEADER* hdr = (WTFILEHEADER*) blk->wt_adrs;

    w->fhp = (WTFILEHEADER*) blk->wt_adrs;
    w->isp = (WTINST*) (blk->wt_adrs + hdr->offsetMelodicInst);
    w->isp = (WTINST*) ((u8*) w->isp + (sit->note & 0xFF00));
    w->rgp = (WTREGION*) (blk->wt_adrs + hdr->offsetRegions);
    w->rgp += w->isp->keyRegion[sit->note & 0xFF];
    w->atp = (WTART*) (blk->wt_adrs + hdr->offsetArticulations);
    w->atp += w->rgp->articulationIndex;
    w->smp = (WTSAMPLE*) (blk->wt_adrs + hdr->offsetSamples);
    w->smp += w->rgp->sampleIndex;
    w->adp = (WTADPCM*) (blk->wt_adrs + hdr->offsetAdpcmContext);
    w->adp += w->smp->adpcmIndex;
}

// Prints the ADSR parameters of the current voice (axv).
void disp_adsr_para(SND_TEST* w)
{
    SND_AXV* axv;

    get_axv_ptr(w);
    axv = w->axv_ptr;
    if (axv == NULL) {
        axv = &Snd_axv_work[0];
    }
    eprintf(0x178, 0x54, 0, 1, "- ADSR -");
    eprintf(0x178, 0x62, 0, 1, "ADSR CTR : %4d", (s16) axv->adsr_ctr);
    eprintf(0x178, 0x70, 0, 1, "ADSR ATK : %4d", axv->adsr_atk);
    eprintf(0x178, 0x7E, 0, 1, "ADSR REL : %4d", axv->adsr_rel);
    eprintf(0x178, 0x8C, 0, 1, "ADSR VOL : %04XH", axv->adsr_vol);
    eprintf(0x178, 0x9A, 0, 1, "ADSR END : %04XH", axv->adsr_end);
    eprintf(0x178, 0xA8, 0, 1, "ADSR SPD : %04XH", axv->adsr_spd);
    eprintf(0x178, 0xB6, 0, 1, "NOW  VOL : %04XH", axv->now_vol);
    eprintf(0x178, 0xC4, 0, 1, "OUT  VOL : %04XH", axv->out_vol);
}

// Finds the AX voice work playing sndId (w->axv, NULL when none).
void get_axv_ptr(SND_TEST* w)
{
    SND_VOICE* vw;

    if (w->snd_id == 0) {
        return;
    }
    vw = Snd_search_voice_work_snd_id(w->snd_id);
    if (vw == NULL) {
        return;
    }
    if (vw->axv_ptr == NULL) {
        return;
    }
    w->axv_ptr = vw->axv_ptr;
}

// Draws the mode menu with the cursor.
void Snd_test_disp_menu(SND_TEST* w)
{
    int y;

    eprintf(0xA8, 0x54, 0, 1, "  SOUND TEST MODE MENU");
    eprintf(0xA8, 0x70, 0, 1, "  SIT   - I.S.S. PLAY");
    eprintf(0xA8, 0x7E, 0, 1, "  RIT   - STREAM PLAY");
    eprintf(0xA8, 0x8C, 0, 1, "  AUX A - EFFECT SET");
    eprintf(0xA8, 0x9A, 0, 1, "  AUX B - EFFECT SET");
    eprintf(0xA8, 0xA8, 0, 1, "  VOL   - SYSTEM VOLUME");
    eprintf(0xA8, 0xB6, 0, 1, "  DUMP  - A-RAM  DUMP");
    eprintf(0xA0, 0xFC, 0, 1, "SOUND PROGRAM %s", "Version 1.910");
    eprintf(0xA0, 0x118, 0, 1, "GAMECUBE SDK  %s", "20Apr2004Patch1");
    y = w->rno00 * 14 + 0x70;
    eprintf(0xA8, y, 4, 1, ">");
    eprintf(0x168, y, 4, 1, "<");
}

// Common footer: sound mode (MONO / STEREO / DPL2), the voice map (active voices highlighted), the
// table / block / request numbers, the master / ISS / STR volume pairs and the voice count / peak.
void Snd_test_disp_basic(SND_TEST* w)
{
    // The original passes a stale r3 to the 2nd/3rd call (no `mr r3,r31` reload): the argument
    // was a hard-register variable, which GCC 2.95 does not restore after a call.
    register SND_TEST* p asm("r3") = w;

    if (w->disp_flag & 2) {
        Snd_test_disp_voice(p);
    }
    if (w->disp_flag & 1) {
        Snd_test_disp_req_para(p);
    }
    if (w->disp_flag & 4) {
        Snd_test_disp_aux(p);
    }
}

static char* sound_mode_name[3] = {"MONO  ", "STEREO", "DPL2  "};

// 8x8 map of the voice slots
void Snd_test_disp_voice(SND_TEST* w)
{
    int i;
    int j;

    eprintf(0x1C0, 0x134, 0, 1, sound_mode_name[Snd_ctrl_work.snd_mode]);
    for (i = 0; i < 8; i++) {
        // x is a per-row statement; n and y are inner-body expressions (y a reduced giv, n
        // computed in the loop header) and the voice is indexed by n: the address giv's
        // increment lands right after the load (auto_inc_opt) and its init is emitted by
        // loop.c after gcse's insertions, so i dies there and not at the PRE'd i+1
        int x = 0x138 + i * 0x18;

        for (j = 0; j < 8; j++) {
            int n = i * 8 + j;
            int y = 0x142 + j * 0xE;
            if (Snd_voice_work[n].be_flag != 0) {
                eprintf(x, y, 4, 1, "%02d", n);
            } else {
                eprintf(x, y, 7, 1, "%02d", n);
            }
        }
    }
}

static char* tbl_name[2] = {"I.S.S.(SIT)", "STREAM(RIT)"};

// the parameter is ignored: the original reads the global work and the control work through locals
void Snd_test_disp_req_para(SND_TEST* unused)
{
    SND_TEST* w = &Snd_test_work;
    SND_CTRL* ctrl = &Snd_ctrl_work;

    eprintf(0x18, 0x142, 0, 1, "TBL    : %s", tbl_name[w->tbl_type]);
    eprintf(0x18, 0x150, 0, 1, "BLK_NO : %4d / %4d", w->blk_no[w->tbl_type], w->blk_max[w->tbl_type] - 1);
    eprintf(0x18, 0x15E, 0, 1, "REQ_NO : %4d / %4d", w->req_now, w->req_max[w->tbl_type] - 1);
    eprintf(0x18, 0x17A, 0, 1, "MASTER : %4d / %4d", ctrl->vol_mas_bgm >> 8, ctrl->vol_mas_se >> 8);
    eprintf(0x18, 0x188, 0, 1, "ISS    : %4d / %4d", ctrl->vol_iss_bgm >> 8, ctrl->vol_iss_se >> 8);
    eprintf(0x18, 0x196, 0, 1, "STR    : %4d / %4d", ctrl->vol_str_bgm >> 8, ctrl->vol_str_se >> 8);
    eprintf(0x18, 0x1A4, 0, 1, "VOICE  : %4d / %4d", ctrl->now_voice, ctrl->max_voice);
}

static u8 efx_type_col[8] = {7, 4, 4, 4, 4, 4, 0, 2};
static char* efx_type_name[8] = {"EFX OFF  ", "REV HI   ", "REV STD. ", "CHORUS   ", "DELAY    ", "REV DPL2 ", "EFX STOP ", "EFX ERROR"};

// AUX screen: slot, effect type / state and the parameters (test_disp_efx_tbl by type).
void Snd_test_disp_aux(SND_TEST* w)
{
    s16 type;

    type = Snd_efx_get_type(0);
    eprintf(0xD0, 0x142, efx_type_col[type], 1, "%d %s", type, efx_type_name[type]);
    type = Snd_efx_get_type(1);
    eprintf(0xD0, 0x150, efx_type_col[type], 1, "%d %s", type, efx_type_name[type]);
}

static char* aux_name[2] = {"AUX A", "AUX B"};
static char* efx_name[6] = {"NO EFFECT  ", "REVERB HI  ", "REVERB STD.", "CHORUS     ", "DELAY      ", "REVERB DPL2"};
// Effect parameter printers by effect type.
static void (*test_disp_efx_tbl[7])(SND_TEST*, SND_EFX*, int, int) = {
    NULL, test_disp_efx_rev_hi, test_disp_efx_rev_std, test_disp_efx_chorus, test_disp_efx_delay, test_disp_efx_rev_dpl2, NULL};

// Prints both AUX slots' effect type and state.
void Snd_test_disp_efx()
{
    SND_TEST* w = &Snd_test_work;
    SND_EFX* efx = &Snd_efx_work[w->aux_type];
    int col;
    int y;

    eprintf(0x18, 0x54, 0, 1, aux_name[w->aux_type]);
    col = 0;
    if (w->efx_ok[w->aux_type] > 0) {
        col = 4;
    }
    eprintf(0x18, 0x70, col, 1, efx_name[w->efx_now]);
    if (w->efx_now == 0) {
        return;
    }
    switch (w->efx_ok[w->aux_type]) {
    case 1:
        eprintf(0x18, 0x8C, 4, 1, "EXECUTED.");
        eprintf(0x18, 0x10A, 0, 1, "USE MEMORY is %7d (0x%06XH) Bytes.", efx->efx_mem, efx->efx_mem);
        break;
    case 0:
        if (w->timer & 0x10) {
            eprintf(0x18, 0x8C, 0, 1, "CHANGING.");
        }
        break;
    case -1:
        eprintf(0x18, 0x8C, 7, 1, "STOPPED.");
        break;
    }
    y = w->aux_now * 14 + 0x54;
    eprintf(0x80, y, 4, 1, ">");
    y = w->aux_now * 14 + 0x54;
    eprintf(0x1D8, y, 4, 1, "<");
    test_disp_efx_tbl[w->efx_now](w, efx, 0x90, 0x54);
    if (w->tbl_type == 0) {
        eprintf(0x90, 0xE0, 0, 1, "I.S.S.(SIT) AUX A : %3d", w->sit.aux_a);
        eprintf(0x90, 0xEE, 0, 1, "I.S.S.(SIT) AUX B : %3d", w->sit.aux_b);
    } else {
        eprintf(0x90, 0xE0, 0, 1, "Stream(RIT) AUX A : %3d", w->rit.aux_a);
        eprintf(0x90, 0xEE, 0, 1, "Stream(RIT) AUX B : %3d", w->rit.aux_b);
    }
}

// Reverb HI parameters (the float table).
static void test_disp_efx_rev_hi(SND_TEST* w, SND_EFX* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->rev_hi.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->rev_hi.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->rev_hi.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->rev_hi.damping);
    eprintf(x, y + 0x38, 0, 1, "CROSSTALK   ( 0.000 -  1.000) : %2.4fF", efx->rev_hi.crosstalk);
    eprintf(x, y + 0x46, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->rev_hi.mix);
}

// Reverb STD parameters.
static void test_disp_efx_rev_std(SND_TEST* w, SND_EFX* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->rev_std.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->rev_std.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->rev_std.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->rev_std.damping);
    eprintf(x, y + 0x38, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->rev_std.mix);
}

// Chorus parameters.
static void test_disp_efx_chorus(SND_TEST* w, SND_EFX* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "BASEDELAY   (     5 -     15) : %8d", efx->chorus.baseDelay);
    eprintf(x, y + 0xE, 0, 1, "VARIATION   (     0 -      5) : %8d", efx->chorus.variation);
    eprintf(x, y + 0x1C, 0, 1, "PERIOD      (   500 -  10000) : %8d", efx->chorus.period);
}

// Delay parameters (per channel).
static void test_disp_efx_delay(SND_TEST* w, SND_EFX* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "DELAY[0]    (    10 -   5000) : %8d", efx->delay.delay[0]);
    eprintf(x, y + 0xE, 0, 1, "DELAY[1]    (    10 -   5000) : %8d", efx->delay.delay[1]);
    eprintf(x, y + 0x1C, 0, 1, "DELAY[2]    (    10 -   5000) : %8d", efx->delay.delay[2]);
    eprintf(x, y + 0x2A, 0, 1, "FEEDBACK[0] (     0 -    100) : %8d", efx->delay.feedback[0]);
    eprintf(x, y + 0x38, 0, 1, "FEEDBACK[1] (     0 -    100) : %8d", efx->delay.feedback[1]);
    eprintf(x, y + 0x46, 0, 1, "FEEDBACK[2] (     0 -    100) : %8d", efx->delay.feedback[2]);
    eprintf(x, y + 0x54, 0, 1, "OUTPUT[0]   (     0 -    100) : %8d", efx->delay.output[0]);
    eprintf(x, y + 0x62, 0, 1, "OUTPUT[1]   (     0 -    100) : %8d", efx->delay.output[1]);
    eprintf(x, y + 0x70, 0, 1, "OUTPUT[2]   (     0 -    100) : %8d", efx->delay.output[2]);
}

// DPL2 reverb parameters.
static void test_disp_efx_rev_dpl2(SND_TEST* w, SND_EFX* efx, int x, int y)
{
    eprintf(x, y, 0, 1, "PREDELAY    ( 0.000 -  0.100) : %2.4fF", efx->rev_dpl2.preDelay);
    eprintf(x, y + 0xE, 0, 1, "TIME        ( 0.010 - 10.000) : %2.4fF", efx->rev_dpl2.time);
    eprintf(x, y + 0x1C, 0, 1, "COLORATION  ( 0.000 -  1.000) : %2.4fF", efx->rev_dpl2.coloration);
    eprintf(x, y + 0x2A, 0, 1, "DAMPING     ( 0.000 -  1.000) : %2.4fF", efx->rev_dpl2.damping);
    eprintf(x, y + 0x38, 0, 1, "MIX         ( 0.000 -  1.000) : %2.4fF", efx->rev_dpl2.mix);
}

// VOL screen: the six system volumes with the cursor.
void Snd_test_disp_vol()
{
    SND_TEST* w = &Snd_test_work;
    SND_CTRL* ctrl = &Snd_ctrl_work;
    int y;

    y = w->vol_idx * 14 + 0x54;
    eprintf(0xA8, y, 4, 1, ">");
    y = w->vol_idx * 14 + 0x54;
    eprintf(0x160, y, 4, 1, "<");
    eprintf(0xB8, 0x54, 0, 1, "MASTER VOL BGM : %3d", ctrl->vol_mas_bgm >> 8);
    eprintf(0xB8, 0x62, 0, 1, "MASTER VOL SE  : %3d", ctrl->vol_mas_se >> 8);
    eprintf(0xB8, 0x70, 0, 1, "I.S.S. VOL BGM : %3d", ctrl->vol_iss_bgm >> 8);
    eprintf(0xB8, 0x7E, 0, 1, "I.S.S. VOL SE  : %3d", ctrl->vol_iss_se >> 8);
    eprintf(0xB8, 0x8C, 0, 1, "STREAM VOL BGM : %3d", ctrl->vol_str_bgm >> 8);
    eprintf(0xB8, 0x9A, 0, 1, "STREAM VOL SE  : %3d", ctrl->vol_str_se >> 8);
}

// LOAD mode entry: opens the disc directory of the current table.
void Snd_test_load_init(SND_TEST* w)
{
    directory_open(w);
}

// LOAD mode: C stick picks the target block, the directory list scrolls with the d-pad, A loads
// the selected file (or enters the directory), B goes up; the list and the block files are drawn.
void Snd_test_mode_load(SND_TEST* w)
{
    int moved;

    moved = blk_no_check(w);
    if (moved == 0) {
        if (w->joy_trg & 0x100) {
            load_select(w);
            moved = 1;
        }
        if (moved == 0) {
            int step = 1;

            if (w->joy_now & 0x20) {
                step = 10;
            }
            if (w->joy_rep & 8) {
                w->file_now -= step;
            }
            if (w->joy_rep & 4) {
                w->file_now += step;
            }
            if (w->file_now < 0) {
                w->file_now = w->file_max - 1;
            }
            if (w->file_now >= w->file_max) {
                w->file_now = 0;
            }
            w->file_top = w->file_now / 10 * 10;
        }
    }
    directory_disp(w);
    cursor_disp(w);
    blk_file_disp(w);
}

// block number of the load target (sub stick); 1 when it changed
int blk_no_check(SND_TEST* w)
{
    s16 max;
    s16 no;
    s16 old;

    if (w->load_type == 0) {
        max = 4;
    } else {
        max = w->blk_max[w->load_type];
    }
    no = w->blk_no[w->load_type];
    old = no;
    if (w->joy_rep & 0x80000) {
        no--;
    }
    if (w->joy_rep & 0x40000) {
        no++;
    }
    if (no < 0) {
        no = max - 1;
    }
    if (no >= max) {
        no = 0;
    }
    if (no != old) {
        w->blk_no[w->load_type] = no;
        return 1;
    }
    return 0;
}

// A on a directory entry: enters it, or loads the .SND / stream file into the current block
// (load_sit_data / load_rit_data).
void load_select(SND_TEST* w)
{
    s8 tbl = w->load_type;
    int cur;

    if (get_dir_level(w->dir_name[tbl]) != 0 && w->file_now == 0) {
        dir_name_up(w);
        directory_open(w);
        return;
    }
    cur = w->file_now;
    if (w->file_dir[cur] != 0) {
        char* path = w->dir_name[tbl];

        strcat(path, w->file_name[cur]);
        strcat(path, "/");
        change_to_cap(path);
        directory_open(w);
        return;
    }
    if (w->load_type == 0) {
        load_sit_data(w, w->file_name[cur]);
    } else {
        load_rit_data(w, w->file_name[cur]);
    }
}

// Loads an ISS bank file into ISS block blkNo[0].
void load_sit_data(SND_TEST* w, char* name)
{
}

// Loads a stream table file into stream block blkNo[1].
void load_rit_data(SND_TEST* w, char* name)
{
}

static char dir_up_name[4] = "..";

// Reads the current path's disc directory into dirName / dirIsDir (up to 128 entries; ".." when
// below the root), cursor reset.
void directory_open(SND_TEST* w)
{
    w->file_max = 0;
    w->file_top = 0;
    w->file_now = 0;
    if (get_dir_level(w->dir_name[w->load_type]) != 0) {
        w->file_name[0] = dir_up_name;
        w->file_dir[0] = 1;
        w->file_max++;
    }
    dir_entry_read(w, 1);
    dir_entry_read(w, 0);
}

static char* blk_ext_name[3] = {"BLK", "BKR", NULL};

// appends the directories (dirs = 1) or the block files of the current path to the list
void dir_entry_read(SND_TEST* w, int dirs)
{
    char name[0x100];

    if (DVDOpenDir(w->dir_name[w->load_type], &w->dir) == 0) {
        return;
    }
    for (;;) {
        // the original returns WITHOUT DVDCloseDir when the list is full (the dirNum test
        // jumps to the epilogue); the loop is rotated around the DVDReadDir break
        if (w->file_max > 0x7F) {
            return;
        }
        if (DVDReadDir(&w->dir, &w->entry) == 0) {
            break;
        }
        if (dirs) {
            if (w->entry.isDir == 0) {
                continue;
            }
        } else {
            if (w->entry.isDir != 0) {
                continue;
            }
            strcpy(name, w->entry.name);
            change_to_cap(name);
            if (strcmp(get_file_ext(name), blk_ext_name[w->load_type]) != 0) {
                continue;
            }
        }
        w->file_name[w->file_max] = w->entry.name;
        w->file_dir[w->file_max] = w->entry.isDir;
        w->file_max++;
    }
    DVDCloseDir(&w->dir);
}

// Draws the directory listing window around the cursor.
void directory_disp(SND_TEST* w)
{
    char name[0x100];
    int i;
    int end;
    int y = 0x62;

    eprintf(0x18, 0x54, 0, 1, "DIR : %s", w->dir_name[w->load_type]);
    eprintf(0x18, 0x62, 0, 1, "NO. : %3d", w->file_now);
    eprintf(0x18, 0x70, 0, 1, "MAX : %3d", w->file_max - 1);
    end = w->file_top + 10;
    if (end > w->file_max) {
        end = w->file_max;
    }
    for (i = w->file_top; i < end; i++) {
        strcpy(name, w->file_name[i]);
        change_to_cap(name);
        if (w->file_dir[i] != 0) {
            eprintf(0xA8, y, 4, 1, "D\0");
        } else {
            eprintf(0xA8, y, 0, 1, "F");
        }
        eprintf(0xB8, y, 0, 1, "%s", name);
        y += 0xE;
    }
}

// ">" at the directory cursor.
void cursor_disp(SND_TEST* w)
{
    eprintf(0x98, (w->file_now - w->file_top + 7) * 14, 4, 1, ">\0\0");
}

// Lists the loaded blocks of the current table with their file names.
void blk_file_disp(SND_TEST* w)
{
    char name[0x100];
    s16 blk = w->blk_no[w->load_type];
    char* file;

    if (w->load_type == 0) {
        eprintf(0x18, 0x10A, 0, 1, "SIT BLK[%02d] :", blk);
        file = w->iss_blk_name[blk];
    } else {
        eprintf(0x18, 0x10A, 0, 1, "RIT BLK[%02d] :", blk);
        file = w->str_blk_name[blk];
    }
    if (file == NULL) {
        eprintf(0x88, 0x10A, 7, 1, "%s", "NO DATA");
    } else {
        strcpy(name, file);
        change_to_cap(name);
        eprintf(0x88, 0x10A, 0, 1, "%s", name);
    }
    // dead code: the original's .rodata keeps an "sbb" literal between "NO DATA" and aram_dump_disp's
    // strings; the code that used it is gone
    if (0) {
        eprintf(0x88, 0x10A, 0, 1, "sbb");
    }
}

// removes the last directory of the current path
void dir_name_up(SND_TEST* w)
{
    char* path = w->dir_name[w->load_type];
    int last = 0;
    int i;

    for (i = 0; i <= 0xFF; i++) {
        if (path[i] == '/') {
            if (*(path + i + 1) == 0) {
                break;
            }
            last = i;
        }
    }
    *(path + last + 1) = 0;
}

// Upper-cases a string in place.
void change_to_cap(char* s)
{
    char c = *s;

    while (c != 0) {
        if (c >= 'a' && c <= 'z') {
            *s = c - 0x20;
        }
        c = *++s;
    }
}

// Pointer to the extension after the last '.', or the end of the string.
char* get_file_ext(char* s)
{
    char c;

    do {
        c = *s++;
    } while (c != '.');
    return s;
}

// Directory depth of a path (number of '/').
int get_dir_level(char* s)
{
    s8 level = 0;
    char c;

    while ((c = *s++) != 0) {
        if (c == '/') {
            level++;
        }
    }
    return level;
}

// DUMP mode entry: fetches the first ARAM page.
void Snd_test_dump_init(SND_TEST* w)
{
    aram_dump_dma(w);
    aram_dump_disp(w);
}

// ARAM DUMP: d-pad / stick step the address by 0x100 .. 0x100000 (X resets to the sound ARAM
// base), C stick jumps to a loaded block; a changed address DMAs the page and it is hex-dumped.
void Snd_test_aram_dump(SND_TEST* w)
{
    u32 old = w->aram_adrs;

    aram_blk_no_select(w);
    if (w->joy_now & 0x20) {
        if (w->joy_rep & 1) {
            w->aram_adrs -= 0x100000;
        }
        if (w->joy_rep & 2) {
            w->aram_adrs += 0x100000;
        }
        if (w->joy_rep & 8) {
            w->aram_adrs -= 0x10000;
        }
        if (w->joy_rep & 4) {
            w->aram_adrs += 0x10000;
        }
    } else {
        if (w->joy_rep & 1) {
            w->aram_adrs -= 0x1000;
        }
        if (w->joy_rep & 2) {
            w->aram_adrs += 0x1000;
        }
        if (w->joy_rep & 8) {
            w->aram_adrs -= 0x100;
        }
        if (w->joy_rep & 4) {
            w->aram_adrs += 0x100;
        }
    }
    if (w->joy_trg & 0x400) {
        w->aram_adrs = Snd_ctrl_work.zero_adrs;
    }
    if ((s32) w->aram_adrs < 0) {
        w->aram_adrs = 0x00FFFF00;
    }
    if ((s32) w->aram_adrs > 0x00FFFFFF) {
        w->aram_adrs = 0;
    }
    if (old != w->aram_adrs) {
        aram_dump_dma(w);
    }
    aram_dump_disp(w);
}

// C stick up/down: the dump address jumps to the previous / next loaded ISS block.
void aram_blk_no_select(SND_TEST* w)
{
    if (w->tbl_type == 1) {
        return;
    }
    if (w->joy_trg & 0x80000) {
        blk_enable_ck(w, -1);
    }
    if (w->joy_trg & 0x40000) {
        blk_enable_ck(w, 1);
    }
}

// Finds the next (dir) loaded ISS block from blkNo[0] and sets the dump address to it.
void blk_enable_ck(SND_TEST* w, int dir)
{
    int i;
    s16 max = w->blk_max[w->tbl_type];
    int no = w->blk_no[w->tbl_type];

    for (i = 0; i < max; i++) {
        no += dir;
        if (no < 0) {
            no = max - 1;
        }
        if (no >= max) {
            no = 0;
        }
        if (Snd_iss_blk[no].sit_num != 0) {
            w->blk_no[w->tbl_type] = no;
            w->req_no[w->tbl_type] = 0;
            break;
        }
    }
    w->aram_adrs = Snd_iss_blk[w->blk_no[w->tbl_type]].pcm_adrs & ~0xFF;
}

// Starts the ARAM -> MRAM DMA of the 0x100-byte page at aramAdrs.
void aram_dump_dma(SND_TEST* w)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    ctrl->arq_flag = 1;
    ARQPostRequest(&ctrl->arq_req, 0, 1, 0, w->aram_adrs, (u32) Snd_test_work.aram_dump, 0x100, cb_dma_end);
    do {
    } while (ctrl->arq_flag != 0);
    DCInvalidateRange(w->aram_dump, 0x100);
}

// ARQ callback: marks the dump page ready.
static void cb_dma_end(u32 task)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    ctrl->arq_flag = 0;
}

// Hex dump of the fetched ARAM page with its address.
void aram_dump_disp(SND_TEST* w)
{
    u32 i;
    u32 j;
    int n;
    int x;

    x = 0x68;
    for (i = 0; i < 16; i++) {
        eprintf(x, 0x46, 4, 1, "%02X", i);
        x += 0x18;
    }
    n = 0;
    for (i = 0; i < 16; i++) {
        int y = 0x54 + i * 14;

        eprintf(0x20, y, 4, 1, "%08X", w->aram_adrs + i * 16);
        x = 0x68;
        for (j = 0; j < 16; j++) {
            eprintf(x, y, 0, 1, "%02X", w->aram_dump[n]);
            x += 0x18;
            n++;
        }
    }
    if (w->tbl_type != 1) {
        eprintf(0xD0, 0x16C, 0, 1, "PCM ADDRESS");
        eprintf(0xE0, 0x188, 0, 1, "%08XH", Snd_iss_blk[w->blk_no[w->tbl_type]].pcm_adrs);
    }
}
