// game/cam_ctrl.cpp: the camera controller (CamCtrl), which picks a cut from the room's B40x camera
// data when the player enters an area. CameraMove (camera.cpp) copies its result into pG->Camera.

#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "cam_motion.h"
#include "db_log.h"
#include "atari.h"
#include "light.h"
#include "model.h"
#include "player.h"
#include "em.h"
#include "main_mem.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "at_mod.h"
#include "joy.h"
#include <string.h>
#include <dolphin/os.h>
#include "pl_npc.h"
#include "esp.h"
#include "quake.h"
#include "eprintf.h"
#include "view.h"
#include "t_camera.h"



void* g_pToolCamData = NULL;

#define CAMERA_MOTION_BUFFER_SIZE 0x440
static u8 CameraMotionBuffer[CAMERA_MOTION_BUFFER_SIZE];

// internal linkage: the table is deferred behind the cManager template strings in .rodata
static const f32 smooth_ratio[12] = {0.0f, 0.9f, 0.85f, 0.92f, 0.8f, 0.92f, 0.9f, 0.9f, 0.9f, 0.9f, 0.0f, 0.0f};

// Byte-wise copy of the float `tmp` into the (unaligned) motion buffer. `&tmp` indexed directly
// (a pointer local is copy-propagated into loops 2/3), `n` is the function-scope counter shared
// by the three copies (one allocno -> r11 in all three, the `&tmp` copies fall to r12/r9/r9).
#define EXPORT_TMP(p)                             \
    {                                             \
        for (n = 0; n < 4; n++) {                 \
            *(p)++ = ((u8*) &tmp)[n];             \
        }                                         \
    }

// Converts a rail cut into the CameraMotion key-frame format (cam_motion): header, 4 channels
// (pos, at, roll, fovy) x 3 components of hermite keys {value, tangent in, tangent out}.
int CameraControl::HermiteExport(CAMERA_DATA* pCdat, u8* p)
{
    u8* buf = p;  // the parameter is the running pointer (r5: `sth 0(r5); stbu 2(r5); addi r5,1`), buf the saved base
    u32* table;
    u16* frames;
    int i;
    int j;
    int k;
    int k0;
    int k1;
    f32 tmp;
    f32 v;
    f32 tan;
    f32 v0;
    f32 v1;
    f32 dt0;
    f32 dt1;
    int n;

    *(u16*) p = (pCdat->nPoint - 1) * 30;
    p += 2;
    *p++ = 4;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
        case 1:
            *(u16*) p = 4;
            break;
        case 2:
        case 3:
            *(u16*) p = 2;
            break;
        }
        p += 2;
    }
    for (i = 0; i < 4; i++) {
        *p++ = i;
    }
    *p++ = 0;
    *(u32*) p = 0;
    p += 4;
    table = (u32*) p;
    for (i = 0; i < 4; i++) {
        *(u32*) p = 0;
        p += 4;
    }
    for (i = 0; i < 4; i++) {
        table[i] = p - buf;
        for (j = 0; j < 3; j++) {
            *(u16*) p = pCdat->nPoint;
            p += 2;
            v = 0.0f;
            v0 = 0.0f;
            v1 = 0.0f;
            frames = (u16*) p;
            for (k = 0; k < pCdat->nPoint; k++) {
                if (pCdat->pFrame == NULL) {
                    *(u16*) p = k * 30;
                } else {
                    *(u16*) p = pCdat->pFrame[k];
                }
                p += 2;
            }
            for (k = 0; k < pCdat->nPoint; k++) {
                k1 = k + 1;
                k0 = k - 1;
                if (k1 > pCdat->nPoint - 1) {
                    k1 = pCdat->nPoint - 1;
                }
                if (k0 < 0) {
                    k0 = 0;
                }
                switch (i) {
                case 0:
                    v = (&pCdat->pCampos[k].x)[j];
                    v0 = (&pCdat->pCampos[k0].x)[j];
                    v1 = (&pCdat->pCampos[k1].x)[j];
                    break;
                case 1:
                    v = (&pCdat->pTarget[k].x)[j];
                    v0 = (&pCdat->pTarget[k0].x)[j];
                    v1 = (&pCdat->pTarget[k1].x)[j];
                    break;
                case 2:
                    v = pCdat->pRoll[k];
                    v0 = pCdat->pRoll[k0];
                    v1 = pCdat->pRoll[k1];
                    break;
                case 3:
                    v = pCdat->pFovy[k];
                    v0 = pCdat->pFovy[k0];
                    v1 = pCdat->pFovy[k1];
                    v1 *= DEG2RAD;
                    v *= DEG2RAD;
                    v0 *= DEG2RAD;
                    break;
                }
                tmp = v;
                EXPORT_TMP(p);
                dt0 = (f32) (frames[k] - frames[k0]);
                dt1 = (f32) (frames[k1] - frames[k]);
                if (k == 0) {
                    tan = (v1 - v) / dt1;
                } else if (pCdat->nPoint - 1 == k) {
                    tan = (v - v0) / dt0;
                } else {
                    tan = (dt1 * ((v - v0) / dt0) + dt0 * ((v1 - v) / dt1)) / (dt0 + dt1);
                }
                tan *= dt0;
                tmp = tan;
                EXPORT_TMP(p);
                EXPORT_TMP(p);
            }
        }
        {
            // `rem` is one multi-set variable (in place in the `p - buf` register) and the pad loop
            // counts on `j` (a GPR elsewhere, so the reversed count stays `addic./bne`, no ctr).
            int rem = p - buf;
            rem %= 4;
            if (rem) {
                rem = 4 - rem;
                for (j = 0; j < rem; j++) {
                    *p++ = 0;
                }
            }
        }
    }
    *(u16*) buf = frames[pCdat->nPoint - 1];
    return p - buf;
}

// 1 during the frame the camera cut changed (m_state_flag bit1); CamStick2World and the
// visibility tests use it.
int CameraControl::IsChangeCamera()
{
    if (m_state_flag & 2) {
        return 1;
    }
    return 0;
}

// Returns control to the area cameras after a forced cut / event camera: clears the "cut held"
// flag, re-enables the area check and drops the motion-camera flag.
void CameraControl::Comeback(int)
{
    pCamData = (CAM_FILE_HEADER*) pG->pCamRoom;
    m_state_flag &= ~4;
    m_system_flag = (m_system_flag & ~8) | 0x10;
    if (m_system_flag & 0x20) {
        m_system_flag &= ~0x20;
    }
    Check();
}

// Stops the controller (r0 Wait, area check off); the room / event drives pG->Camera itself.
void CameraControl::Disable()
{
    r0 = 0;
    m_system_flag |= 8;
}

// mode 0 disables the per-frame area check (the current camera stays), else re-enables it.
void CameraControl::AreaCheckOnOff(int sw)
{
    switch (sw) {
    case 0:
        m_system_flag |= 8;
        break;
    case 1:
        m_system_flag = (m_system_flag & ~8) | 0x10;
        break;
    }
}

// Number of camera areas in the room data.
u8 CameraControl::AreaNum()
{
    return pCamData->nAdat;
}

// Area number of the active camera (-1 none).
int CameraControl::CurrentAreaNo()
{
    return areaNo;
}

// Camera number of the active cut (-1 none).
int CameraControl::CurrentCameraNo()
{
    return cameraNo;
}

// The cut record with camera_no `no` (the last record when not found).
CAMERA_DATA* CameraControl::DataSearch(int cameraNo)
{
    CUT_INFO* rec = (CUT_INFO*) (pCamData + 1);
    AREA_DATA* area = (AREA_DATA*) (rec + pCamData->nAdat);
    CAMERA_DATA* cut = (CAMERA_DATA*) (area + pCamData->nAdat);
    int i = 0;

    while (i < pCamData->nCdat && cameraNo != cut->No) {
        i++;
        cut++;
    }
    return cut;
}

// The interpolation record for the transition from one area / camera to another; NULL when the
// data has none (a hard cut).
LERP_DATA* CameraControl::LerpDataSearch(int srcNo, int srcSuf, int dstNo, int dstSuf)
{
    CUT_INFO* rec = (CUT_INFO*) (pCamData + 1);
    AREA_DATA* area = (AREA_DATA*) (rec + pCamData->nAdat);
    CAMERA_DATA* cut = (CAMERA_DATA*) (area + pCamData->nAdat);
    LERP_DATA* lerp = (LERP_DATA*) (cut + pCamData->nCdat);
    int i;

    for (i = 0; i < pCamData->nLdat; i++, lerp++) {
        if (srcNo == lerp->SrcNo && srcSuf == lerp->SrcSuffix && dstNo == lerp->DstNo &&
            dstSuf == lerp->DstSuffix) {
            return lerp;
        }
    }
    return NULL;
}

// Relocates a camera data file in place ("B400".."B404": file offsets -> pointers for the area
// polygons and the cut key arrays); older versions get their attr 8 promoted to 0x20. Returns
// the buffer, or unchanged when already relocated / unknown.
CAM_FILE_HEADER* CameraControl::calcAddr(CAM_FILE_HEADER* pBuff)
{
    int ver2;
    int i;
    CUT_INFO* rec;
    AREA_DATA* area;
    CAMERA_DATA* cut;

    if (cameraDataVersion((char*) pBuff) <= 1) {
        return pBuff;
    }
    ver2 = 0;  // assigned after the early return: its `li` lands after the strncmp call
    if (strncmp((char*) pBuff, "B402", 4) == 0) {
        ver2 = 1;
        OSReport("CameraControl::calcAddr(): R%1d%02x Ver02", pG->stage_no, pG->room_no);
    }

    rec = (CUT_INFO*) (pBuff + 1);
    for (i = 0; i < pBuff->nAdat; i++, rec++) {
        if ((s32) rec->pAdat < 0) {
            return pBuff;
        }
        rec->pAdat = (AREA_DATA*) ((u32) rec->pAdat + (u32) pBuff);
        if (rec->pCdat) {
            rec->pCdat = (CAMERA_DATA*) ((u32) rec->pCdat + (u32) pBuff);
        }
    }

    area = (AREA_DATA*) rec;
    for (i = 0; i < pBuff->nAdat; i++, area++) {
        area->pVer = (Vec*) ((u32) area->pVer + (u32) pBuff);
        if (ver2) {
            area->Attr = 3;
        }
        if (area->Attr & 8) {
            area->Attr |= 0x20;
        }
        if (cameraDataVersion((char*) pBuff) <= 3) {
            area->Type_char = 1;
            area->Type_addr = 0xFF;
            OSReport("CameraControl::calcAddr(): R%1d%02x Ver%02d", pG->stage_no, pG->room_no,
                     cameraDataVersion((char*) pBuff));
        }
    }

    cut = (CAMERA_DATA*) area;
    for (i = 0; i < pBuff->nCdat; i++, cut++) {
        cut->pCampos = (Vec*) ((u32) cut->pCampos + (u32) pBuff);
        cut->pTarget = (Vec*) ((u32) cut->pTarget + (u32) pBuff);
        cut->pRoll = (f32*) ((u32) cut->pRoll + (u32) pBuff);
        cut->pFovy = (f32*) ((u32) cut->pFovy + (u32) pBuff);
        cut->pFrame = (u16*) ((u32) cut->pFrame + (u32) pBuff);
    }
    return pBuff;
}

