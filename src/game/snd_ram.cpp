#include "snd_drv.h"

// Sound library work areas, in the order the other snd_*.cpp units expect them.
SND_CTRL Snd_ctrl_work;
SND_EFX Snd_efx_work[2];
SND_REQ Snd_req_work[SND_REQ_BANK_MAX][SND_REQ_MAX];
SND_VOICE Snd_voice_work[SND_VOICE_MAX];
SND_AXV Snd_axv_work[SND_AXV_MAX];
SND_IBLK Snd_iss_blk[SND_ISS_BLK_MAX];
SND_RBLK Snd_str_blk[SND_STR_BLK_MAX];
SND_SEQ Snd_seq_work[SND_SEQ_MAX];
SND_STR Snd_str_work[SND_STR_MAX];
u8* Snd_str_buff[SND_STR_MAX];
SND_TEST Snd_test_work __attribute__((aligned(32)));
