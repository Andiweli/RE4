#include "types.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"
#include "vec.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_qfps.h"
#include "db_cam.h"
#include "player.h"
#include "eprintf.h"
#include "dbmodule.h"
#include "light.h"
#include "atari.h"
#include "t_camera.h"
#include <stdio.h>
#include <string.h>

// Camera tool (t_camera REL, t_camera_data.cpp): the bridge between the tool pools (tcAdat / tcCdat /
// tcLdat) and the game's room camera data image (tcDataExport / tcDataImport, the format CamCtrl
// reads), the tool <-> game camera copies, the preview player step, the shoulder-camera (quasi-FPS)
// offset transfer and the colour-rotating drawing wrappers the editors use.

void tcGetFileName(char* path, int no, int flag)
{
    if (flag & 2) {
        if (flag & 1) {
            sprintf(path, "X:/Soft/Room/Etc/Core/core00.cam");
        } else {
            sprintf(path, "y:/Room/Etc/Core/core00.cam");
        }
    } else {
        if (flag & 1) {
            sprintf(path, "X:/Soft/Room/St%x/R%x%02x/r%x%02x%02d.cam", pG->stage_no, pG->stage_no, pG->room_no,
                    pG->stage_no, pG->room_no, no);
        } else {
            sprintf(path, "y:/Room/St%x/R%x%02x/r%x%02x%02d.cam", pG->stage_no, pG->stage_no, pG->room_no,
                    pG->stage_no, pG->room_no, no);
        }
    }
}

