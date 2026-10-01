#ifndef BANK_OFFLINE_PATCH_FS_HELPERS_H
#define BANK_OFFLINE_PATCH_FS_HELPERS_H

#include "patch_types.h"

/* Common 3DS filesystem types, entry points, and checked SD helpers. */
/* 共用的 3DS 文件系统类型、入口地址和带检查的 SD 辅助函数。 */
enum FsProtocolConstant {
    FS_ARCHIVE_ID_SDMC = 9,
    FS_PATH_TYPE_EMPTY = 1,
    FS_PATH_TYPE_ASCII = 3,
    OPEN_READ = 1,
    OPEN_WRITE = 2,
    OPEN_CREATE = 4,
    WRITE_FLUSH = 1,
    RESULT_SUMMARY_SHIFT = 21,
    RESULT_SUMMARY_MASK = 0x3F,
    RESULT_SUMMARY_NOT_FOUND = 4
};

typedef s32 (*FSUSER_OpenFileDirectlyFn)(volatile u32 *,u32 *,u32,u32,u32,const void *,u32,
    u32,const void *,u32,u32,u32);
typedef s32 (*FSFILE_ReadFn)(u32 *,u32 *,u64,void *,u32);
typedef s32 (*FSFILE_WriteFn)(u32 *,u32 *,u64,const void *,u32,u32);
typedef s32 (*FSFILE_CloseFn)(u32 *);
typedef s32 (*FSFILE_GetSizeFn)(u32 *,u64 *);

#define FSUSER_HandleSlot ((volatile u32 *)0x00390100u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x0022D338u)
#define FSFILE_Read ((FSFILE_ReadFn)0x001658C8u)
#define FSFILE_Close ((FSFILE_CloseFn)0x00165920u)
#define FSFILE_Write ((FSFILE_WriteFn)0x0016594Cu)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001659ACu)
#define FSUSER_CMD_OPEN_ARCHIVE 0x080C00C2u
#define FSUSER_CMD_DELETE_FILE 0x08040142u
#define FSUSER_CMD_RENAME_FILE 0x08050244u
#define FSUSER_CMD_CREATE_DIRECTORY 0x08090182u
#define FSUSER_CMD_CLOSE_ARCHIVE 0x080E0080u
#define FSFILE_CMD_SET_SIZE 0x08050080u

s32 openArchive(u64 *archive);
void closeArchive(u64 archive);
s32 pathCommand(u32 command,u64 archive,const char *path,u32 pathSize);
s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize);
s32 setSize(u32 handle,u64 size);
s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle);
int resultIsNotFound(s32 result);
s32 deleteFile(const char *path,u32 pathSize);
s32 readCompleteFile(const char *path,u32 pathSize,u32 expectedFileSize,u64 offset,
    void *data,u32 dataSize);
int writeCompleteFile(const char *path,u32 pathSize,const void *data,u32 dataSize,u32 fileSize);

#endif
