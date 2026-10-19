#ifndef COMMON_FS_STORAGE_NOTICE_H
#define COMMON_FS_STORAGE_NOTICE_H

#include <stdint.h>

enum FsStorageIssue {
    FS_STORAGE_ISSUE_NONE,
    FS_STORAGE_ISSUE_IO,
    FS_STORAGE_ISSUE_FULL,
    FS_STORAGE_ISSUE_CAPACITY
};

/* Request-local classification is copied into the asynchronous reply. Only
   an actually delivered failure may request a UI acknowledgement. */
/* 请求内的分类复制到异步响应；只有实际交付的失败才请求界面确认。 */
void FsStorageNotice_BeginRequest(void);
void FsStorageNotice_Mark(uint32_t issue);
int FsStorageNotice_CheckIo(int success);
uint32_t FsStorageNotice_RequestIssue(int result);
void FsStorageNotice_Report(int result,uint32_t issue);
typedef void (*FsStorageNoticeShow)(void *view,uint32_t issue);
typedef int (*FsStorageNoticeConfirmed)(void *view);
int FsStorageNotice_Poll(uintptr_t peer,void *view,FsStorageNoticeShow show,
    FsStorageNoticeConfirmed confirmed);

#endif
