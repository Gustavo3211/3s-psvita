#ifndef PSP_AFS_H
#define PSP_AFS_H

#include <pspiofilemgr.h>
#include "types.h"

#define AFS_MAX_ENTRIES 1536

typedef struct {
    u32 offset;
    u32 size;
} AFSEntry;

typedef struct {
    SceUID fd;
    u32 entry_count;
    AFSEntry entries[AFS_MAX_ENTRIES];
} AFS;

// Initialize AFS from file path (call from main thread)
s32 afsInit(const char* path);
s32 afsIsReady(void);
void afsClose(void);

// Get file size from AFS index (instant — cached from header)
u32 afsGetFileSize(u16 fnum);

// Get sector count for a file
u32 afsGetSectorCount(u16 fnum);

// Read file data synchronously (for immediate loads)
s32 afsReadSync(u16 fnum, void* buf, u32 size);

// Async: start a read (kicks I/O thread)
s32 afsReadAsync(u16 fnum, void* buf, u32 size);

// Async: check if read is done (0=reading, 1=done, 2=error)
s32 afsCheckRead(void);

#endif
