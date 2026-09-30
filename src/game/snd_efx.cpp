// game/snd_efx: sound driver AUX effects — the two AUX buses (A / B) each carry one AXFX effect
// (1 reverb HI, 2 reverb STD, 3 chorus, 4 delay, 5 DPL2 reverb); Snd_efx_req switches / updates /
// stops them, the game sets the parameters in Snd_efx_work[].fx first (snd.cpp SndSetReverb).
#include "snd_drv.h"

// Clears both effect works (aux 0 / 1, no effect).
void Snd_efx_work_clear(void)
{
    SND_EFX* efx;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < 2; i++) {
        efx = &Snd_efx_work[i];
        p = (u8*) efx;
        for (j = 0; j < sizeof(SND_EFX); j++) {
            *p++ = 0;
        }
        efx->work_id = i;
        efx->efx_type = 0;
    }
}

// Sets AUX bus `no` to effect `type` (0 = off, 6 = stop and clear the buffers, 1..5 = start, or
// update the parameters when the same type already runs). Returns 1 on failure (type 7 = error).
int Snd_efx_req(s16 no, s16 type)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_EFX* efx;
    int ret;

    efx = &Snd_efx_work[no];
    if (type == 0) {
        efx_req_off(efx);
        return 0;
    }
    if (type == 6) {
        efx_req_stop(efx);
        return 0;
    }
    ctrl->efx_mem = 0;
    if (efx->be_flag & 0x1) {
        if (type == efx->efx_type) {
            ret = efx_req_set_update(efx, type);
        } else {
            ret = efx_req_set_new(efx, type);
            efx_buffer_free(efx, efx->efx_type);
        }
    } else {
        ret = efx_req_set_new(efx, type);
    }
    if (ret == 1) {
        OSReport("SND EFX is not executed !! : %d\n", type);
        efx->efx_mem = ctrl->efx_mem;
        efx->be_flag = 0;
        efx->efx_type = 7;
        return 1;
    }
    efx->be_flag = 1;
    efx->efx_type = type;
    return 0;
}

// Unhooks the bus callback and frees the effect's buffers.
void efx_req_off(SND_EFX* efx)
{
    if (efx->be_flag & 0x3) {
        if (efx->work_id == 0) {
            AXRegisterAuxACallback(NULL, NULL);
        } else {
            AXRegisterAuxBCallback(NULL, NULL);
        }
        if (efx->be_flag & 0x1) {
            efx_buffer_free(efx, efx->efx_type);
        }
    }
    efx->be_flag = 0;
    efx->efx_type = 0;
}

// Replaces the running effect by the buffer-clearing callback (silence) and frees it; status 2.
void efx_req_stop(SND_EFX* efx)
{
    if (efx->be_flag & 0x1) {
        if (efx->work_id == 0) {
            AXRegisterAuxACallback((SND_AUX_CB) cb_efx_clear_bass, NULL);
        } else {
            AXRegisterAuxBCallback((SND_AUX_CB) cb_efx_clear_bass, NULL);
        }
        efx_buffer_free(efx, efx->efx_type);
        efx->be_flag = 2;
        efx->efx_type = 6;
    } else {
    }
}

// Initialises effect `type` (DPL2 reverb only in DPL2 mode) and hooks it onto the bus. Returns 1
// when the init failed.
int efx_req_set_new(SND_EFX* efx, s16 type)
{
    SND_AUX_CB cb;
    void* param;
    int ret;

    switch (type) {
    case 1:
        ret = AXFXReverbHiInit(&efx->rev_hi);
        cb = (SND_AUX_CB) AXFXReverbHiCallback;
        param = &efx->rev_hi;
        break;
    case 2:
        ret = AXFXReverbStdInit(&efx->rev_std);
        cb = (SND_AUX_CB) AXFXReverbStdCallback;
        param = &efx->rev_hi;
        break;
    case 3:
        ret = AXFXChorusInit(&efx->chorus);
        cb = (SND_AUX_CB) AXFXChorusCallback;
        param = &efx->rev_hi;
        break;
    case 4:
        ret = AXFXDelayInit(&efx->delay);
        cb = (SND_AUX_CB) AXFXDelayCallback;
        param = &efx->rev_hi;
        break;
    case 5:
        if (Snd_ctrl_work.snd_mode != 2) {
            OSReport("SOUND MODE is not DPL2\n");
            return 1;
        }
        ret = AXFXReverbHiInitDpl2(&efx->rev_dpl2);
        cb = (SND_AUX_CB) AXFXReverbHiCallbackDpl2;
        param = &efx->rev_hi;
        break;
    default:
        ret = 0;
        cb = NULL;
        param = NULL;
        break;
    }
    if (ret != 1) {
        return 1;
    } else {
        if (efx->work_id == 0) {
            AXRegisterAuxACallback(cb, param);
        } else {
            AXRegisterAuxBCallback(cb, param);
        }
        return 0;
    }
}

// Re-applies the parameters of the running effect. Returns 1 on failure.
int efx_req_set_update(SND_EFX* efx, s16 type)
{
    int ret;

    switch (type) {
    case 1:
        ret = AXFXReverbHiSettings(&efx->rev_hi);
        break;
    case 2:
        ret = AXFXReverbStdSettings(&efx->rev_std);
        break;
    case 3:
        ret = AXFXChorusSettings(&efx->chorus);
        break;
    case 4:
        ret = AXFXDelaySettings(&efx->delay);
        break;
    case 5:
        ret = AXFXReverbHiSettingsDpl2(&efx->rev_dpl2);
        break;
    default:
        ret = 0;
        break;
    }
    if (ret != 1) {
        return 1;
    } else {
        return 0;
    }
}

// Shuts the effect of `type` down (frees its delay lines).
void efx_buffer_free(SND_EFX* efx, s16 type)
{
    switch (type) {
    case 1:
        AXFXReverbHiShutdown(&efx->rev_hi);
        break;
    case 2:
        AXFXReverbStdShutdown(&efx->rev_std);
        break;
    case 3:
        AXFXChorusShutdown(&efx->chorus);
        break;
    case 4:
        AXFXDelayShutdown(&efx->delay);
        break;
    case 5:
        AXFXReverbHiShutdownDpl2(&efx->rev_dpl2);
        break;
    }
}

// AUX callback that outputs silence (used while stopping an effect).
void cb_efx_clear_bass(AXFX_BUFFERUPDATE* buf, void* context)
{
    memclr_asm(buf->left, 0x280);
    memclr_asm(buf->right, 0x280);
    memclr_asm(buf->surround, 0x280);
}

// 1 while an effect runs on bus `no`.
int Snd_efx_get_status(s16 no)
{
    SND_EFX* efx;

    efx = &Snd_efx_work[no];
    if (efx->be_flag & 0x1) {
        return 1;
    } else {
        return 0;
    }
}

// Effect type on bus `no` (0 off, 6 stopping, 7 error).
s16 Snd_efx_get_type(s16 no)
{
    return Snd_efx_work[no].efx_type;
}