// Installs the room's camera data (relocated).
void CameraControl::RoomDataRead(CAM_FILE_HEADER* pBuff)
{
    pG->pCamRoom = calcAddr(pBuff);
    pCamData = (CAM_FILE_HEADER*) pG->pCamRoom;
}

// Installs the core (shared) camera data.
void CameraControl::CoreDataRead(CAM_FILE_HEADER* pBuff)
{
    pG->pCamCore = calcAddr(pBuff);
}

// Line of sight test for cameras: from -> to against characters, objects and the scenery (walls
// only, camera-ignored attributes masked); the nearest hit in *pos / *nrm. 1 when blocked.
int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to)
{
    static f32 R_GAIN = 1.1f;
    static f32 GAIN = 1.33f;
    Vec posA;
    Vec posB;
    Vec posC;
    Vec nrmA;
    Vec nrmB;
    Vec nrmC;
    Vec p;
    Vec hp;
    Vec hn;
    int hitA;
    int hitB;
    int hitC;
    int ret = 0;
    int first;
    f32 dist;
    f32 d;

    hitA = EmHitCheck(&posA, &nrmA, from, to, 1);
    hitB = ObjHitCheck(&posB, &nrmB, from, to, 1);
    hitC = SatMgr.hitCheck(from, to, &posC, &nrmC, 0x8000, 0x1C2810);
    if (hitA | hitB | hitC) {
        // COMPILER-DIFF: codeless anchor. The empty loop leaves NOTE_INSN_LOOP_BEG/END here, which ends
        // the first cse pass's extended basic block at this point (cse1 stops at LOOP_END). Without it
        // cse1 folds the `&posB`/`&nrmB` recomputations into the earlier `&posA` pseudos and the target's
        // `mr r18,r28` (gcse PRE copy) and fresh `addi r6/r7` argument forms are not produced.
        do { } while (0);
        dist = 0.0f;
        first = 1;
        if (hitA) {
            dist = PSVECDistance(from, &posA);
            first = 0;
            *pos = posA;
            *nrm = nrmA;
        }
        if (hitB) {
            d = PSVECDistance(from, &posB);
            if (first || d < dist) {
                dist = d;
                first = 0;
                *pos = posB;
                *nrm = nrmB;
            }
        }
        if (hitC) {
            d = PSVECDistance(from, &posC);
            if (first || d < dist) {
                *pos = posC;
                *nrm = nrmC;
            }
        }
        ret = 1;
    }
    if (pSUB && pSUB->id == 3) {
        cAtariInfo atBuf;
        // The target reads/writes the info through a pointer register (lha 0x18(r29), stfs 0x4(r29)) that is
        // a copy of the constructor's `this` register (`mr r29,r30`), and the 76-byte copy below increments
        // that `this` register in place. A plain `cAtariInfo& at = atBuf;` cannot produce this: cse makes the
        // longer-lived reference the canonical register (the copy loop then runs on a copy of it), and gcse
        // copy propagation replaces the reference by the `this` temporary everywhere else.
        // COMPILER-DIFF: register pin (r29) plus launder. The pin keeps `at` out of cse's canonical class
        // (hard regs go last), so the copy loop's address is the `this` temporary; the launder gives `at` a
        // second set so the `at = this` copy is not propagated into the later field accesses.
        register cAtariInfo* at asm("r29") = &atBuf;
        asm("" : "+r"(at));
        cCoord* parts;
        Vec w;

        atBuf = pSUB->atari;
        if (at->m_parts_no != 0) {
            parts = pSUB->getPartsPtr(at->m_parts_no - 1);
        } else {
            parts = pSUB;
        }
        if (parts) {
            f32 r;
            int hit;

            at->m_offset.y -= 1000.0f;
            at->m_height += 1000.0f;
            PSMTXMultVec(parts->mat, &at->m_offset, &w);
            r = at->m_radius * R_GAIN;
            if (ret) {
                p = *pos;
            } else {
                p = *to;
            }
            // `hit = 0` after the `p` copy: the `li` sits in the join block and the w.y/p.y compare
            // registers come out as f12/f13 (declaring it initialised moves both).
            hit = 0;
            if (w.y <= p.y) {
                if (p.y <= w.y + at->m_height) {
                    Vec a;
                    Vec b;

                    a = w;
                    b = p;
                    a.y = 0.0f;
                    b.y = 0.0f;
                    if (PSVECDistance(&b, &a) <= r) {
                        hit = 1;
                    }
                }
            }
            if (hit == 1) {
                cEm* sub = pSUB;
                at->m_radius *= GAIN;
                if (ObaLineHitChk(sub, at, *from, p, hp, hn)) {
                    ret = 1;
                    *pos = hp;
                }
            }
        }
    }
    return ret;
}

// Copies the cut's first key (pos / at / roll / fov) into a Camera and rebuilds its orientation.
void CameraSetCutData(CAMERA* pCam, CAMERA_DATA* pData)
{
    pCam->param.Campos = *pData->pCampos;
    pCam->param.Target = *pData->pTarget;
    pCam->param.Roll = *pData->pRoll;
    pCam->param.Fovy = *pData->pFovy;
    CameraSetOrientationRoll(pCam);
}

// Script: enables / disables the camera area (area_no, camera_no) for the area check.
void CameraControl::AreaOnOff(int No, int Suffix, int OnOff)
{
    CAM_FILE_HEADER* d = pCamData;
    CUT_INFO* rec = (CUT_INFO*) (d + 1);
    s8 i;

    for (i = 0; i < d->nAdat; i++, rec++) {
        if (No == rec->pAdat->No && Suffix == rec->pAdat->Suffix) {
            rec->pAdat->Be_flag = OnOff;
            break;
        }
    }
}

// Script: ORs `attr` bits into the area's attribute (0x20 normal, 1 / 2 calm / battle...).
void CameraControl::SetAreaAttr(int No, int Suffix, u8 attr)
{
    CAM_FILE_HEADER* d = pCamData;
    CUT_INFO* rec = (CUT_INFO*) (d + 1);
    s8 i;

    for (i = 0; i < d->nAdat; i++, rec++) {
        if (No == rec->pAdat->No && Suffix == rec->pAdat->Suffix) {
            rec->pAdat->Attr |= attr;
            break;
        }
    }
}

// Script: clears `attr` bits of the area's attribute.
void CameraControl::UnsetAreaAttr(int No, int Suffix, u8 attr)
{
    CAM_FILE_HEADER* d = pCamData;
    CUT_INFO* rec = (CUT_INFO*) (d + 1);
    s8 i;

    for (i = 0; i < d->nAdat; i++, rec++) {
        if (No == rec->pAdat->No && Suffix == rec->pAdat->Suffix) {
            rec->pAdat->Attr &= ~attr;
            break;
        }
    }
}

// Script: forces camera cut `no` (its first area record) and holds it (m_state_flag bit2) until
// Comeback.
void CameraControl::CutCall(int cutNo)
{
    CAM_FILE_HEADER* d = pCamData;
    CUT_INFO* rec = (CUT_INFO*) (d + 1);
    int found = 0;
    s8 i;

    for (i = 0; i < d->nAdat; i++, rec++) {
        if (cutNo == rec->pCdat->No) {
            found = 1;
            break;
        }
    }
    if (found) {
        clearAttachCamera();
        m_Inter.frame = 0;
        switchCamera(rec);
        m_system_flag |= 8;
        m_state_flag |= 4;
    } else {
        pLog->err(0, 0, "CameraControl::CutCall(): Cut %02d doesn't exist.", cutNo);
    }
}