// Serialises the tool pools into a room camera data image (CAM_FILE_HEADER, per-area records with
// their cuts, lerps) at `buf`; returns the byte size.
int tcDataExport(u8* buf)
{
    CAM_FILE_HEADER* hdr = (CAM_FILE_HEADER*) buf;
    CUT_INFO* rec;
    CUT_INFO* r;
    // the target's `addi r31,buf,0x10` + `mr r25,r31` after the header stores and its loop-2 fovy
    // pointer in the same r31: one work pointer set at the top, copied into rec, reused as fovy
    // (a multi-set pseudo crossing the two calls -> the first callee-saved allocno, size exact)
    f32* fovy = (f32*) (buf + 0x10);
    AREA_DATA* area;
    CAMERA_DATA* cut;
    LERP_DATA* lerp;
    // one CAMERA_DATA* for loops 2 and 4 and one AREA_DATA* for loops 1 and 5: gcse's `d + 1`
    // copies then merge into one pseudo per type (r30 / r4 in the target) and buf takes r29
    CAMERA_DATA* dc;
    AREA_DATA* da;
    Vec* vp;
    Vec* pos;
    u16* fp;
    TC_AREA_DATA* a;
    TC_CAMERA_DATA* c;
    LERP_DATA* l;
    int i;
    int j;
    int num;
    int size;
    // a static array, not a pointer local: the .rodata order is B404, EMPT (parsed first), and the
    // address high is computed at the strncpy (block 2), so `mr r3,buf` issues before `addi r4`
    // (a pointer local's high sits in block 0 and dies at the call setup: sched1 weight 0 vs 1)
    static const char tag[] = "B404";

    memclr_asm(buf, 0x10);
    if (*(u16*) &pTc->cdatNum == 0) {
        strncpy((char*) buf, "EMPT", 4);
        return 4;
    }
    strncpy((char*) buf, tag, 4);
    hdr->nCdat = pTc->cdatNum;
    hdr->nAdat = pTc->adatNum;
    hdr->nLdat = pTc->ldatNum;
    rec = (CUT_INFO*) fovy;
    area = (AREA_DATA*) (rec + hdr->nAdat);
    cut = (CAMERA_DATA*) (area + hdr->nAdat);
    lerp = (LERP_DATA*) (cut + hdr->nCdat);
    vp = (Vec*) (lerp + hdr->nLdat);

    {
        da = area;
        for (i = 0; i < 0x60; i++) {
            a = &tcAdat[i];
            if (a->enable != 0xFF) {
                da->Be_flag = a->enable;
                da->No = a->area_no;
                da->Suffix = a->cam_no;
                da->Attr = tcTypeTbl[a->area_no][0];
                da->Dir = a->dir;
                da->Type_char = a->attr2;
                da->Type_addr = a->attr3;
                da->Height = a->height;
                da->Y = a->base_y;
                num = a->num;
                da->pVer = (Vec*) ((u8*) vp - buf);
                da->nVer = num;
                for (j = 0; j < a->num; j++) {
                    *vp++ = a->pt[j];
                }
                da++;
            }
        }
    }
    pos = (Vec*) vp;
    {
        dc = cut;
        for (i = 0; i < 0x40; i++) {
            TC_CAMERA_DATA* cd = &tcCdat[i];
            if (cd->enable != 0xFF) {
                Vec* pp;
                Vec* at;
                f32* roll;
                dc->Be_flag = cd->enable;
                dc->No = cd->cam_no;
                dc->Id = cd->type;
                dc->nPoint = cd->num;
                dc->Attr = cd->flags;
                dc->offset = cd->aim_ofs;
                switch (cd->type) {
                case 4:
                    *(Vec*) &dc->floor_ratio = cd->u44.dir;
                    break;
                case 8:
                    dc->floor_ratio = cd->u44.floor;
                    break;
                }
                pp = pos;
                at = pp + cd->num;
                roll = (f32*) (at + cd->num);
                fovy = roll + cd->num;
                dc->pCampos = (Vec*) ((u8*) pp - buf);
                dc->pTarget = (Vec*) ((u8*) at - buf);
                dc->pRoll = (f32*) ((u8*) roll - buf);
                dc->pFovy = (f32*) ((u8*) fovy - buf);
                for (j = 0; j < cd->num; j++) {
                    *pp++ = cd->pos[j];
                    *at++ = cd->at[j];
                    *roll++ = cd->roll[j];
                    *fovy++ = cd->fovy[j];
                }
                pos = (Vec*) fovy;
                dc++;
            }
        }
    }
    {
        LERP_DATA* d = lerp;
        for (i = 0; i < 0x40; i++) {
            l = &tcLdat[i];
            if (l->Be_flag != 0xFF) {
                *d = *(LERP_DATA*) l;
                d++;
            }
        }
    }
    fp = (u16*) pos;
    {
        dc = cut;
        for (i = 0; i < pTc->cdatNum; i++) {
            c = &tcCdat[i];
            if (c->enable != 0xFF) {
                if (c->type == 6 || c->type == 7) {
                    dc->pFrame = (u16*) ((u8*) fp - buf);
                    for (j = 0; j < c->num; j++) {
                        *fp++ = c->frame[j];
                    }
                }
                dc++;
            }
        }
    }
    {
        r = rec;  // r, da, size in this order: the preheader `mr r10,r25; mr r7,r28; subf r26` is LUID order
        da = area;
        size = (u8*) fp - buf;
        // `i++` in the header (the PRE'd `i + 1` shortens i's live range: i is allocated before
        // found), the cut walker is the shared `dc` (r8 in loops 2/4/5: one pseudo), and the area
        // offset is a raw word store: an INDIRECT_REF store is not MEM_IN_STRUCT_P, so the `pTc`
        // reload depends on it and issues after `extsb no` / `addi i` (no's load temp then takes r9)
        for (i = 0; i < pTc->adatNum; i++) {
            s8 no = da->No;
            dc = cut;
            int found = 0;
            *(u32*) &r->pAdat = (u8*) da - buf;
            for (j = 0; j < pTc->cdatNum; j++, dc++) {
                if (no == dc->No) {
                    r->pCdat = (CAMERA_DATA*) ((u8*) dc - buf);
                    found = 1;
                    r->Attr = tcTypeTbl[no][0];
                    break;
                }
            }
            if (found == 0) {
                r->pCdat = (CAMERA_DATA*) found;
                da->Be_flag = found;
            }
            r++;
            da++;
        }
    }
    return size;
}

