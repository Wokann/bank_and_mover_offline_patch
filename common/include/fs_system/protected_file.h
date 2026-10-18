#ifndef COMMON_FS_SYSTEM_PROTECTED_FILE_H
#define COMMON_FS_SYSTEM_PROTECTED_FILE_H

#include "fakeServer/types.h"

enum FsFileBackupPolicy { FS_FILE_KEEP_BACKUP, FS_FILE_RETIRE_AFTER_ACK };

/* Format-independent file publication, not a transaction coordinator.
   The caller validates data and decides whether a published backup is usable.
   Missing main may read backup; I/O errors and invalid main never silently fall back.
   Temporary files are never read as published data. */
/* 只管理文件发布，不决定事务。业务层校验内容、决定备份可用性；仅主文件缺失时
   读取备份，不吞掉 I/O 错误，不从 tmp 接纳未提交数据。 */
int FsFile_BackupPath(const char *path,char *out,uint32_t capacity);
int FsFile_OpenPublished(const FsStorageIo *,const char *path,const char *backup,
    uint32_t *handle,uint32_t *size,int *fromBackup);
/* The temporary must already be flushed, closed and verified. Retains backup
   until an explicit completion acknowledgement; never deletes the old main. */
/* tmp 须先写齐、刷新、关闭并验证；原主文件改名为备份，不直接删除。 */
int FsFile_PublishStaged(const FsStorageIo *,const char *path,const char *temp,
    const char *backup,int mainPresent);
/* Call only after business-level completion, never after a mere write callback.
   KEEP performs no I/O; RETIRE requires acknowledgement and an existing main. */
/* 仅业务确认完成后调用；保留型不产生 I/O，清理型须明确确认且主文件存在。 */
int FsFile_Complete(const FsStorageIo *,const char *path,const char *temp,
    const char *backup,uint32_t policy,int acknowledged);

/* Text decoding/schema validation stays in the configuration layer. Publication
   skips identical primary bytes and verifies staging once before rotating. */
/* 文本解析／修复仍由配置层负责；内容相同不写盘，暂存回读一次后再轮替。 */
int FsText_Read(const FsStorageIo *,const char *,char **,uint32_t *);
int FsText_Publish(const FsStorageIo *,const char *,const char *,const char *,uint32_t);

#endif
