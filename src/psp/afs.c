#include "psp/afs.h"
#include <string.h>

#define AFS_MAGIC 0x00534641  // "AFS\0" little-endian
#define SECTOR_SIZE 2048

static AFS afs;
static s32 afs_ready = 0;

s32 afsInit(const char* path) {
    u32 magic;

    memset(&afs, 0, sizeof(afs));
    afs.fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (afs.fd < 0) {
        return 0;
    }

    sceIoRead(afs.fd, &magic, 4);
    if (magic != AFS_MAGIC) {
        sceIoClose(afs.fd);
        afs.fd = -1;
        return 0;
    }

    sceIoRead(afs.fd, &afs.entry_count, 4);
    if (afs.entry_count > AFS_MAX_ENTRIES) {
        afs.entry_count = AFS_MAX_ENTRIES;
    }

    sceIoRead(afs.fd, afs.entries, afs.entry_count * sizeof(AFSEntry));

    afs_ready = 1;
    return 1;
}

s32 afsIsReady(void) {
    return afs_ready;
}

void afsClose(void) {
    if (afs.fd >= 0) {
        sceIoClose(afs.fd);
        afs.fd = -1;
    }
    afs_ready = 0;
}

u32 afsGetFileSize(u16 fnum) {
    if (fnum >= afs.entry_count) return 0;
    return afs.entries[fnum].size;
}

u32 afsGetSectorCount(u16 fnum) {
    u32 size = afsGetFileSize(fnum);
    return (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

// All reads are synchronous — seek+read within one open fd
// On PPSSPP this is instant. On real PSP hardware this would
// benefit from async, but AFS sequential access is already fast.
s32 afsReadSync(u16 fnum, void* buf, u32 size) {
    if (fnum >= afs.entry_count || afs.fd < 0) return 0;
    if (afs.entries[fnum].size == 0) return 0;

    u32 read_size = size;
    if (read_size > afs.entries[fnum].size) {
        read_size = afs.entries[fnum].size;
    }

    sceIoLseek32(afs.fd, afs.entries[fnum].offset, PSP_SEEK_SET);
    s32 read = sceIoRead(afs.fd, buf, read_size);
    return (read >= 0) ? 1 : 0;
}

// "Async" read — just does sync since PPSSPP I/O is instant
s32 afsReadAsync(u16 fnum, void* buf, u32 size) {
    return afsReadSync(fnum, buf, size);
}

// Always done — reads are synchronous
s32 afsCheckRead(void) {
    return 1;
}
