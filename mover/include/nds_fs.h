#ifndef MOVER_OFFLINE_PATCH_NDS_FS_H
#define MOVER_OFFLINE_PATCH_NDS_FS_H

#include "fs_helpers.h"

/* UTF-16 SD paths are passed with a byte count including the terminator. */
/* UTF-16 SD 路径的长度以字节计，包含末尾零字符。 */
s32 openFileUtf16(const u16 *path,u32 pathSize,u32 flags,u32 *handle);
s32 openDirectoryUtf16(const u16 *path,u32 pathSize,u64 *archive,u32 *handle);
s32 readDirectoryEntry(u32 handle,FsDirectoryEntry *entry,u32 *entriesRead);
s32 closeDirectory(u32 handle);
s32 readFileRangeUtf16(const u16 *path,u32 pathSize,u64 offset,void *data,u32 dataSize);
s32 writeFileRangeUtf16(const u16 *path,u32 pathSize,u64 offset,const void *data,u32 dataSize);
int fileIsReadWriteUtf16(const u16 *path,u32 pathSize);

#endif
