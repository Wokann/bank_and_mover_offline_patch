#ifndef COMMON_OFFLINE_PATCH_FS_HELPERS_H
#define COMMON_OFFLINE_PATCH_FS_HELPERS_H

#include "patch_types.h"

/* Shared filesystem ABI and operations; SDK addresses stay title-specific. */
/* 共用文件系统 ABI 和操作；SDK 地址由各软件自己定义。 */
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
typedef s32 (*SRV_GetServiceHandleFn)(u32 *,const char *,s32,u32);

#include "fs_sdk.h"
#define FSUSER_CMD_OPEN_ARCHIVE 0x080C00C2u
#define FSUSER_CMD_DELETE_FILE 0x08040142u
#define FSUSER_CMD_RENAME_FILE 0x08050244u
#define FSUSER_CMD_OPEN_DIRECTORY 0x080B0102u
#define FSUSER_CMD_CREATE_DIRECTORY 0x08090182u
#define FSUSER_CMD_CLOSE_ARCHIVE 0x080E0080u
#define FSUSER_CMD_GET_FREE_BYTES 0x08120080u
#define FSFILE_CMD_SET_SIZE 0x08050080u
#define FSDIR_CMD_READ 0x08010042u
#define FSDIR_CMD_CLOSE 0x08020000u

/* ARM-only TLS/SVC primitives also support Thumb callers by literal address. */
/* TLS／SVC 原语保留 ARM 状态，Thumb 调用方以地址间接调用。 */
volatile u32 *fsCommandBuffer(void) __attribute__((target("arm"),long_call,noinline));
s32 fsSync(u32 handle) __attribute__((target("arm"),long_call,noinline));
s32 fsCloseHandle(u32 handle) __attribute__((target("arm"),long_call,noinline));
s32 closeFile(u32 handle);

/* Exact directory-entry ABI returned by FSUSER. */
/* FSUSER 返回的精确目录项 ABI。 */
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

typedef struct FsPathView {
    u32 type;
    u32 size;
    const void *data;
} FsPathView;

s32 openArchive(u64 *archive);
s32 closeArchive(u64 archive);
s32 pathCommand(u32 command,u64 archive,const char *path,u32 pathSize);
s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize);
s32 setSize(u32 handle,u64 size);
s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle);
int resultIsNotFound(s32 result);

#endif
