#ifndef DB_TOOLBASE_H
#define DB_TOOLBASE_H

#include "types.h"
#include "db_log.h"

extern "C" unsigned int strlen(const char* s);
extern "C" char* strcpy(char* dst, const char* src);

// Debug tool window/button base (D:/Bio4/Prog/db_toolbase.h; bodies in Tools/db_toolbase.cpp, the same
// object is t_event's db_filelist.cpp and most of Sscrn's ss_term.cpp). The string-returning inlines
// reproduce the header's parse-time strings (none of the four polymorphic classes may own them: every
// in-class inline of a vtable-owning class is emitted out of line).

u32 MakeCol(f32 r, f32 g, f32 b, f32 a);
void DbgDrawBox(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);

class cDbgButtonBase {
public:
    u32 m_px;          // 0x00  column inside the window
    u32 m_py;          // 0x04  row inside the window
    int m_cx;         // 0x08  cursor column
    int m_cy;         // 0x0C  cursor row
    char* m_pStr;    // 0x10  own copy of the label
    u32 m_strlen;    // 0x14  strlen + 1
    // 0x18 vptr

    virtual ~cDbgButtonBase() { delete m_pStr; }

    int Init(int x_, int y_, const char* name, int cx_, int cy_) {
        int len;

        m_px = x_;
        m_py = y_;
        m_cx = cx_;
        m_cy = cy_;
        len = strlen(name) + 1;
        m_strlen = len;
        m_pStr = new char[len];
        if (m_pStr == 0) {
            pLog->err(0, 0, "cDbgButtonBase::Init(): new failed.");
            return 0;
        }
        strcpy(m_pStr, name);
        return 1;
    }
    int GetCx() { return m_cx; }
    int GetCy() { return m_cy; }
    // Replaces the label (truncated to the allocated length).
    void SetString(const char* s) {
        if (strlen(s) > m_strlen) {
            u32 i;

            for (i = 0; i < m_strlen - 2; i++) {
                m_pStr[i] = s[i];
            }
            m_pStr[i] = 0;
        } else {
            strcpy(m_pStr, s);
        }
    }
};

// The cursor mark of cDbgWindow::LocalDisp (a header inline owns it: it opens the .rodata string
// group of every unit including this header, right after the Init message). The other display
// strings of the group come from the file-select / ok-cancel window inlines of dbg_tool.h, which
// every user of this header (db_toolbase.cpp included) parses after it.
struct cDbgStr {
    static const char* cursor() { return ">"; }
};

class cDbgWindowBase {
public:
    u32 m_px;              // 0x00  window column (8 px units; unsigned: the frame conversions use the 2^52 trick without xoris)
    u32 m_py;              // 0x04  window row (14 px units)
    u32 m_wx;              // 0x08  width in columns
    u32 m_wy;              // 0x0C  height in rows
    int m_max_cx;          // 0x10  largest button cursor column
    int m_max_cy;          // 0x14  largest button cursor row
    const char* m_pTitle;  // 0x18  title
protected:
    int m_IsPushed;        // 0x1C  the decide button was pressed this frame
    int m_IsCanceled;      // 0x20  the cancel button was pressed this frame
    // 0x24 vptr

    void SetPushed() { m_IsPushed = 1; }
    void SetCanceled() { m_IsCanceled = 1; }

public:
    int IsPushed() { return m_IsPushed; }
    int IsCanceled() { return m_IsCanceled; }
    // decide / cancel flags from the pad, then LocalUpdate; the frame and title, then LocalDisp
    // (defined in dbg_tool.h)
    int Update();
    void Disp();

    virtual ~cDbgWindowBase() {}
    virtual int GetCx() { return 0; }
    virtual int GetCy() { return 0; }
    virtual void SetCurrentBottomButton() {}
    virtual void ButtonAllUpdate() = 0;
    virtual int LocalUpdate() = 0;
    virtual void LocalDisp() = 0;
};