// Activates the area record: sets up the lerp from the current camera and picks the routine from
// the cut type.
void CameraControl::switchCamera(CUT_INFO* rec)
{
    AREA_DATA* area = rec->pAdat;
    CAMERA_DATA* cut = rec->pCdat;
    LERP_DATA* lerp = NULL;
    CAM_FILE_HEADER* d;
    int i;
    int size;

    if (areaNo != -1) {
        lerp = LerpDataSearch(areaNo, areaSuffix, area->No, area->Suffix);
        if (lerp && lerp->Be_flag == 1) {
            m_Inter.set(lerp->InterFrame, &cur);
        }
    } else {
        m_Inter.frame = 0;
    }

    if (m_system_flag & 2) {
        if (!(rec->pAdat->Attr & 8)) {
            CUT_INFO* r;
            d = pCamData;
            for (r = (CUT_INFO*) (d + 1), i = 0; i < d->nAdat; r++, i++) {
                if (r->pAdat->Attr & 8) {
                    r->pAdat->Be_flag = 0;
                }
            }
        }
        m_system_flag &= ~2;
    }

    if (area_rec != NULL) {
        AREA_DATA* a = area_rec->pAdat;
        if (a->Attr & 0x10) {
            a->Be_flag = 0;
        } else if (a->Attr & 8) {
            CUT_INFO* r;
            d = pCamData;
            for (r = (CUT_INFO*) (d + 1), i = 0; i < d->nAdat; r++, i++) {
                if (r->pAdat->Attr & 8) {
                    r->pAdat->Be_flag = 0;
                }
            }
        }
    }

    areaNo = area->No;
    areaSuffix = area->Suffix;
    cameraNo = cut->No;
    area_rec = rec;

    if (areaNo != -1) {
        if (m_system_flag & 0x10) {
            if (!(m_system_flag & 0x40)) {
                LightMgr.update(areaNo, -1);
            }
        } else if (!(area->Attr & 0x80)) {
            LightMgr.update(areaNo, -1);
        }
    }
    m_state_flag |= 2;

    switch (cut->Id) {
    case 0:
        r1 = 0;
        r0 = 1;
        break;
    case 1:
        r0 = 2;
        r1 = 0;
        break;
    case 2:
        r0 = 3;
        r1 = 0;
        break;
    case 3:
        r0 = 4;
        r1 = 0;
        break;
    case 4:
        r0 = 6;
        r1 = 0;
        break;
    case 5:
        r0 = 7;
        r1 = 0;
        break;
    case 6:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (m_pProc) {
            delete m_pProc;
        }
        m_pProc = new (m_Free) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        ((CameraMotion*) m_pProc)->setBaseMatPtr(NULL);
        r0 = 5;
        break;
    case 7:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (m_pProc) {
            delete m_pProc;
        }
        m_pProc = new (m_Free) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        r0 = 9;
        break;
    case 8: {
        // two pointers to qfps: `q` (readyArrayPtr, setAreaData, bindAreaCamera) keeps the addi; `p`
        // (transArrayPtr, setBlendData) and the direct `qfps.` calls share gcse's copy (mr r29,r30)
        CameraQuasiFPS* q = &m_QuasiFPS;
        CameraQuasiFPS* p = &m_QuasiFPS;
        if (q->readyArrayPtr() && p->transArrayPtr()) {
            p->setBlendData(q->readyArrayPtr(), p->transArrayPtr());
        }
        q->setAreaData(area_rec->pCdat);
        q->bindAreaCamera(area_rec);
        if (r0_old == 10 && !(m_system_flag & 0x10)) {
            m_QuasiFPS.setBlendCount(10);
        } else {
            m_QuasiFPS.init();
            r1 = 0;
        }
        r0 = 10;
        break;
    }
    }
    m_system_flag &= ~0x10;
}

// 1 when the area is enabled and matches both attribute masks.
int areaAttr(AREA_DATA* p_area, u8 cut_attr, u8 char_type)
{
    if ((p_area->Be_flag & 1) && (p_area->Attr & cut_attr) && (p_area->Type_char & char_type)) {
        return 1;
    }
    return 0;
}

// 1 when `pos` is inside the area polygon (and, with attr 0x40, the facing `dir` is within 135
// degrees of the area's direction).
int areaHit(Vec* pPos, AREA_DATA* pArea, f32 dir_y)
{
    int ret;

    if (pArea->Attr & 4) {
        return 0;
    }
    if (pArea->Attr & 0x40) {
        f32 d = pArea->Dir;
        while (dir_y >= PI) {
            dir_y -= PI2;
        }
        while (dir_y < -PI) {
            dir_y += PI2;
        }
        dir_y -= d;
        while (dir_y >= PI) {
            dir_y -= PI2;
        }
        while (dir_y < -PI) {
            dir_y += PI2;
        }
        if (dir_y > 2.3561945f || dir_y < -2.3561945f) {
            return 0;
        }
    }
    if (pArea->nVer > 4) {
        ret = area_hit_pN(pPos, pArea);
    } else {
        ret = area_hit_p3(pPos, pArea);
    }
    return ret;
}

// Point in a convex area polygon of up to 4 points (height band base_y .. base_y + height).
int area_hit_p3(Vec* pPos, AREA_DATA* pArea)
{
    Vec* p[3];  // the three corner pointers live in memory (stw/lwz around the calls)
    Vec v1, v2, v0, c0, c1;
    f32 y = pPos->y + 100.0f;
    int i, n, n1, i0;

    if (y < pArea->Y) {
        return 0;
    }
    if (y >= pArea->Y + pArea->Height) {
        return 0;
    }
    for (i = 0; i <= 1; i++) {
        n = pArea->nVer;
        n1 = n - 1;   // its own statement: `(i0 + n - 1)` is reassociated by fold into `(i0 - 1) + n`
        i0 = i + i + 1;
        i0 %= n;      // two sets of i0: loop.c does not strength-reduce the 2i+1 giv
        p[0] = &pArea->pVer[i0];
        p[1] = &pArea->pVer[(i0 + n1) % n];
        p[2] = &pArea->pVer[(i0 + 1) % n];
        PSVECSubtract(pPos, p[0], &v0);
        PSVECSubtract(p[1], p[0], &v1);
        PSVECSubtract(p[2], p[0], &v2);
        PSVECCrossProduct(&v1, &v0, &c0);
        PSVECCrossProduct(&v2, &v0, &c1);
        // one `||` return: the shared `li r3,0` block starts with a label, so loop.c's exit-block move
        // leaves it inside the loop and jump2 folds the two entry returns into it
        if (c0.y > 0.0f || c1.y < 0.0f) {
            return 0;
        }
    }
    return 1;
}

// Point in an area polygon of more than 4 points (fan of triangles).
int area_hit_pN(Vec* pPos, AREA_DATA* pArea)
{
    f32 y = pPos->y + 100.0f;
    f32 a0, c, pz;
    f32 xi, zi, a, b, dx, dz, xmin, xmax, zmin, zmax;
    Vec *pi, *pj;
    Vec* pt[2];  // 8-byte pointer pair = one DImode pseudo (r7:r8); its halves are copied out before each
                 // load (`mr r9,r7; lfs 0(r9)`), which combine cannot fold through the subreg
    int i, count, fx, fz;

    if (y < pArea->Y) {
        return 0;
    }
    if (y >= pArea->Y + pArea->Height) {
        return 0;
    }
    a0 = 1.0f;  // a named 1.0: cse cannot fold it inside the loop (fmsubs/fmadds with f5), pool order 100/1.0/0.0
    pz = pPos->z;
    c = pPos->x - pz;
    count = 0;
    for (i = 0; i < pArea->nVer; i++) {
        pi = &pArea->pVer[i];
        pj = &pArea->pVer[(i + 1) % pArea->nVer];
        dx = pj->x - pi->x;
        dz = pj->z - pi->z;
        pt[0] = pi;
        pt[1] = pj;
        if (dz != 0.0f) {
            a = dx / dz;
            b = pi->x - a * pi->z;
            if (a0 == a) {
                continue;
            }
            xi = (a0 * b - c * a) / (a0 - a);
            zi = (b - c) / (a0 - a);
        } else {
            zi = pi->z;
            xi = a0 * zi + c;
        }
        if (dx > 0.0f) {
            xmin = pt[0]->x;
            xmax = pt[1]->x;
            fx = 0;
        } else {
            xmin = pt[1]->x;
            xmax = pt[0]->x;
            fx = 1;
        }
        if (dz > 0.0f) {
            zmin = pt[0]->z;
            zmax = pt[1]->z;
            fz = 0;
        } else {
            zmin = pt[1]->z;
            zmax = pt[0]->z;
            fz = 1;
        }
        if (!(xi >= pPos->x)) {
            continue;
        }
        if (fx == 0) {
            if (!(xi >= xmin && xi < xmax)) {
                continue;
            }
        } else {
            if (!(xi > xmin && xi <= xmax)) {
                continue;
            }
        }
        if (fz == 0) {
            if (!(zi >= zmin && zi < zmax)) {
                continue;
            }
        } else {
            if (!(zi > zmin && zi <= zmax)) {
                continue;
            }
        }
        count++;
    }
    if (count & 1) {
        return 1;
    }
    return 0;
}

