#ifndef COMMON_OFFLINE_PATCH_FS_UTF16_H
#define COMMON_OFFLINE_PATCH_FS_UTF16_H

#include "fs_helpers.h"

/* UTF-16 SD paths are passed with a byte count including the terminator. */
/* UTF-16 SD 路径的长度以字节计，包含末尾零字符。 */
s32 openFileUtf16(const u16 *path,u32 pathSize,u32 flags,u32 *handle);
/* The caller owns any opened archive, even if opening the directory fails. */
/* 即使目录打开失败，已打开的归档仍由调用方负责关闭。 */
s32 openDirectoryUtf16(const u16 *path,u32 pathSize,u64 *archive,u32 *handle);
s32 readDirectoryEntry(u32 handle,FsDirectoryEntry *entry,u32 *entriesRead);
s32 closeDirectory(u32 handle);
s32 readFileRangeUtf16(const u16 *path,u32 pathSize,u64 offset,void *data,u32 dataSize);
int fileIsReadWriteUtf16(const u16 *path,u32 pathSize);

#endif
