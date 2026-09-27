// game/file_app (D:/Bio4/Prog/file_app.cpp): development-host file helpers over the SN ProDG PC file
// server, with ".lock" files so developers do not overwrite each other's room data. Unused in retail.
#include "types.h"
#include "file.h"
#include "main_mem.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include <string.h>
#include <dolphin/os.h>
#include "main.h"


// The original object carries 16 zero bytes of .sdata that no function references: four
// zero-initialised statics (the names are lost) kept alive by an inline function this build never
// calls. GCC 2.95 emits a static as soon as an inline body mentions it.
static int hd_stat0 = 0;
static int hd_stat1 = 0;
static int hd_stat2 = 0;
static int hd_stat3 = 0;

int hdRead_malloc(const char* path, void** buf, int mode, int flag);
int hdWrite_main(const char* path, void* buf, int size);
void createBackupFile(const char* path);

// Reads the whole host file into buf; returns the byte count (0 = not found / empty).
int HDRead(const char* fname, void* addr)
{
    int fd;
    int size;
    int ret = 0;

    OSReport("HDread : \"%s\"  ", fname);
    fd = file_open(fname, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    size = file_seek(fd, 0, 2);
    if (size != 0) {
        file_seek(fd, 0, 0);
        ret = file_read(fd, addr, size);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

// Reads len bytes from offset ofs of a host file into buf; returns the byte count.
int HDReadSeekLen(const char* fname, void* addr, u32 seeksize, int len)
{
    int fd;
    int ret = 0;

    OSReport("HDread : \"%s\"  ", fname);
    fd = file_open(fname, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    if ((u32) file_seek(fd, 0, 2) > seeksize) {
        file_seek(fd, seeksize, 0);
        ret = file_read(fd, addr, len);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

// Reads a host file into a new game-heap block (group 13); *buf receives it. Returns the size.
int HDReadMemAlloc(const char* fname, void** addr)
{
    return hdRead_malloc(fname, addr, 0, 0);
}

// Reads a host file into a new debug-heap block (Debug_alloc flag). Returns the size.
int HDReadDebugAlloc(const char* fname, void** addr, int release_flag)
{
    return hdRead_malloc(fname, addr, 1, release_flag);
}

// Shared body: opens, measures, allocates (mode 0 game heap, 1 debug heap), reads and closes.
int hdRead_malloc(const char* fname, void** addr, int malloc_mode, int release_flag)
{
    int fd;
    int size;
    int ret = 0;

    *addr = NULL;
    OSReport("HDread : \"%s\"  ", fname);
    fd = file_open(fname, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    size = file_seek(fd, 0, 2);
    if (size != 0) {
        switch (malloc_mode) {
        case 0:
#line 165 "D:/Bio4/Prog/file_app.cpp"
            *addr = MEM_ALLOC(size, 1, 13);
            break;
        case 1:
            *addr = Debug_alloc(size, release_flag);
            break;
        default:
            return 0;
        }
        if (*addr == NULL) {
            OSReport("malloc failed!!!\n");
            file_close(fd);
            return 0;
        }
        file_seek(fd, 0, 0);
        ret = file_read(fd, *addr, size);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

// Writes a host file after checking the lock (refuses when another user holds it and the user
// declines to overwrite) and saving the old contents as path.bak.
int HDWrite(const char* fname, void* addr, int size)
{
    if (file_lock_check(fname) == 1) {
        OSReport("not write...\n");
        return 0;
    }
    createBackupFile(fname);
    return hdWrite_main(fname, addr, size);
}

// Writes a host file without lock check or backup.
int HDWrite_only(const char* fname, void* addr, int size)
{
    return hdWrite_main(fname, addr, size);
}

// Shared body: creates (size 0 -> truncates) or rewrites the file. Returns bytes written.
int hdWrite_main(const char* fname, void* addr, int size)
{
    int fd;
    int ret;

    OSReport("HDwrite: \"%s\"  ", fname);
    ret = 0;
    if (size == 0) {
        fd = file_open(fname, FILE_OPEN_WRITE);
        if (fd == 0) {
            OSReport("write failed!\n");
            return 0;
        }
        OSReport("write complete...\n");
    } else {
        fd = file_open(fname, FILE_OPEN_RDWR);
        if (fd == 0) {
            OSReport("write failed!\n");
            return 0;
        }
        ret = file_write(fd, addr, size);
        OSReport("write complete...\n");
    }
    file_close(fd);
    return ret;
}

// Copies the current contents of path to path.bak (debug heap temporaries).
void createBackupFile(const char* fname)
{
    void* data;
    int size;
    char* bak;
    int fd;

    size = HDReadDebugAlloc(fname, &data, 1);
    if (size == 0) {
        return;
    }
    bak = (char*) Debug_alloc(strlen(fname) + 5, 1);
    strcpy(bak, fname);
    strcat(bak, ".bak");
    OSReport("HDwrite: \"%s\"  ", bak);
    fd = file_open(bak, FILE_OPEN_RDWR);
    if (fd == 0) {
        Debug_free(data);
        Debug_free(bak);
        OSReport("backup failed!\n");
        return;
    }
    file_write(fd, data, size);
    Debug_free(data);
    Debug_free(bak);
    OSReport("backup complete...\n");
    file_close(fd);
}

// 1 when the file's .lock exists, names another user and that user declined the overwrite prompt.
int file_lock_check(const char* name)
{
    char lock[64];
    char* user;

    strcpy(lock, name);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) != 0) {
            if (strcmp(user, pUser_name) != 0) {
                if (file_lock_msg(1, name, user) == 1) {
                    Debug_free(user);
                    return 1;
                }
            }
            Debug_free(user);
        }
    }
    return 0;
}

// Takes the lock for the current user (writes pUser_name into the .lock), asking before taking it
// over from another user; returns 1 when held.
int file_lock(const char* name)
{
    char lock[64];
    char* user;

    strcpy(lock, name);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    // `pUser_name` read directly in each arm: a `path = pUser_name` reassignment gives the
    // pseudo a second set, which global-allocates it with a phantom r31 (`stmw r30` vs `stw r31`).
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) == 0) {
            HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
        } else {
            if (strcmp(user, pUser_name) != 0) {
                if (file_lock_msg(0, name, user) != 0) {
                    Debug_free(user);
                    return 0;
                }
                HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
            }
            Debug_free(user);
        }
    } else {
        HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
    }
    return 1;
}

// Releases the lock when it is held by the current user (truncates the .lock). Returns 1 on success.
int file_unlock(const char* name)
{
    char lock[64];
    char* user;

    strcpy(lock, name);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) != 0) {
            if (strcmp(user, pUser_name) == 0) {
                HDWrite_only(lock, user, 0);
                Debug_free(user);
                return 1;
            }
            Debug_free(user);
        }
    }
    return 0;
}