class cDbgButton : public cDbgButtonBase {
public:
    void (*m_pFuncExec)(cDbgButton*);    // 0x1C  pushed
    void (*m_pFuncUpdate)(cDbgButton*);  // 0x20  called every frame by ButtonAllUpdate

    // inlined into cDbgWindow::AddButton; the tool modules are compiled with -fno-implement-inlines,
    // so no out-of-line body exists although cDbgButton's vtable is emitted there
    cDbgButton(int x_, int y_, const char* name, int cx_, int cy_) { Init(x_, y_, name, cx_, cy_); }
    virtual ~cDbgButton() {}
};

class cDbgWindow : public cDbgWindowBase {
public:
    u32 m_nBut;                   // 0x28
    cDbgButton* m_pButList[128];  // 0x2C
    cDbgButton* m_pCurrentBut;          // 0x22C
    cDbgButton* m_pStartBut;          // 0x230
    cDbgButton* m_pEndBut;       // 0x234

    // Init: declared here for every user (both t_esp_area/t_lightarea and t_event call it out of
    // line, `bl Init__10cDbgWindowiiPCc`), but db_toolbase.cpp emits it as an in-class inline
    // between the deferred ~cDbgButton and ~cDbgWindow (saved_inlines order, class order); no single
    // source form gives our cc1plus both, so the body is in-class only for the implementing unit.
#ifndef DB_TOOLBASE_IMPLEMENTATION
    void Init(int wx, int wy, const char* name);
#else
    void Init(int wx, int wy, const char* name)
    {
        m_px = wx;
        m_py = wy;
        m_wx = strlen(name);
        m_wy = 1;
        m_max_cx = 1;
        m_max_cy = 1;
        m_pTitle = name;
        m_IsPushed = 0;
        m_IsCanceled = 0;
        // COMPILER-DIFF: #13 -- the original stores the REG_EQUIV zero as a constant: the zero's
        // `li` is not in its sched1 (reload re-creates it after `li 1`) and the last zero store
        // carries no death. The two dead loop notes split our sched1 region so that `li 1` outranks
        // `li 0` (3 vs 2 dependents) and the last store stays last (13 -> 0 words).
        do {
        } while (0);
        m_nBut = 0;
        m_pCurrentBut = 0;
        m_pEndBut = 0;
        do {
        } while (0);
        m_pStartBut = 0;
    }
#endif
    // the virtuals with bodies here are what the derived windows of dbg_tool.h inline (their
    // synthesized destructors carry ~cDbgWindow's loop); LocalUpdate is the key function
    virtual ~cDbgWindow()
    {
        u32 i;

        for (i = 0; i < m_nBut; i++) {
            if (m_pButList[i]) {
                delete m_pButList[i];
            }
        }
    }
    virtual int GetCx()
    {
        if (m_pCurrentBut == 0) {
            return 0;
        }
        return m_pCurrentBut->GetCx();
    }
    virtual int GetCy()
    {
        if (m_pCurrentBut == 0) {
            return 0;
        }
        return m_pCurrentBut->GetCy();
    }
    virtual void SetCurrentTopButton() { m_pCurrentBut = m_pStartBut; }
    virtual void SetCurrentBottomButton() { m_pCurrentBut = m_pEndBut; }
    virtual void ButtonAllUpdate()
    {
        u32 i;

        for (i = 0; i < m_nBut; i++) {
            cDbgButton* b = m_pButList[i];

            if (b && b->m_pFuncUpdate) {
                b->m_pFuncUpdate(b);
            }
        }
    }
    virtual int LocalUpdate();
    virtual void LocalDisp();

    void AddButton(int x, int y, const char* name, int cx, int cy, void (*func)(cDbgButton*),
                   void (*update)(cDbgButton*));
    int FindButton(int cx, int cy, cDbgButton** out);
    // pad repeat moves the cursor over the button grid with wrap; a decide press runs the current button
    // (defined in dbg_tool.h)
    void CursorMove();
    void ButtonPushCheck();
};

#endif