// Per-frame area check: picks the calm or battle cut attribute and switches to the first enabled
// area containing the player. With no area, the shoulder camera takes over.
void CameraControl::areaHitCheck()
{
    static u8 blink = 0;
    CAM_FILE_HEADER* d;
    CUT_INFO* rec;
    CUT_INFO* first;
    AREA_DATA* pArea;
    CAMERA_DATA* cut;
    u8 attr = 1;
    u8 old_attr;
    int old_area = areaNo;
    s8 i;

    if (DbgFlagChk(pG, DBG_CAM_AREA_OFF)) {
        return;
    }
    d = pCamData;
    if (d == NULL) {
        cameraNo = -1;
        areaNo = -1;
        areaSuffix = -1;
        r0 = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (m_system_flag & 0x10)) {
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        area_rec = NULL;
        return;
    }
    if (m_system_flag & 1) {
        if (m_system_flag & 0x10) {
            r0 = 0xA;
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        return;
    }
    if (cameraDataVersion((char*) d) <= 1) {
        cameraNo = -1;
        areaNo = -1;
        areaSuffix = -1;
        r0 = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (m_system_flag & 0x10)) {
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        area_rec = NULL;
        return;
    }

    if (SubCharGetStatus() & 0x20000000) {
        attr = 2;
    } else {
        switch (pG->pl_type) {
        case 0:
            break;
        case 1:
            if (pG->game_country == 0) {
                attr = 4;
            }
            break;
        case 2:
            attr = 8;
            break;
        case 3:
            attr = 0x20;
            break;
        case 5:
            attr = 0x10;
            break;
        case 4:
            attr = 0x40;
            break;
        }
    }

    first = rec = (CUT_INFO*) (d + 1);
    for (i = 0; i < d->nAdat; i++, rec++) {
        pArea = rec->pAdat;
        cut = rec->pCdat;
        if (areaAttr(pArea, 0x20, attr) && areaHit(&pPL->pos, pArea, pPL->ang.y)) {
            if ((m_system_flag & 0x10) || cut->No != cameraNo) {
                switchCamera(rec);
            }
            return;
        }
    }

    old_attr = m_cut_attr;
    if (DbgFlagChk(pG, DBG_BATTLE_CAM)) {
        m_cut_attr = 2;
        if (blink++ & 0x18) {
            eprintf(27, 18, 22, 0, "[ BATTLE ]");
        }
    } else {
        if (Battle_delay > 0) {
            Battle_delay--;
        }
        if (Battle_delay == 0 && EmMgr.isBattle()) {
            m_cut_attr = 2;
        } else {
            m_cut_attr = 1;
        }
    }
    if (m_system_flag & 4) {
        m_cut_attr = 2;
    }
    if (old_attr != m_cut_attr) {
        m_system_flag |= 0x10;
    }

    if (areaNo != -1 && !(m_system_flag & 0x10)) {
        pArea = area_rec->pAdat;
        if (areaAttr(pArea, m_cut_attr, attr) && areaHit(&pPL->pos, pArea, pPL->ang.y)) {
            return;
        }
    }

    rec = first;
    for (i = 0; i < d->nAdat; i++, rec++) {
        pArea = rec->pAdat;
        cut = rec->pCdat;
        if (areaAttr(pArea, m_cut_attr, attr) && areaHit(&pPL->pos, pArea, pPL->ang.y)) {
            if ((m_system_flag & 0x10) || cut->No != cameraNo) {
                switchCamera(rec);
            }
            return;
        }
    }

    areaNo = -1;
    areaSuffix = -1;
    cameraNo = -1;
    area_rec = NULL;
    r0 = 0xA;  // LAST (see the reset arms above); store order found by permutation
    if (m_system_flag & 0x10) {
        m_system_flag &= ~0x10;
        m_QuasiFPS.bindDefaultCamera();
        m_QuasiFPS.init();
        r1 = 0;
        LightMgr.update(0, -1);
    } else if (old_area != -1) {
        CameraQuasiFPS* q = &m_QuasiFPS;
        if (r0_old == 0xA) {
            if (q->readyArrayPtr() && q->transArrayPtr()) {
                q->setBlendData(q->readyArrayPtr(), q->transArrayPtr());
            }
            q->setBlendCount(10);
        }
        q->bindDefaultCamera();
        LightMgr.update(0, -1);
    }
}

// Room start: installs the room / core camera data (be_flag bit0 when present), resets to the
// shoulder camera with default offsets, no area, the behind-camera tuning constants, clears the
// attach cameras; rooms without data start in Wait with the area check off.
void CameraControl::roomInit()
{
    s8 ver;

    StaFlagOn(pG, STA_CAMERA);
    if (pCamData == NULL) {
        be_flag = 0;
    } else {
        m_system_flag = 0x10;
        ver = cameraDataVersion((char*) pCamData);
        switch (ver) {
        case 2:
            OSReport("CameraControl::roomInit(): Ver.02");
            break;
        case 1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.01");
            break;
        case 0:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.00");
            break;
        case -1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Empty!");
            break;
        }
        if (ver >= -1) {
            if (ver > 1) {
                if (ver > 4) {
                    goto clear;
                }
                be_flag |= 1;
            } else {
                be_flag |= 1;
                m_system_flag |= 1;
            }
        } else {
        clear:
            be_flag = 0;
        }
    }
    area_rec = NULL;
    r0 = 0xA;
    be_flag &= ~4;
    m_state_flag &= ~4;
    m_QuasiFPS.offsetCorrection();
    m_QuasiFPS.bindDefaultCamera();
    m_QuasiFPS.setFloorRatio(0.33333334f);
    m_QuasiFPS.init();
    areaNo = -1;
    areaSuffix = -1;
    cameraNo = -1;
    m_behind_fovy = 60.0f;
    m_side_play = 600.0f;
    m_back_play = 400.0f;
    m_ang_h_limit = 0.7853982f;
    m_ang_v_limit = 0.3926991f;
    m_quick_cnt = 0xF;
    m_key_speed = 0.001f;
    m_behind_A_ratio = 0.75f;
    clearAttachCamera();
    m_system_flag |= 2;
    if (SysFlagChk(pG, SYS_DOORDEMO)) {
        r0 = 0;
        m_system_flag |= 8;
    }
    m_pExtraCamera = 0;
    m_pProc = NULL;
    m_Inter.frame = 0;
    Check();
    Move();
    CameraMove();
    QuakeInit();
    memset(m_Free, 9, sizeof(m_Free));
    g_pToolCamData = NULL;
}

// Per-frame, before Move: runs the area check (unless disabled), then handles the fall / drop
// cases: while the player's hip is more than 500 units above his feet (falling, ladder) the
// controller switches to an event-driven camera state (0xB) and back when he lands, unless an
// extra / boss camera or a held cut is active.
void CameraControl::Check()
{
    Vec d;

    if (StaFlagChk(pG, STA_SUB_SCRN)) {
        return;
    }
    if (!(be_flag & 1)) {
        return;
    }
    if (be_flag & 4) {
        return;
    }
    m_state_flag &= ~2;
    r0_old = r0;
    if (!(m_system_flag & 8)) {
        areaHitCheck();
    }
    checkAttachCamera();
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL)) {
        return;
    }
    if (StaFlagChk(pG, STA_EVENT)) {
        return;
    }
    if (m_pExtraCamera != 0) {
        return;
    }
    if (m_state_flag & 4) {
        return;
    }
    if (pPL->Motion.pAttachCam && pPL->Motion.pAttachCam->type) {
        return;
    }
    PSVECSubtract(&pPL->getPartsPtr(1)->world, &pPL->pos, &d);
    if (r0 != 0xB) {
        if (d.y <= 500.0f) {
            m_Inter.set(3, &camera.param);
            r0 = 0xB;
            if (m_pProc) {
                delete m_pProc;
            }
            m_pProc = new (m_Free) CameraLookAt(&camera);
        }
    } else {
        if (d.y > 500.0f) {
            m_Inter.set(30, &camera.param);
            m_system_flag = 0x10;
        }
    }
}

// Per-frame camera computation: runs the r0 routine into `cur`, applies the cut interpolation and
// the smoothing, and rebuilds `camera`.
void CameraControl::Move()
{
    static f32 gain = 2.0f;
    f32 water_y;
    f32 t;
    f32 lim;

    if (StaFlagChk(pG, STA_SUB_SCRN)) {
        return;
    }
    if (!(be_flag & 1)) {
        return;
    }
    m_state_flag &= ~1;
    if (area_rec) {
        CalcAim(area_rec->pCdat);
    }
    switch (r0) {
    case 0:
        r0_Wait();
        break;
    case 1:
        r0_Fix();
        break;
    case 2:
        r0_Pan();
        break;
    case 3:
        r0_Track();
        break;
    case 4:
        r0_RailPan();
        break;
    case 5:
        CamSmth.setRatio(0.0f);
        m_pProc->move();
        cur = m_pProc->param;
        if (((CameraMotion*) m_pProc)->getState() == 1) {
            if (m_pProc) {
                delete m_pProc;
            }
            r0 = 0;
        }
        break;
    case 6:
        r0_RailBehind();
        break;
    case 7:
        r0_Free();
        break;
    case 8:
        r0_Debug();
        break;
    case 9:
        r0_UpCut();
        break;
    case 0xA:
        m_QuasiFPS.move();
        cur = m_QuasiFPS.cam.param;
        break;
    case 0xC:
        CamSmth.setRatio(0.0f);
        m_pProc->move();
        cur = m_pProc->param;
        break;
    case 0x10:
    case 0x11:
        CamSmth.setRatio(0.0f);
        m_pProc->move();
        cur = m_pProc->param;
        break;
    case 0xB:
    case 0xF:
        m_pProc->move();
        cur = m_pProc->param;
        break;
    case 0xD:
        CamSmth.setRatio(0.0f);
        m_pProc->move();
        cur = m_pProc->param;
        break;
    default:
        r0_Debug();
        break;
    }

    if (!StaFlagChk(pG, STA_EVENT)) {
        if (GetWaterHeight(&cur.Campos, &water_y)) {
            t = sinf(cur.Fovy * PI / 360.0f) / cosf(cur.Fovy * PI / 360.0f);
            lim = gain * (ZNEAR * t * 1.3333334f) + water_y;
            if (cur.Campos.y < lim) {
                cur.Campos.y = lim;
            }
        }
    }
    m_Inter.move(&cur);
    if (m_Inter.frame != 0) {
        CamSmth.unsetFlag();
    }
    CamSmth.move(m_Inter.getCamPtr());
    camera.param = *CamSmth.getCamPtr();
    CameraSetOrientationRoll(&camera);
    if (!DbgFlagChk(pG, DBG_DBG_CAM) && (m_state_flag & 4)) {
        pG->Camera = CamCtrl.camera;
    }
}

// The aim point: player position + the cut's aim offset (flags bit0) or the default (1000 up).
void CameraControl::CalcAim(CAMERA_DATA* pCdat)
{
    static Vec offset0 = {0.0f, 1000.0f, 0.0f};

    switch (r0) {
    case 0:
    case 1:
    case 5:
    case 9:
        break;
    default:
        if (pCdat->Attr & 1) {
            PSVECAdd(&pPL->pos, &pCdat->offset, &Aim);
        } else {
            PSVECAdd(&pPL->pos, &offset0, &Aim);
        }
        break;
    }
}

// Always 0 (unused pitch query).
f32 CameraControl::getCameraPitch()
{
    return 0.0f;
}

// Starts an interpolation of `f` frames from camera parameters `p`.
void CameraInterpolation::set(int inter_frame, CAMERA_POINT* p)
{
    frame = inter_frame;
    param = *p;
}

// One step toward the target parameters `p`: param moves 1 / frame of the remaining distance
// each frame; when frame reaches 0 it snaps to `p`.
void CameraInterpolation::move(CAMERA_POINT* p)
{
    CAMERA_POINT tmp;
    f32 r, s;

    if (frame != 0) {
        r = 1.0f / (f32) frame;
        s = 1.0f - r;
        tmp = *p;
        PSVECScale(&param.Campos, &param.Campos, s);
        PSVECScale(&param.Target, &param.Target, s);
        param.Roll *= s;
        param.Fovy *= s;
        PSVECScale(&p->Campos, &p->Campos, r);
        PSVECScale(&p->Target, &p->Target, r);
        p->Roll *= r;
        p->Fovy *= r;
        PSVECAdd(&p->Campos, &param.Campos, &param.Campos);
        PSVECAdd(&p->Target, &param.Target, &param.Target);
        param.Roll += p->Roll;
        param.Fovy += p->Fovy;
        frame--;
    } else {
        frame = 0;
        param = *p;
    }
}

// Resets the smoothing state to `p`.
void CameraSmooth::init(CAMERA_POINT* p)
{
    m_effect = *p;
}