// Turns a room data path into its lock file name: the "room"/"Room"/"ROOM" directory component
// becomes "lock" and ".lock" is appended (in place). NULL when the path has no room component.
char* get_lock_file(char* name)
{
    char* p;

    p = strstr(name, "room");
    if (p == NULL) {
        p = strstr(name, "Room");
        if (p == NULL) {
            p = strstr(name, "ROOM");
        }
    }
    if (p == NULL) {
        return NULL;
    }
    strncpy(p, "lock", 4);
    strcat(name, ".lock");
    return name;
}

// Blocking on-screen YES/NO prompt (mode 0 "UNLOCKED?", 1 "OVER WRITE?") driven by the pad; returns
// the cursor (0 yes, 1 no).
int file_lock_msg(int msg_no, const char* name, const char* id)
{
    int cur = 1;
    int n;
    int y = 80;

    while (1) {
        switch (msg_no) {
        case 0:
            eprintf2(10, 20, 30, 100, 6, 0, "(%s)", name);
            y = 160;
            eprintf2(10, 20, 30, 120, 22, 0, " is locked by the other user [%s].", id);
            eprintf2(10, 20, 30, 160, 2, 0, " UNLOCKED?");
            break;
        case 1:
            eprintf2(10, 20, 30, 100, 6, 0, "(%s)", name);
            y = 160;
            eprintf2(10, 20, 30, 120, 22, 0, " is locked by the other user [%s].", id);
            eprintf2(10, 20, 30, 160, 2, 0, " OVER WRITE?");
            break;
        }
        y += 20;
        eprintf2(10, 20, 30, y, 0, 0, "   YES");
        y += 20;
        eprintf2(10, 20, 30, y, 0, 0, "   NO");
        if (Joy[0].trg & JOY_UP) {
            cur--;
        }
        if (Joy[0].trg & JOY_DOWN) {
            cur++;
        }
        if (cur >= 0) {
            n = cur;
            if (n > 1) {
                n = 1;
            }
        } else {
            n = 0;
        }
        cur = n;
        if (n == 0) {
            eprintf2(10, 20, 30, y - 20, 0, 0, "  >");
        } else {
            eprintf2(10, 20, 30, y, 0, 0, "  >");
        }
        TaskSleep(1);
        if (Joy[0].trg & JOY_A) {
            break;
        }
    }
    Joy[0].trg = 0;
    return n;
}
