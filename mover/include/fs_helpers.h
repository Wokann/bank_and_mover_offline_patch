#ifndef MOVER_OFFLINE_PATCH_FS_HELPERS_H
#define MOVER_OFFLINE_PATCH_FS_HELPERS_H

#include "patch_types.h"

/* Common 3DS filesystem types, entry points, and checked SD helpers. */
/* 共用的 3DS 文件系统类型、入口地址和带检查的 SD 辅助函数。 */
enum FsProtocolConstant {
    FS_ARCHIVE_ID_SDMC = 9,
    FS_PATH_TYPE_EMPTY = 1,
    FS_PATH_TYPE_ASCII = 3,
    FS_PATH_TYPE_UTF16 = 4,
    OPEN_READ = 1,
    OPEN_WRITE = 2,
    OPEN_CREATE = 4,
    WRITE_FLUSH = 1,
    FS_ATTRIBUTE_DIRECTORY = 1,
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

#define FSUSER_HandleSlot ((volatile u32 *)0x00311F80u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x001DF448u)
#define FSFILE_Read ((FSFILE_ReadFn)0x0015930Cu)
#define FSFILE_Close ((FSFILE_CloseFn)0x00159364u)
#define FSFILE_Write ((FSFILE_WriteFn)0x00159390u)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001593F0u)
#define FSUSER_CMD_OPEN_ARCHIVE 0x080C00C2u
#define FSUSER_CMD_CREATE_DIRECTORY 0x08090182u
#define FSUSER_CMD_DELETE_FILE 0x08040142u
#define FSUSER_CMD_RENAME_FILE 0x08050244u
#define FSUSER_CMD_OPEN_DIRECTORY 0x080B0102u
#define FSUSER_CMD_CLOSE_ARCHIVE 0x080E0080u
#define FSFILE_CMD_SET_SIZE 0x08050080u
#define FSDIR_CMD_READ 0x08010042u
#define FSDIR_CMD_CLOSE 0x08020000u

/* Exact ABI returned by FSUSER directory enumeration. */
/* FSUSER 目录枚举所返回的精确 ABI 布局。 */
typedef struct FsDirectoryEntry {
    u16 name[0x106];
    char shortName[0x0A];
    char shortExtension[0x04];
    u8 valid;
    u8 reserved;
    u32 attributes;
    u32 fileSizeLow;
    u32 fileSizeHigh;
} FsDirectoryEntry;

/* Keep TLS/SVC primitives in ARM state. Literal-address calls avoid the
   importer's unsupported immediate Thumb-to-ARM relocation. */
/* TLS／SVC 原语保留 ARM 状态；地址间接调用避开导入器不支持的立即数跨状态重定位。 */
volatile u32 *fsCommandBuffer(void) __attribute__((target("arm"),long_call,noinline));
s32 fsSync(u32 handle) __attribute__((target("arm"),long_call,noinline));
s32 fsCloseHandle(u32 handle) __attribute__((target("arm"),long_call,noinline));
s32 closeFile(u32 handle);

s32 openArchive(u64 *archive);
s32 closeArchive(u64 archive);
s32 pathCommand(u32 command,u64 archive,const char *path,u32 pathSize);
s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize);
s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle);
int resultIsNotFound(s32 result);
s32 readCompleteFile(const char *path,u32 pathSize,u32 expectedFileSize,u64 offset,
    void *data,u32 dataSize);
int writeCompleteFile(const char *path,u32 pathSize,const void *data,u32 dataSize,u32 fileSize);

#endif