// Expands a room camera data image (any known version) into the tool pools; -1 on a bad header.
int tcDataImport(u8* buf)
{
    CAM_FILE_HEADER* hdr = (CAM_FILE_HEADER*) buf;
    CUT_INFO* rec;
    AREA_DATA* area;
    CAMERA_DATA* cut;
    LERP_DATA* lerp;
    u8 cnt[64];
    int ver;
    int lo;
    int i;
    int j;

    ver = cameraDataVersion((char*) buf);
    if (ver == -1) {
        pTc->adatNum = 0;
        return -1;
    }
    if (ver < -1) {
        pTc->adatNum = 0;
        return -1;
    }
    if (ver > 4) {
        pTc->adatNum = 0;
        return -1;
    }
    // `2` through a local: the tree folder would turn `ver < 2` into `ver <= 1`, but the target
    // shares one `cmpwi 2` (cr0 kept in r25) with the `ver <= 2` test inside the area loop.
    lo = 2;
    if (ver < lo) {
        pTc->adatNum = 0;
        return -1;
    }
    pTc->adatNum = 0;
    pTc->cdatNum = 0;
    pTc->ldatNum = 0;
    for (i = 0; i < 64; i++) cnt[i] = 0;
    rec = (CUT_INFO*) (buf + 0x10);

    area = (AREA_DATA*) (rec + hdr->nAdat);
    cut = (CAMERA_DATA*) (area + hdr->nAdat);
    lerp = (LERP_DATA*) (cut + hdr->nCdat);

    {
        CUT_INFO* r = rec;
        for (i = 0; i < hdr->nAdat; i++) {
            if (r->pCdat) {
                tcTypeTbl[r->pCdat->No][0] = r->Attr;
            } else {
                tcTypeTbl[r->pAdat->No][0] = r->Attr;
            }
            r++;
        }
    }
    {
        AREA_DATA* s = area;
        for (i = 0; i < hdr->nAdat; i++) {
            TC_AREA_DATA* a = tcAdatNew();
            a->area_no = s->No;
            a->cam_no = s->Suffix;
            pTc->adatTypeNum[s->No]++;
            a->height = s->Height;
            a->base_y = s->Y;
            a->num = s->nVer;
            if (ver <= 2) {
                a->attr = 3;
                tcTypeTbl[s->No][0] = 3;
            } else {
                a->attr = s->Attr;
            }
            a->dir = s->Dir;
            if (ver <= 3) {
                a->attr2 = 1;
                a->attr3 = 0xFF;
            } else {
                a->attr2 = s->Type_char;
                a->attr3 = s->Type_addr;
            }
            {
                Vec* pt = s->pVer;
                for (j = 0; j < s->nVer; j++) {
                    a->pt[j] = *pt++;
                }
            }
            s++;
        }
    }
    {
        CAMERA_DATA* s = cut;
        for (i = 0; i < hdr->nCdat; i++) {
            TC_CAMERA_DATA* c = tcCdatNew();
            c->cam_no = s->No;
            cnt[s->No]++;
            if (cnt[s->No] != 1) {
                pLog->err(0, 0, "Camera[%02d] is duplicate.", s->No);
            }
            c->type = s->Id;
            c->num = s->nPoint;
            c->aim_ofs = s->offset;
            c->flags = s->Attr;
            switch (s->Id) {
            case 4:
                c->u44.dir = *(Vec*) &s->floor_ratio;
                break;
            case 8:
                c->u44.floor = s->floor_ratio;
                break;
            }
            {
                Vec* pos = s->pCampos;
                Vec* at = s->pTarget;
                f32* roll = s->pRoll;
                f32* fovy = s->pFovy;
                u16* frames = s->pFrame;
                for (j = 0; j < s->nPoint; j++) {
                    c->pos[j] = *pos++;
                    c->at[j] = *at++;
                    c->roll[j] = *roll++;
                    c->fovy[j] = *fovy++;
                    if (s->Id == 6 || s->Id == 7) {
                        c->frame[j] = *frames++;
                    }
                }
            }
            s++;
        }
    }
    {
        LERP_DATA* s = lerp;
        for (i = 0; i < hdr->nLdat; i++) {
            LERP_DATA* l = tcLdatNew();
            *(LERP_DATA*) l = *s;
            l->Attr = 0;
            s++;
        }
    }
    if (hdr->nAdat != 0) {
        pTc->cdatNo = rec->pAdat->No;
        pTc->adatNo = pTc->cdatNo;
        pTc->adatSuffix = 0;
        pTc->pAdat = tcAdatPtr(pTc->adatNo, pTc->adatSuffix);
    }
    return 0;
}

// Floor height used by the shoulder camera preview (fixed 100).
f32 tcGetFloor(Vec*)
{
    return 100.0f;
}

struct TcPreviewWork {
    int blink;
    int x4;
};
static TcPreviewWork tcPreview = {8, 0};

// Preview: blinking [ PREVIEW ] with the current camera / area numbers, then the player moves.
void tcPlayerMove()
{
    if (tcPreview.blink <= 15) {
        eprintf(0xD8, 0xFC, 5, 0, "[ PREVIEW ]");
    }
    tcPreview.blink++;
    if (tcPreview.blink > 31) tcPreview.blink = 0;
    eprintf(0xD8, 0x10A, 0, 0, "C:%02d", pTc->cameraNo);
    eprintf(0x100, 0x10A, 0, 0, "A:%02d-%1d", pTc->areaNo, pTc->areaSuffix);
    pPL->move();
}

