#include "types.h"
#include "db_filelist.h"
#include "db_log.h"
#include "main_mem.h"
#include "joy.h"
#include "eprintf.h"
#include "file.h"
#include <string.h>

// Host file list of the debug tools (t_event / t_sce db_filelist.cpp, real name unknown): the same code
// sits inside Sscrn's ss_term.cpp. t_event's object is Tools' db_toolbase.cpp followed by this file.

// Host file list start: search pattern d (NULL: \bio4\data\*.*), prefix f stripped from every name;
// returns the first read's result.
int cFileList::init(const char* d, const char* f)
{
    text = 0;
    list = 0;
    cursor = 0;
    pattern = 0;
    filter = 0;
    dir(d, f);
    return update();
}

// Scrolling list display at (x, y) with `rows` visible lines: up/down (fast repeat) move the
// cursor, stick up/down half a page; returns the selected name.
char* cFileList::disp(int x, int y, int rows)
{
    JOY* joy = &Joy[0];
    int end;
    int i;

    if (joy->rep2 & 0x000C000C) {
        if (joy->rep2 & 4) {
            cursor++;
        }
        if (joy->rep2 & 8) {
            cursor--;
        }
        if (joy->rep2 & 0x40000) {
            cursor += rows / 2;
        }
        if (joy->rep2 & 0x80000) {
            cursor -= rows / 2;
        }
        cursor = cursor < 0 ? 0 : (cursor > num - 1 ? num - 1 : cursor);
    }
    if (top < cursor - rows + 1) {
        top = cursor - rows + 1;
    }
    if (top > cursor) {
        top = cursor;
    }
    if (top + rows > num) {
        end = num;
    } else {
        end = top + rows;
    }
    for (i = top; i < end; i++) {
        int col = 0;
        if (i == cursor) {
            col = 6;
        }
        eprintf(x, y, col, 0, "%s", list[i]);
        y += 16;
    }
    return list[cursor];
}

// Re-reads d:\bio4\room\filelist.txt from the host, converts the backslashes, splits the CRLF
// lines into `list` (stripping `filter` from each). 0 when the file is missing.
int cFileList::update()
{
    char* p;
    int i;

    if (text) {
        Debug_free(text);
    }
    if (list) {
        Debug_free(list);
    }
    HDReadDebugAlloc("d:\\bio4\\room\\filelist.txt", (void**) &text, 1);
    num = 0;
    if (text == 0) {
        pLog->err(0, 0, "cFileList::update : file not found");
        return 0;
    }
    p = text;
    while ((p = strchr(p, '\\')) != 0) {
        *p = '/';
    }
    p = text;
    num = 0;
    while ((p = strchr(p, '\r')) != 0) {
        *p = 0;
        p++;
        num++;
    }
    list = (char**) Debug_alloc(num * 4, 1);
    p = text;
    for (i = 0; i < num; i++) {
        if (filter) {
            p = strstr(p, filter);
            p += strlen(filter);
        }
        list[i] = p;
        p += strlen(p);
        p += 2;
    }
    return 1;
}

// Sets the search pattern `d` and name prefix `f` (copied, backslashes converted); d == 0 gives the
// defaults \bio4\data\*.* and /bio4/data/.
void cFileList::dir(const char* d, const char* f)
{
    if (pattern) {
        Debug_free(pattern);
    }
    if (filter) {
        Debug_free(filter);
    }
    if (d == 0) {
        char defDir[15] = "\\bio4\\data\\*.*";
        pattern = (char*) Debug_alloc(strlen(defDir), 1);
        strcpy(pattern, defDir);
        char defFilter[12] = "/bio4/data/";
        filter = (char*) Debug_alloc(strlen(defFilter), 1);
        strcpy(filter, defFilter);
    } else {
        pattern = (char*) Debug_alloc(strlen(d), 1);
        strcpy(pattern, d);
        if (f) {
            char* p;
            filter = (char*) Debug_alloc(strlen(f), 1);
            strcpy(filter, f);
            p = filter;
            while ((p = strchr(p, '\\')) != 0) {
                *p = '/';
            }
        } else {
            filter = (char*) f;
        }
    }
}

// global (t_event.cpp drives it); the name is not in the binary
cFileList DbgFileList;