// Exponential smoothing: m_effect = m_ratio * old + (1 - m_ratio) * p (the quake offset is removed
// from the old value first); a set reinit flag snaps to `p`.
void CameraSmooth::move(CAMERA_POINT* p)
{
    Vec tmp;

    if (m_flag & 1) {
        m_flag &= ~1;
        init(p);
        return;
    }
    PSVECAdd(&m_effect.Campos, &pG->quake_ofs, &m_effect.Campos);
    PSVECAdd(&m_effect.Target, &pG->quake_ofs, &m_effect.Target);
    PSVECScale(&m_effect.Campos, &m_effect.Campos, m_ratio);
    PSVECScale(&p->Campos, &tmp, 1.0f - m_ratio);
    PSVECAdd(&m_effect.Campos, &tmp, &m_effect.Campos);
    PSVECScale(&m_effect.Target, &m_effect.Target, m_ratio);
    PSVECScale(&p->Target, &tmp, 1.0f - m_ratio);
    PSVECAdd(&m_effect.Target, &tmp, &m_effect.Target);
    m_effect.Roll *= m_ratio;
    m_effect.Roll = p->Roll * (1.0f - m_ratio) + m_effect.Roll;
    m_effect.Fovy *= m_ratio;
    m_effect.Fovy = p->Fovy * (1.0f - m_ratio) + m_effect.Fovy;
}

// r0 == 0: idle (an event / room owns pG->Camera).
void CameraControl::r0_Wait()
{
}

