#ifndef SOFDEC_H
#define SOFDEC_H

#include "types.h"
#include "db_log.h"
#include "gx.h"
#include "vec.h"
#include "mwply.h"
#include "cString.h"

#line 8 "D:/Bio4/Prog/sofdec.h"

// Camera matrices of a scene: view and projection.
struct CamObj {
    Mtx view;    // 0x00
    Mtx44 proj;  // 0x30
}; // 0x70

// Movie texture: the decoded frame either as Y8 + UV (IA8) planes (mode 0) or one RGBA8 texture
// (mode 1). The two layouts share the storage after width/height.
struct TexObj {
    int width;  // 0x00  (the u16 half at +2 is what GX gets)
    int height; // 0x04
    union {
        struct {
            GXTexObj ytobj;  // 0x08
            GXTexObj ctobj;  // 0x28
            u8* ybuf;        // 0x48
            u32 ybufsiz;     // 0x4C
            u8* cbuf;        // 0x50
            u32 cbufsiz;     // 0x54
        };
        struct {
            GXTexObj tobj;   // 0x08
            u8* rgbbuf;      // 0x28
            u32 bufsiz;      // 0x2C
        };
    };
}; // 0x58

// Render state of a scene: the camera and the movie texture.
struct SceneCtrlObj {
    CamObj cam;  // 0x00
    TexObj tex;  // 0x70
}; // 0xC8

// Player state around the MWPLY handle.
struct AP_OBJ {
    MWPLY mwply;             // 0x00
    MWS_PLY_INIT_SFD iprm;   // 0x04
    MWS_PLY_CPRM_SFD cprm;   // 0x24  PS2's MwsfdCrePrm is 0x30 bytes, the SDK struct ends at 0x48
    u8 pad_48[0xC];          // 0x48
    int mwstat;              // 0x54
    MWS_FRM frm;             // 0x58
    s8* mwsfd_wkadr;         // 0xE0
    int term_flag;           // 0xE4
    int disp_flag;           // 0xE8  1: draw the debug frame info
    char fname[0x40];        // 0xEC
    u16 fno;                 // 0x12C
    u8 pad_12E[2];
}; // 0x130

// Sofdec movie player front end (game/sofdec.cpp, `Sofdec`, 0x240 bytes). The inline range check
// emits the file-name string into the .rodata of every unit that includes it.
class cSofdec {
private:
    u32 m_be_flag;         // 0x00  bit0: a movie is playing, bit2: paused, bit5: skipped, bit8: keep black
    AP_OBJ m_ap_obj;    // 0x08
    SceneCtrlObj m_scn_ctrl;   // 0x138
    s16 m_width;        // 0x200
    s16 m_height;       // 0x202
    int m_is_set_black;       // 0x204  1 from the start of playback until the first frame arrives
    u32 m_stop_flg_bak;      // 0x208
    u32 m_disp_flg_bak;       // 0x20C
    u32 m_clrsize;    // 0x210
    u8 m_save_cur_heap;        // 0x214
    s8 m_vcnt_save;          // 0x215
    u16 m_frame_no;          // 0x216
    int m_isScreenResize;      // 0x218
    int m_draw_mode;         // 0x21C
    char m_fname[0x20];  // 0x220

    void drawTex();
    void drawQuad(SceneCtrlObj* d);
    void drawPolygon(SceneCtrlObj* d);
    void setCamera(SceneCtrlObj* d);
    void loadMvFrmFx(MWPLY hn, MWS_FRM* frm);
    void allocTexMem(TexObj* tex, int w, int h);
    void clrTexMem(TexObj* tex);
    void initDraw(SceneCtrlObj* d);
    void initApp(const char* fname);
    int startApp();
    void initSync();
    int appMain();
    void draw();
    void finishMovie();
    int initWork(const char* fname);
    int initSub(const char* fname, u32 flags);

public:
    int Initialize(const char* fname, u32 flags);
    int Initialize(cString& fname, u32 flags);
    int Move();
    static void ThreadMove(cSofdec* pThis);

    // playing check: `if (Sofdec.flag & 1) return 1; return 0;` form (li 0 / li 1)
    int IsActive() {
        if (m_be_flag & 1) {
            return 1;
        }
        return 0;
    }
    // 1 when `bit` is set in m_be_flag (0x100 = keep the screen black after the movie).
    int ckFlag(u32 bit) {
        return (m_be_flag & bit) ? 1 : 0;
    }
    // Playback was skipped by the player (bit 5).
    int isCancel() {
        return (m_be_flag & 0x20) ? 1 : 0;
    }
    // Placeholder. Some inline in this header holds an assert whose __FILE__ string is in the .rodata of every
    // unit that includes it, but none of the in-use inlines call the assert.
    // Its name, body and line are unknown; this only reproduces the string.
    void placeholder() {
        dbgAssert(__FILE__, __LINE__);
    }

    void PlayPause(int sw);
    ~cSofdec() {}
};

extern cSofdec Sofdec;

extern "C" {
void ADXM_ExecMain();
void SofdecInit();
void UsrSfcnt2time(int sf, int ncnt, int* hh, int* mm, int* ss, int* ff);
void disp_info(AP_OBJ* app);
void setTevPrm(int mapY, int mapUV);
void restoreTevPrm();
void ap_mwply_err_func(void* obj, const char* errmsg);
}

#endif
