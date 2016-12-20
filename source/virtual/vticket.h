#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVTicketDrive(void);
u32 CheckVTicketDrive(void);

bool ReadVTicketDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVTicketFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
// int WriteVTicketFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count); // no writing
