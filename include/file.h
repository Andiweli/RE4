#ifndef FILE_H
#define FILE_H

#include "types.h"

// Host (SN debugger) file access (game/file.cpp). Returns a handle (0 = failure).
#define FILE_OPEN_WRITE  0  // truncate/create
#define FILE_OPEN_READ   1
#define FILE_OPEN_RDWR   2  // create for writing

// game/file.cpp (enabled by pG->flags_54 & 0x20000).
int InitFile();
int file_open(const char* path, int mode);
int file_close(int hFile);   // 0 = ok, -1 = failed
int file_read(int hFile, void* addr, int len);
int file_write(int hFile, const void* addr, int len);
int file_seek(int hFile, int ofs, int mode);
int file_exist(const char* path);
int file_path(const char* name);   // "SETROOT:<dir>"

// game/file_app.cpp: whole-file helpers on top of the above.
int HDRead(const char* fname, void* addr);
int HDReadSeekLen(const char* fname, void* addr, u32 seeksize, int len);
int HDReadMemAlloc(const char* fname, void** addr);
int HDReadDebugAlloc(const char* fname, void** addr, int release_flag);
int HDWrite(const char* fname, void* addr, int size);
int HDWrite_only(const char* fname, void* addr, int size);
int file_lock_check(const char* name);
int file_lock(const char* name);
int file_unlock(const char* name);
char* get_lock_file(char* name);
int file_lock_msg(int msg_no, const char* name, const char* id);

#endif