// r0 == 8: the debug behind camera (Debug_flg): a fixed offset behind / above the player, pulled
// in front of walls.
void CameraControl::r0_Debug()
{
    Vec a;
    Vec b;
    Vec c;
    Vec unused[2];  // 0x18-byte frame slot between c and m in the original
    Mtx m;
    CAMERA_POINT p;
    Vec hit;
    const Vec campos_ofs = {0.0f, 1900.0f, -2000.0f};
    const Vec target_ofs = {0.0f, 1000.0f, 0.0f};
    CAMERA* cam = &camera;
    f32 rate;

    switch (r1) {
    case 0:
        this->campos_ofs = campos_ofs;
        this->target_ofs = target_ofs;
        PSMTXMultVec(pPL->mat, &this->campos_ofs, &p.Campos);
        PSVECAdd(&pPL->pos, &this->target_ofs, &p.Target);
        p.Roll = 0.0f;
        p.Fovy = 55.0f;
        cur = p;
        CamSmth.setFlag();
        r1++;
        break;
    case 1: {
        JOY* joy = &Joy[0];
        Vec* dp = &this->campos_ofs;
        Vec* da = &this->target_ofs;

        if (joy->substickX != 0) {
            PSMTXRotRad(m, 'y', (f32) joy->substickX * 0.05f * DEG2RAD);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        if (joy->substickY != 0) {
            Vec up = {0.0f, 1.0f, 0.0f};

            PSVECCrossProduct(dp, &up, &up);
            PSMTXRotAxisRad(m, &up, (f32) joy->substickY * 0.05f * DEG2RAD);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        rate = 0.8f;
        if (joy->on == 0) {
            if (counter_58++ > 30) {
                rate = 0.8f * 1.2f;  // 0x3F75C290 (0.96f is 0x3F75C28F)
            }
        } else {
            counter_58 = 0;
        }
        PSVECAdd(&pPL->pos, dp, &a);
        PSVECAdd(&pPL->pos, da, &b);
        PSVECScale(&cam->param.Campos, &cam->param.Campos, rate);
        PSVECScale(&a, &c, 1.0f - rate);
        PSVECAdd(&cam->param.Campos, &c, &cam->param.Campos);
        PSVECScale(&cam->param.Target, &cam->param.Target, rate);
        PSVECScale(&b, &c, 1.0f - rate);
        PSVECAdd(&cam->param.Target, &c, &cam->param.Target);
        if (SatMgr.hitCheck(&cam->param.Target, &cam->param.Campos, &hit, NULL, 0x8000, 0)) {
            cam->param.Campos = hit;
        }
        cur = cam->param;
        break;
    }
    }
}

// r0 == 1: fixed camera at the cut's key looking at the aim point (keeps the previous roll / fov
// when the cut has no key).
void CameraControl::r0_Fix()
{
    CAMERA cam;

    CameraSetCutData(&cam, area_rec->pCdat);
    cur = cam.param;
    CamSmth.setFlag();
    r0 = 0;
}

// r0 == 2: fixed position panning to follow the aim point; a multi-key cut turns into a rail
// (Parametrize + searchRail + BSpline).
void CameraControl::r0_Pan()
{
    CAMERA_POINT p;
    CAMERA_DATA* cut = area_rec->pCdat;

    switch (r1) {
    case 0:
        p.Campos = *cut->pCampos;
        p.Roll = *cut->pRoll;
        p.Fovy = *cut->pFovy;
        p.Target = Aim;
        cur = p;
        CamSmth.setRatio(smooth_ratio[1]);
        CamSmth.setFlag();
        r1++;
    case 1:
        p.Campos = camera.param.Campos;
        p.Roll = camera.param.Roll;
        p.Fovy = camera.param.Fovy;
        p.Target = Aim;
        cur = p;
        break;
    }
}

// r0 == 3: the camera slides along the cut's B-spline rail to the point nearest the aim and looks
// at the aim.
void CameraControl::r0_Track()
{
    CAMERA cam;
    CAM_B_SPLINE* bs = &CamBSpline;
    CAMERA_DATA* cut = area_rec->pCdat;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        CamSmth.setRatio(smooth_ratio[2]);
        CamSmth.setFlag();
        r1++;
        break;
    case 1:
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        if (pG->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

// r0 == 4: rail camera whose target is the aim point (rail evaluated every frame).
void CameraControl::r0_RailPan()
{
    CAMERA cam;
    CAM_B_SPLINE* bs = &CamBSpline;
    CAMERA_DATA* cut = area_rec->pCdat;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.Target = Aim;
        cur = cam.param;
        CamSmth.setRatio(smooth_ratio[2]);
        CamSmth.setFlag();
        r1++;
        break;
    case 1:
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.Target = Aim;
        cur = cam.param;
        if (pG->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

// r0 == 9: placeholder (the up-cut camera is the CameraMotion extra started by switchCamera).
void CameraControl::r0_UpCut()
{
}

// r0_RailBehind: argument addresses substituted into the hard-register sets (see COMPILER-DIFF #3 there).
static inline void VecLinComb(Vec* a, Vec* b, f32 s, f32 t, Vec* out)
{
    VecLinearCombination(a, s, b, t, out);
}

// r0 == 6: the behind-the-player camera on a rail: C-stick looks around within the h / v angle
// limits (quick snap to a limit with a short tap), the camera slides along the rail behind the
// player with side / back play zones, fov m_behind_fovy, smoothing m_behind_A_ratio, pulled in
// by cameraHitCheck.
void CameraControl::r0_RailBehind()
{
    static CAMERA camera_old;
    static f32 move_z;
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec pos_old;
    static int init_flg;
    static int edge_camera;
    static int c_rno;
    static int key_flg;
    static Vec ang;
    static int nI = 1;
    static int mI = 2;
    static f32 rate = 0.95f;
    CAMERA cam;
    Mtx m;
    Mtx inv;
    CAMERA* c = &camera;
    CAMERA_DATA* cut = area_rec->pCdat;
    Vec xaxis = {1.0f, 0.0f, 0.0f};
    Vec yaxis = {0.0f, 1.0f, 0.0f};
    Vec zaxis = {0.0f, 0.0f, 1.0f};
    Vec dir;
    Vec v;
    Vec d;
    Vec p0;
    Vec p1;
    Vec p2;
    Vec q;
    Vec q2;
    Vec hit;
    Vec floor;
    Vec a;
    int reset = 0;
    CAM_B_SPLINE* bs = &CamBSpline;
    JOY* joy = &Joy[0];
    int moved;
    int edge;
    f32 t;
    f32 k;
    f32 n;
    f32 mm;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        if (cut->Attr & 1) {
            this->campos_ofs = cut->offset;
            this->target_ofs = *(Vec*) &cut->floor_ratio;
        } else {
            this->campos_ofs = campos_ofs0;
            this->target_ofs = target_ofs0;
        }
        pos_old = pPL->pos;
        memclr_asm(&camera_old, sizeof(CAMERA));
        r2 = 0;
        r1++;
        edge_camera = 0;
        init_flg = 1;
        ang.x = ang.y = ang.z = 0.0f;
        key_flg = 0xFF;
        c_rno = 0;
        reset = 1;
    case 1:
        if (c_rno == 0) {
            if (joy->trg & 0xF00000) {
                if (joy->trg & 0x800000) {
                    if (key_flg == 2) {
                        key_flg = 0;
                    } else {
                        key_flg = 1;
                    }
                }
                if (joy->trg & 0x400000) {
                    if (key_flg == 1) {
                        key_flg = 0;
                    } else {
                        key_flg = 2;
                    }
                }
                if (joy->trg & 0x100000) {
                    if (key_flg == 4) {
                        key_flg = 0;
                    } else {
                        key_flg = 3;
                    }
                }
                if (joy->trg & 0x200000) {
                    if (key_flg == 3) {
                        key_flg = 0;
                    } else {
                        key_flg = 4;
                    }
                }
                c_rno++;
            }
            if (joy->trg & 0x200) {
                ang.x = ang.y = ang.z = 0.0f;
                key_flg = 0;
            }
        } else {
            if (joy->on & 0xF00000) {
                ang.y -= (f32) joy->substickX * m_key_speed;
                ang.x -= (f32) joy->substickY * m_key_speed;
                ang.y = ang.y < -m_ang_h_limit ? -m_ang_h_limit : (ang.y > m_ang_h_limit ? m_ang_h_limit : ang.y);
                ang.x = ang.x < -m_ang_v_limit ? -m_ang_v_limit : (ang.x > m_ang_v_limit ? m_ang_v_limit : ang.x);
                c_rno++;
            } else {
                if (c_rno < m_quick_cnt) {
                    ang.x = 0.0f;
                    ang.y = 0.0f;
                    switch (key_flg) {
                    case 0:
                        break;
                    case 1:
                        ang.x = -m_ang_v_limit;
                        break;
                    case 2:
                        ang.x = m_ang_v_limit;
                        break;
                    case 3:
                        ang.y = m_ang_h_limit;
                        break;
                    case 4:
                        ang.y = -m_ang_h_limit;
                        break;
                    }
                }
                c_rno = 0;
            }
        }
        moved = 0;
        if (PSVECDistance(&pos_old, &pPL->pos) > 50.0f) {
            moved = 1;
        }
        searchRail(bs, cut, &Aim, 0);
        edge = 0;
        if (cut->Attr & 4) {
            if (bs->cand_t == 0.0f || (f32) (cut->nPoint - 1) == bs->cand_t) {
                cam = camera_old;
                edge = 1;
            }
        }
        if (edge_camera != 0) {
            if (edge == 0) {
                edge_camera = 0;
            }
        } else {
            if (edge == 1) {
                edge_camera = 1;
            }
            if ((cut->Attr & 8) && init_flg == 1) {
                edge_camera = 0;
                if (edge == 0) {
                    init_flg = 0;
                }
            }
        }
        t = bs->cand_t;
        BSpline(bs, &cam, 0);
        p0 = cam.param.Target;
        bs->cand_t = t - 0.1f;
        if (bs->cand_t < 0.0f) {
            bs->cand_t = 0.0f;
        }
        BSpline(bs, &cam, 0);
        p1 = cam.param.Target;
        bs->cand_t = t + 0.1f;
        if (bs->cand_t > (f32) (cut->nPoint - 1)) {
            bs->cand_t = (f32) (cut->nPoint - 1);
        }
        BSpline(bs, &cam, 0);
        p2 = cam.param.Target;
        PSVECSubtract(&p1, &p2, &dir);
        dir.y = 0.0f;
        switch (r2) {
        case 0:
            if (init_flg == 1 && edge_camera == 1) {
                PSVECSubtract(&pPL->pos, &p0, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
            } else {
                PSMTXRotRad(m, 'y', pPL->ang.y);
                PSMTXMultVecSR(m, &zaxis, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
                reset = 1;
            }
            r2++;
            break;
        case 1:
            PSVECSubtract(&pPL->pos, &c->param.Campos, &v);
            if (PSVECDotProduct(&v, &dir) < 0.0f) {
                PSVECScale(&dir, &dir, -1.0f);
            }
            break;
        }
#line 2628 "D:/Bio4/Prog/cam_ctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        m[0][0] = xaxis.x;
        m[1][0] = xaxis.y;
        m[2][0] = xaxis.z;
        m[0][1] = yaxis.x;
        m[1][1] = yaxis.y;
        m[2][1] = yaxis.z;
        m[0][2] = dir.x;
        m[1][2] = dir.y;
        m[2][2] = dir.z;
        m[0][3] = p0.x;
        m[1][3] = p0.y;
        m[2][3] = p0.z;
        if (edge_camera) {
            floor = p0;
            floor.y = EatMgr.getFloor(&floor, NULL, 600.0f, 100000.0f, 0);
        } else {
            floor = pPL->pos;
        }
        PSMTXMultVecSR(m, &this->campos_ofs, &cam.param.Campos);
        PSVECAdd(&cam.param.Campos, &floor, &cam.param.Campos);
        PSMTXMultVecSR(m, &this->target_ofs, &cam.param.Target);
        PSVECAdd(&cam.param.Target, &floor, &cam.param.Target);
        if (!(cut->Attr & 1)) {
            cam.param.Fovy = m_behind_fovy;
            cam.param.Roll = 0.0f;
        } else {
            cam.param.Roll = 0.0f;
        }
        PSMTXInverse(m, inv);
        PSMTXMultVec(inv, &cam.param.Campos, &q);
        if (moved) {
            PSMTXMultVec(inv, &cam.param.Target, &q2);
            q2.x = q.x;
            PSMTXMultVec(m, &q2, &cam.param.Target);
        }
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        n = (f32) nI;
        mm = (f32) mI;
        k = 1.0f / (mm + n);
        // COMPILER-DIFF: #3 fresh-addi arguments. The target recomputes the three pointer arguments of
        // this one call (`addi r3,r1,0xb8; addi r4,r1,0xac; addi r5,r1,0x220`) while every surrounding
        // call copies them from the live address registers (`mr r3,r28` ...). Inside this `if` the
        // `&x` arguments of a plain call are precomputed into pseudos and gcse PRE turns them into
        // copies of the reaching registers; through the inline wrapper integrate substitutes the
        // addresses straight into the hard-register argument sets, which PRE never touches.
        VecLinComb(&cam.param.Target, &cam.param.Campos, mm * k, n * k, &floor);
        PSVECSubtract(&cam.param.Target, &cam.param.Campos, &dir);
        dir.y = 0.0f;
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        MtxRotAxisPosRad(m, &xaxis, &floor, a.x);
        PSMTXMultVec(m, &cam.param.Target, &cam.param.Target);
        PSMTXMultVec(m, &cam.param.Campos, &cam.param.Campos);
        MtxRotAxisPosRad(m, &yaxis, &floor, a.y);
        PSMTXMultVec(m, &cam.param.Target, &cam.param.Target);
        PSMTXMultVec(m, &cam.param.Campos, &cam.param.Campos);
        PSMTXRotRad(m, 'y', pPL->ang.y);
        PSMTXMultVecSR(m, &zaxis, &v);
        if (PSVECDotProduct(&v, &dir) < 0.0f) {
            PSVECSubtract(&pos_old, &pPL->pos, &d);
            pos_old = pPL->pos;
            PSMTXMultVecSR(inv, &d, &d);
            move_z = move_z + d.z;
            if (move_z > m_back_play || move_z < -m_back_play) {
                if (edge_camera == 0) {
                    r2 = 0;
                }
            }
        } else {
            move_z = 0.0f;
        }
        if (SatMgr.hitCheck(&cam.param.Target, &cam.param.Campos, &hit, NULL, 0x8000, 0)) {
            cam.param.Campos = hit;
        }
        if (edge_camera) {
            CamSmth.setRatio(rate);
            cam.param.Target = pPL->pos;
            cam.param.Target.y += 1550.0f;
        } else {
            CamSmth.setRatio(m_behind_A_ratio);
        }
        cur = cam.param;
        CamSmth.setFlag();
        if (reset == 1) {
            CamSmth.setRatio(m_behind_A_ratio);
        }
        break;
    }
    pos_old = pPL->pos;
}

// Separate `on & bit` tests: fold merges `(on & a) || (on & b)` on one lvalue into one mask.
static inline u32 JoyOn(JOY* j, u32 bit)
{
    return j->on & bit;
}



// r0 == 7: the free behind camera: orbits the player at a fixed distance with C-stick yaw /
// pitch, recentres behind him when idle, fov m_behind_fovy, pulled in by cameraHitCheck.
void CameraControl::r0_Free()
{
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec ang;
    static Mtx cam_mat;
    CAMERA cam;
    Mtx m;
    Vec hit;
    Vec nrm;
    Vec tmp;
    Vec unused[2];
    JOY* joy = &Joy[0];
    JOY* joy2 = joy;
    f32 rate;

    switch (r1) {
    case 0: {
        PSMTXIdentity(cam_mat);
        cam_mat[0][3] = pPL->mat[0][3];
        cam_mat[1][3] = pPL->mat[1][3];
        cam_mat[2][3] = pPL->mat[2][3];
        ang.x = 0.0f;
        ang.y = pPL->ang.y;
        ang.z = 0.0f;
        Vec xaxis = {1.0f, 0.0f, 0.0f};
        Vec yaxis = {0.0f, 1.0f, 0.0f};
        Vec tofs;
        Vec a;
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        tofs = target_ofs0;
        MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
        PSMTXMultVec(m, &campos_ofs0, &this->campos_ofs);
        PSMTXMultVec(m, &target_ofs0, &this->target_ofs);
        MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
        PSMTXMultVec(m, &this->campos_ofs, &this->campos_ofs);
        PSMTXMultVec(m, &this->target_ofs, &this->target_ofs);
        PSMTXMultVec(cam_mat, &this->campos_ofs, &cam.param.Campos);
        PSMTXMultVec(cam_mat, &this->target_ofs, &cam.param.Target);
        cam.param.Roll = 0.0f;
        cam.param.Fovy = m_behind_fovy;
        cur = cam.param;
        CamSmth.setFlag();
        r2 = 0;
        r1++;
    }
    case 1: {
        ang.y -= (f32) joy->substickX * 0.00125f;
        ang.x -= (f32) joy->substickY * 0.00125f;
        ang.x = ang.x < -0.7853982f ? -0.7853982f : (ang.x > PI * 0.35f ? PI * 0.35f : ang.x);
        ang.y = ang.y < -PI ? PI : (ang.y > PI ? -PI : ang.y);
        {
            cam_mat[0][3] = pPL->mat[0][3];
            cam_mat[1][3] = pPL->mat[1][3];
            cam_mat[2][3] = pPL->mat[2][3];
            Vec xaxis = {1.0f, 0.0f, 0.0f};
            Vec yaxis = {0.0f, 1.0f, 0.0f};
            Vec tofs;
            Vec a;
            if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
                PSVECScale(&ang, &a, -1.0f);
            } else {
                a = ang;
            }
            tofs = target_ofs0;
            MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
            PSMTXMultVec(m, &campos_ofs0, &this->campos_ofs);
            PSMTXMultVec(m, &target_ofs0, &this->target_ofs);
            MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
            PSMTXMultVec(m, &this->campos_ofs, &this->campos_ofs);
            PSMTXMultVec(m, &this->target_ofs, &this->target_ofs);
        }
        {
            int st = r2;  // COMPILER-DIFF 2: the int copy + (u8) re-extend the loaded byte as the original does

            switch ((u8) st) {
            case 0:
                if ((joy->trg & 0x200) || JoyOn(joy, 0x200) || JoyOn(joy, 0x20)) {
                    r2 = st + 1;
                }
                break;
            case 1: {
                Vec d;

                d.x = 0.0f - ang.x;
                d.y = pPL->ang.y - ang.y;
                d.z = 0.0f;
                VecRadLimit(&d);
                ang.y += d.y * 0.1f;
                ang.x += d.x * 0.1f;
                if (!JoyOn(joy, 0x200) && !JoyOn(joy, 0x20)) {
                    // the pairs fold to one halfword test each; the second pointer keeps fold
                    // from merging all four bytes into one word compare
                    if (PSVECMag(&d) < 0.05f || (joy->substickX != 0 || joy->substickY != 0) || (joy2->stickX != 0 || joy2->stickY != 0)) {
                        r2--;
                    }
                }
                break;
            }
            }
        }
        PSMTXMultVec(cam_mat, &this->campos_ofs, &cam.param.Campos);
        PSMTXMultVec(cam_mat, &this->target_ofs, &cam.param.Target);
        cam.param.Roll = 0.0f;
        cam.param.Fovy = m_behind_fovy;
        rate = 0.8f;
        PSVECScale(&cam.param.Campos, &cam.param.Campos, rate);
        PSVECScale(&cur.Campos, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.Campos, &tmp, &cam.param.Campos);
        PSVECScale(&cam.param.Target, &cam.param.Target, rate);
        PSVECScale(&cur.Target, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.Target, &tmp, &cam.param.Target);
        {
            Vec from = cam.param.Target;
            Vec to = cam.param.Campos;

            if (cameraHitCheck(&hit, &nrm, &from, &to)) {
                cam.param.Campos = hit;
            }
        }
        cur = cam.param;
        break;
    }
    }
}

// Shoulder camera: delay before it snaps to the stored player matrix (search frames).
void CamCtrlShoulderSetSearchFrame(s16 frame)
{
    CamCtrl.m_QuasiFPS.m_search_frame = frame;
    CamCtrl.m_QuasiFPS.m_search_cnt = 0;
}

// Shoulder camera: the aim point used during the search delay.
void CamCtrlShoulderSetAim(Vec* pos)
{
    CamCtrl.m_QuasiFPS.m_Aim = *pos;
}

// Defined here: a float literal in an inline body in the shared header moves constant pool labels in
// other units.
inline void CameraQuasiFPS::resetDepressionRatio()
{
    m_depression_ratio = 0.0f;
}

inline void CameraQuasiFPS::resetDirectionRatio()
{
    m_direction_ratio = 0.0f;
}

// Shoulder camera: clears the C-stick look angles.
void CameraControl::resetCameraAngle()
{
    CameraQuasiFPS* q = &CamCtrl.m_QuasiFPS;

    q->resetDepressionRatio();
    q->resetDirectionRatio();
}

// Shoulder camera: returns and clears the yaw look angle (the player turns by it).
f32 CameraControl::getCameraDirection()
{
    f32 dir = CamCtrl.m_QuasiFPS.m_direction_ratio;
    CamCtrl.m_QuasiFPS.resetDirectionRatio();
    return dir;
}

// Fits the cut's keys (pos, at, roll, fov) with a B-spline of degree min(2, num - 1): solves the
// de Boor-Cox basis matrix for the control points (temporary MEM_ALLOC buffers).
void Parametrize(CAMERA_DATA* pCdat, CAM_B_SPLINE* pB)
{
    int i;
    f32* B;
    f32* Binv;
    f32* px;
    f32* py;
    f32* pz;
    f32* ax;
    f32* ay;
    f32* az;
    f32* roll;
    f32* fovy;

    pB->p_num = pCdat->nPoint;
    if (pB->p_num > 1) {
#line 3058 "D:/Bio4/Prog/cam_ctrl.cpp"
        B = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num * pB->p_num, 1, 0xd);
        Binv = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num * pB->p_num, 1, 0xd);
        px = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        py = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        pz = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        ax = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        ay = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        az = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        roll = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        fovy = (f32*) MEM_ALLOC(sizeof(f32) * pB->p_num, 1, 0xd);
        pB->order = 2;
        if (pB->order > pB->p_num - 1) {
            pB->order = pB->p_num - 1;
        }
        for (i = 0; i < pB->p_num; i++) {
            px[i] = pCdat->pCampos[i].x;
            py[i] = pCdat->pCampos[i].y;
            pz[i] = pCdat->pCampos[i].z;
            ax[i] = pCdat->pTarget[i].x;
            ay[i] = pCdat->pTarget[i].y;
            az[i] = pCdat->pTarget[i].z;
            roll[i] = pCdat->pRoll[i];
            fovy[i] = pCdat->pFovy[i];
        }
        for (i = 0; i < pB->p_num; i++) {
            de_Boor_Cox(pB->p_num, NULL, (f32) i, pB->order, &B[pB->p_num * i]);
        }
        MtxNNInverse(pB->p_num, B, Binv);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, px, pB->c_alpha);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, py, pB->c_beta);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, pz, pB->c_gamma);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, ax, pB->t_alpha);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, ay, pB->t_beta);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, az, pB->t_gamma);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, roll, pB->r_alpha);
        MtxNNMultVecSR(pB->p_num, pB->p_num, Binv, fovy, pB->f_alpha);
        Mem_free(B);
        Mem_free(Binv);
        Mem_free(px);
        Mem_free(py);
        Mem_free(pz);
        Mem_free(ax);
        Mem_free(ay);
        Mem_free(az);
        Mem_free(roll);
        Mem_free(fovy);
    }
}

