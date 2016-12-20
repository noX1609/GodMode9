#pragma once

#include "common.h"
#include "fsdir.h"
#include "nand.h"

#define VRT_SYSNAND NAND_SYSNAND
#define VRT_EMUNAND NAND_EMUNAND
#define VRT_IMGNAND NAND_IMGNAND
#define VRT_XORPAD  NAND_ZERONAND
#define VRT_MEMORY  (1<<10)
#define VRT_GAME    (1<<11)
#define VRT_TICKDB  (1<<12)

#define VRT_SOURCE  (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD|VRT_MEMORY|VRT_GAME|VRT_TICKDB)

#define VFLAG_DIR       (1<<16)
#define VFLAG_ROOT      (1<<17)
#define VFLAG_A9LH_AREA (1<<18)
#define VFLAG_LV3       (1<<19)

#define VRT_DRIVES  {'S', VRT_SYSNAND}, {'E', VRT_EMUNAND}, {'I', VRT_IMGNAND}, {'X', VRT_XORPAD }, \
                    {'M', VRT_MEMORY}, {'G', VRT_GAME}, {'T', VRT_TICKDB}

// virtual file flag (subject to change):
// bits 0...9  : reserved for NAND virtual sources and info
// bits 10...15: reserved for other virtual sources
// bits 16...23: reserved for external flags
// bits 24...31: reserved for internal flags (different per source)
typedef struct {
    char name[32];
    u64 offset; // must be a multiple of 0x200 (for NAND access)
    u64 size;
    u32 keyslot;
    u32 flags;
} __attribute__((packed)) VirtualFile;

// virtual dirs are only relevant for virtual game drives
typedef struct {
    int index;
    u64 offset;
    u64 size;
    u32 flags;
    u32 virtual_src;
} __attribute__((packed)) VirtualDir;

u32 GetVirtualSource(const char* path);
bool InitVirtualImageDrive(void);
bool CheckVirtualDrive(const char* path);

bool ReadVirtualDir(VirtualFile* vfile, VirtualDir* vdir);
bool OpenVirtualRoot(VirtualDir* vdir, u32 virtual_src);
bool OpenVirtualDir(VirtualDir* vdir, VirtualFile* ventry);

bool GetVirtualFile(VirtualFile* vfile, const char* path);
bool GetVirtualDir(VirtualDir* vdir, const char* path);
bool GetVirtualDirContents(DirStruct* contents, char* fpath, int fnsize, const char* pattern, bool recursive);

bool GetVirtualFilename(char* name, const VirtualFile* vfile, u32 n_chars);

int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count, u32* bytes_read);
int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written);