// Runs the debug camera (pad 2) on the tool camera.
void tcCameraDebugMove()
{
    tcToolCamera2GameCamera();
    CamDbg.move(&pG->Camera, &Joy[1], 0);
    tcGameCamera2ToolCamera();
}

// Copies pG->Camera into the tool camera.
void tcGameCamera2ToolCamera()
{
    pTc->cam = pG->Camera;
}

// Copies the tool camera into pG->Camera.
void tcToolCamera2GameCamera()
{
    pG->Camera = pTc->cam;
}

CAMERA tcGameCamera;

// Saves the game camera (tool entry).
void tcGameCameraStore()
{
    tcGameCamera = pG->Camera;
}

// Restores the saved game camera (tool exit).
void tcGameCameraLoad()
{
    pG->Camera = tcGameCamera;
}

// 3D line in RGBA colour (rotated into the ARGB word Draw_line3d takes).
void tcDrawLine3D(Vec* a, Vec* b, u32 color)
{
    Draw_line3d(a, b, (color >> 8) | (color << 24), 0);
}

// Wire sphere of radius r.
void tcDrawSphere(Vec* pos, u32 color, f32 r)
{
    Draw_sphere(pos, r, color, 1, 1);
}

// Filled quad in RGBA colour.
void tcDrawPoly(Vec* p, u32 color)
{
    Draw_poly(p, (color >> 8) | (color << 24), 1);
}

// Shoulder camera floor ratio into CamCtrl's quasi-FPS controller.
void tcSetBesideFloor(f32 ratio)
{
    tcCdatPtr(pTc->cdatNo)->u44.floor = ratio;
}

// Copies the shoulder camera ready / transition offset tables into the current cut's key data.
void tcSetBesideOffset(QFPS_OFFSET (*ready)[3], QFPS_OFFSET (*trans)[3])
{
    TC_CAMERA_DATA* c = tcCdatPtr(pTc->cdatNo);
    int n = 0;
    int i;
    int j;
    // one `o` for both loops: the shared pseudo is live across loop 1's r9/r10/r11 temporaries,
    // so global alloc gives it r8 in loop 2 as well (a loop-local `o` takes r11 there)
    QFPS_OFFSET* o;

    c->num = 24;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
            c->pos[n] = o->m_campos[0];
            c->at[n] = o->m_target;
            c->roll[n] = o->m_roll;
            c->fovy[n] = o->m_fovy;
            n++;
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
            c->pos[n++] = o->m_campos[1];
        }
    }
}

// Applies the current cut's shoulder camera data (offset tables, floor ratio) to CamCtrl's
// quasi-FPS controller for the preview.
void tcSetBesideCamera()
{
    QFPS_OFFSET ready[2][3];
    QFPS_OFFSET trans[2][3];
    TC_CAMERA_DATA* c = tcCdatPtr(pTc->cdatNo);
    int i;
    int j;
    int n = 0;

    CamCtrl.m_QuasiFPS.setAreaData(g_readyOfs[0], g_transOfs[0]);
    CamCtrl.m_QuasiFPS.getAreaData(ready, trans);
    if (c->num == 24) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++, n++) {
                QFPS_OFFSET* o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
                if (c->flags & 0x20) {
                    if (i > 1) continue;
                } else if (!(c->flags & 0x10)) {
                    if (i <= 1) continue;
                }
                o->m_campos[0] = c->pos[n];
                o->m_target = c->at[n];
                o->m_roll = c->roll[n];
                o->m_fovy = c->fovy[n];
            }
        }
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++, n++) {
                QFPS_OFFSET* o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
                if (c->flags & 0x20) {
                    if (i > 1) continue;
                } else if (!(c->flags & 0x10)) {
                    if (i <= 1) continue;
                }
                o->m_campos[1] = c->pos[n];
                // dead test (o is re-set at the body top): the extra ref/live range lets `o` beat
                // the `&c->pos[n]` giv in global alloc (r8/r7 as the original); deleted at flow2
                if (c == 0) o = 0; // COMPILER-DIFF: #13 (global-alloc order, dead test)
            }
        }
    } else {
        c->num = n;
    }
    CamCtrl.m_QuasiFPS.setAreaData(ready, trans);
    CamCtrl.m_QuasiFPS.setFloorRatio(c->u44.floor);
}

// the split object ends .rodata with a 4-byte pad to 8 (the linker does not re-create it)
asm(".section .rodata; .balign 8; .text");