// Evaluates the rail at parameter bs->t into the camera's pos / at / roll / fov.
void BSpline(CAM_B_SPLINE* bs, CAMERA* cam, int)
{
    int i;

    memclr_asm(cam, sizeof(CAMERA));
    de_Boor_Cox(bs->p_num, NULL, bs->cand_t, bs->order, bs->B);
    for (i = 0; i < bs->p_num; i++) {
        cam->param.Target.x += bs->B[i] * bs->t_alpha[i];
        cam->param.Target.y += bs->B[i] * bs->t_beta[i];
        cam->param.Target.z += bs->B[i] * bs->t_gamma[i];
        cam->param.Campos.x += bs->B[i] * bs->c_alpha[i];
        cam->param.Campos.y += bs->B[i] * bs->c_beta[i];
        cam->param.Campos.z += bs->B[i] * bs->c_gamma[i];
        cam->param.Roll += bs->B[i] * bs->r_alpha[i];
        cam->param.Fovy += bs->B[i] * bs->f_alpha[i];
    }
}

// Finds the rail parameter nearest the aim point: projects the aim on every key segment of the
// cut's `at` polyline (falls back to the nearest key), storing t and the segment.
void searchRail(CAM_B_SPLINE* bs, CAMERA_DATA* cut, Vec* aim, int)
{
    Vec d;
    Vec v;
    f32 min = 10000000000.0f;
    int found = 0;
    int i;
    f32 dot;
    f32 s;
    f32 dist;

    for (i = 0; i < cut->nPoint - 1; i++) {
        // One variable per value (each block-local with a single death): `dot0` for the first
        // product, `dot` for the second, `prod` tied to `dot` (`fmuls f31, f30, f31`).
        f32 dot0;
        f32 prod;

        PSVECSubtract(&cut->pTarget[i + 1], &cut->pTarget[i], &d);
        d.y = 0.0f;
        PSVECSubtract(aim, &cut->pTarget[i], &v);
        v.y = 0.0f;
        dot0 = PSVECDotProduct(&d, &v);
        s = dot0 / PSVECMag(&d);
        PSVECSubtract(aim, &cut->pTarget[i + 1], &v);
        v.y = 0.0f;
        dot = PSVECDotProduct(&d, &v);
        dot = dot / PSVECMag(&d);
        prod = s * dot;
        if (prod < 0.0f) {
            d.y = cut->pTarget[i + 1].y - cut->pTarget[i].y;
            PSVECScale(&d, &v, s / PSVECMag(&d));
            PSVECAdd(&v, &cut->pTarget[i], &v);
            dist = PSVECDistance(aim, &v);
            if (dist < min) {
                min = dist;
                bs->cand_t = (f32) i + s / PSVECMag(&d);
                bs->history_i = i;
                found = 1;
            }
        }
    }
    if (found) {
        f32 min2 = 10000000000.0f;
        int seg = 0;

        for (i = 0; i < cut->nPoint; i++) {
            PSVECSubtract(aim, &cut->pTarget[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                seg = i;
            }
        }
        if (min > min2) {
            bs->history_i = seg;
            bs->cand_t = (f32) seg;
        }
    } else {
        f32 min2 = 10000000000.0f;

        for (i = 0; i < cut->nPoint; i++) {
            PSVECSubtract(aim, &cut->pTarget[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                bs->history_i = i;
                bs->cand_t = (f32) i;
            }
        }
    }
}

// Debug: draws the cut's rail (spline samples) and its keys.
void CameraControl::debugDrawRail(CAMERA_DATA* pCdat)
{
    static Vec Fc_old;
    static Vec Ft_old;
    CAM_B_SPLINE* bs = &CamBSpline;
    Vec fc;
    Vec ft;
    int i;
    int j;

    for (i = 0; i < 100; i++) {
        de_Boor_Cox(pCdat->nPoint, NULL, (f32) ((pCdat->nPoint - 1) * i) / 100.0f + 0.0f, bs->order, bs->B);
        fc.x = 0.0f;
        fc.y = 0.0f;
        fc.z = 0.0f;
        ft.x = 0.0f;
        ft.y = 0.0f;
        ft.z = 0.0f;
        for (j = 0; j < pCdat->nPoint; j++) {
            fc.x += bs->B[j] * bs->c_alpha[j];
            fc.y += bs->B[j] * bs->c_beta[j];
            fc.z += bs->B[j] * bs->c_gamma[j];
            ft.x += bs->B[j] * bs->t_alpha[j];
            ft.y += bs->B[j] * bs->t_beta[j];
            ft.z += bs->B[j] * bs->t_gamma[j];
        }
        if (i > 0) {
            Draw_line3d(&Fc_old, &fc, 0xFF2020FF, 0);
            Draw_line3d(&Ft_old, &ft, 0xFF20FF20, 0);
        }
        Fc_old = fc;
        Ft_old = ft;
    }
}

CameraControl CamCtrl;
CAM_B_SPLINE CamBSpline;
CameraSmooth CamSmth;

// Stores the up-cut placement (sel 0 position, 1 angles, 2 scale) used by the up-cut motion
// camera.
void CameraControl::UpCutCall(int cutNo, Vec* pos, Vec* ang, Vec* scale, int type)
{
    switch (type) {
    case 0:
        pCamData = (CAM_FILE_HEADER*) pG->pCamCore;
        break;
    case 1:
        pCamData = (CAM_FILE_HEADER*) pG->pCamRoom;
        break;
    }
    if (pos) {
        upcut_pos = *pos;
    }
    if (ang) {
        upcut_ang = *ang;
    }
    if (scale) {
        upcut_scale = *scale;
    }
    CutCall(cutNo);
}

// Enters the push-object camera (r0 0xF, CameraPushObject extra).
void CameraControl::startPushObject()
{
    m_pProc = new (m_Free) CameraPushObject();
    r0 = 0xF;
    AreaCheckOnOff(0);
}

// Leaves the push-object camera and returns to the area cameras.
void CameraControl::endPushObject()
{
    if (m_pProc) {
        delete m_pProc;
    }
    Comeback(0);
}

// Enters the look-down camera on enemy `em` (r0 0xD) from above the player; hides the HUD
// (Status_flg[0] 0x2000000 off).
void CameraControl::StartLookDownEm(void* pEm)
{
    Vec c;
    cParts* p[2];

    p[0] = pPL->getPartsPtr(0x20);
    p[1] = pPL->getPartsPtr(0x21);
    PSVECAdd(&p[0]->world, &p[1]->world, &c);
    PSVECScale(&c, &c, 0.5f);
    m_pProc = new (m_Free) CameraLookDownEm(pEm, &c);
    r0 = 0xD;
    AreaCheckOnOff(0);
    StaFlagOff(pG, STA_SSCRN_ENABLE);
}

// Leaves the look-down camera, HUD back on.
void CameraControl::EndLookDownEm()
{
    if (m_pProc) {
        delete m_pProc;
    }
    Comeback(0);
    StaFlagOn(pG, STA_SSCRN_ENABLE);
}

// Enters the rifle scope camera (r0 0x10; Status_flg[0] 0x40 scope, 0x8000 first-person view).
void CameraControl::startScope(Vec* campos, Vec* target)
{
    if (!StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        StaFlagOn(pG, STA_SCOPE_CAMERA);
        StaFlagOn(pG, STA_LOOK_THROUGH);
        m_pProc = new (m_Free) CameraScope(campos, target);
        r0 = 0x10;
        DpfFlagOn(pG, DPF_PL);
        AreaCheckOnOff(0);
    }
}

// Leaves the scope camera.
void CameraControl::endScope()
{
    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        StaFlagOff(pG, STA_SCOPE_CAMERA);
        StaFlagOff(pG, STA_LOOK_THROUGH);
        DpfFlagOff(pG, DPF_PL);
        if (m_pProc) {
            delete m_pProc;
        }
        Comeback(0);
    }
}

// While scoped: the scope camera's pos / at (the rifle's shot line).
void CameraControl::getTrajectory(Vec* p_pos0, Vec* p_pos1)
{
    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        ((CameraScope*) m_pProc)->getTrajectory(p_pos0, p_pos1);
    }
}

// Saves the scope zoom / pitch and reticle timers before the scope is closed.
void CameraControl::saveScopeParam()
{
    ((CameraScope*) m_pProc)->getParam(&m_scope_zoom, &m_scope_ang_x);
    ((CameraScope*) m_pProc)->m_id.save(0);
}

// Restores them when the scope is re-opened.
void CameraControl::loadScopeParam()
{
    ((CameraScope*) m_pProc)->setParam(m_scope_zoom, m_scope_ang_x);
    ((CameraScope*) m_pProc)->m_id.load(0);
}

// Limits the binocular view angles.
void CameraControl::SetBinocularRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up)
{
    ((CameraBinocular*) m_pProc)->setRange(x_low, x_up, y_low, y_up);
}

// Enters the binocular camera (r0 0xC; Status_flg[0] 0x400 binocular, 0x8000 first person) with
// its HUD data.
void CameraControl::HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at)
{
    StaFlagOn(pG, STA_BINOCULAR);
    StaFlagOn(pG, STA_LOOK_THROUGH);
    m_pProc = new (m_Free) CameraBinocular(pos, at, id_a, id_b);
    r0 = 0xC;
    DpfFlagOn(pG, DPF_PL);
    AreaCheckOnOff(0);
}

// Leaves the binocular camera.
void CameraControl::LowerBinocular()
{
    StaFlagOff(pG, STA_BINOCULAR);
    StaFlagOff(pG, STA_LOOK_THROUGH);
    DpfFlagOff(pG, DPF_PL);
    if (m_pProc) {
        delete m_pProc;
    }
    Comeback(0);
}

// The binocular HUD data pointers.
void CameraControl::GetBinocularIDAddr(void** eff_addr, void** uwf_addr)
{
    *eff_addr = ((CameraBinocular*) m_pProc)->getEffAddr();
    *uwf_addr = ((CameraBinocular*) m_pProc)->getUwfAddr();
}

// Plays a camera motion file (cutscene camera, r0 5) interpolating from the current camera over
// `frame` frames; flags the event camera (m_system_flag 0x28, Status_flg[2] 0x10000000).
void CameraControl::MotionSet(void* motion, f32 speed, int frame)
{
    m_system_flag |= 0x28;
    StaFlagOn(pG, STA_CUT_CHANGE);
    m_pProc = new (m_Free) CameraMotion(motion, 0, 0, speed);
    ((CameraMotion*) m_pProc)->setBaseMatPtr(NULL);
    r0 = 5;
    m_Inter.set(frame, &pG->Camera.param);
}

// 1 while a camera motion is playing (m_system_flag 0x20).
int CameraControl::IsMotionSet()
{
    if (m_system_flag & 0x20) {
        return 1;
    }
    return 0;
}

// 1 when no camera motion is set or the current one reached its end.
int CameraControl::IsMotionEnd()
{
    if (r0 != 5) {
        return 1;
    }
    return ((CameraMotion*) m_pProc)->getState() == 1;
}

// Places the camera motion in the world through `mat` (event position).
void CameraControl::setMotionBaseMatPtr(Mtx* p_mat)
{
    ((CameraMotion*) m_pProc)->setBaseMatPtr(p_mat);
}

// The playing camera motion's work (frame / state).
MOTION_INFO* CameraControl::getMotionInfoPtr()
{
    return (MOTION_INFO*) ((CameraMotion*) m_pProc)->getInfoPtr();
}

// Forgets all registered attach cameras (motion-driven cameras of models).
void CameraControl::clearAttachCamera()
{
    int i;

    m_attach_num = 0;
    m_p_attach_model_old = NULL;
    for (i = 0; i < 3; i++) {
        m_p_model[i] = NULL;
        m_p_attach[i] = NULL;
    }
}

// Registers (or replaces) the attach camera of `model` (up to 3).
void CameraControl::registAttachCamera(ATTACH_CAMERA* p_attach, cModel* p_model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (p_model == m_p_model[i]) {
            m_p_attach[i] = p_attach;
            return;
        }
    }
    for (i = 0; i < 3; i++) {
        if (m_p_model[i] == NULL) {
            m_attach_num++;
            m_p_model[i] = p_model;
            m_p_attach[i] = p_attach;
            return;
        }
    }
    pLog->err(0, 0, "registAttachCamera(): lack of ptr table.");
}

// Unregisters the attach camera of `model`.
void CameraControl::deleteAttachCamera(ATTACH_CAMERA* p_attach, cModel* p_model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (p_model == m_p_model[i] && p_attach == m_p_attach[i]) {
            m_attach_num--;
            m_p_model[i] = NULL;
            m_p_attach[i] = NULL;
            return;
        }
    }
}

// The model whose attach camera is active (any when `model` is NULL, else `model` if registered).
cModel* CameraControl::getAttachModel(cModel* p_model)
{
    int i;

    if (p_model == NULL) {
        for (i = 0; i < 3; i++) {
            if (m_p_model[i] != NULL) {
                return m_p_model[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (p_model == m_p_model[i]) {
                return p_model;
            }
        }
    }
    return NULL;
}

// The active attach camera (any when `model` is NULL, else that model's).
ATTACH_CAMERA* CameraControl::getAttachCamera(cModel* p_model)
{
    int i;

    if (p_model == NULL) {
        for (i = 0; i < 3; i++) {
            if (m_p_model[i] != NULL) {
                return m_p_attach[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (p_model == m_p_model[i]) {
                return m_p_attach[i];
            }
        }
    }
    return NULL;
}

// Per-frame: when a registered model's motion has an active camera track, switches to the
// attached-motion camera (r0 0x11, interpolating over the track's frame count) and back to the
// area cameras when it ends. Not while the scope is up.
void CameraControl::checkAttachCamera()
{
    static int inter_frame;
    cModel* em[3] = {NULL, NULL, NULL};
    cModel* model = NULL;
    ATTACH_CAMERA* ac;
    int i;

    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        return;
    }
    if (m_state_flag & 4) {
        return;
    }
    switch (getAttachCameraNum()) {
    case 0:
        break;
    case 1:
        model = getAttachModel(NULL);
        break;
    default:
        for (i = 0; i < 2; i++) {
            model = getAttachModel(em[i]);
            if (model) {
                break;
            }
        }
        break;
    }
    if (model) {
        ac = getAttachCamera(model);
        if (m_p_attach_model_old != model) {
            m_system_flag |= 8;
            m_Inter.set(ac->frame, &pG->Camera.param);
            r0 = 0x11;
            if (m_pProc) {
                delete m_pProc;
            }
            m_pProc = new (m_Free) CameraAttachedToMotion(model);
            m_pProc->param.Campos = pG->Camera.param.Campos;
            m_pProc->param.Target = pG->Camera.param.Target;
            m_pProc->param.Roll = pG->Camera.param.Roll;
            m_pProc->param.Fovy = pG->Camera.param.Fovy;
        }
        inter_frame = ac->frame;
    } else if (m_p_attach_model_old) {
        m_system_flag = 0x10;
        m_Inter.set(inter_frame, &pG->Camera.param);
    }
    m_p_attach_model_old = model;
}

// The numeric version of a camera data file ("B40x" -> x); -1 when not camera data.
int cameraDataVersion(char* verStr)
{
    if (strncmp(verStr, "B404", 4) == 0) {
        return 4;
    }
    if (strncmp(verStr, "B403", 4) == 0) {
        return 3;
    }
    if (strncmp(verStr, "B402", 4) == 0) {
        return 2;
    }
    if (strncmp(verStr, "B401", 4) == 0) {
        return 1;
    }
    if (strncmp(verStr, "B400", 4) == 0) {
        return 0;
    }
    strncmp(verStr, "EMPT", 4);
    return -1;
}
