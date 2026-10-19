#include "fs_system/storage_notice.h"

static struct {
    uint32_t requestIssue,noticeIssue;
    uintptr_t peer;
} storageNotice __attribute__((section(".data.storage_notice")))={0};

void FsStorageNotice_BeginRequest(void) { storageNotice.requestIssue=0; }

void FsStorageNotice_Mark(uint32_t issue)
{
    if (issue && issue<=FS_STORAGE_ISSUE_CAPACITY &&
        (!storageNotice.requestIssue || issue!=FS_STORAGE_ISSUE_IO))
        storageNotice.requestIssue=issue;
}

int FsStorageNotice_CheckIo(int success)
{
    if (!success) FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);
    return success;
}

uint32_t FsStorageNotice_RequestIssue(int result)
{
    return result<0?storageNotice.requestIssue:0;
}

void FsStorageNotice_Report(int result,uint32_t issue)
{
    if (result<0 && issue && issue<=FS_STORAGE_ISSUE_CAPACITY && !storageNotice.noticeIssue)
        storageNotice.noticeIssue=issue;
}

int FsStorageNotice_Poll(uintptr_t peer,void *view,FsStorageNoticeShow show,
    FsStorageNoticeConfirmed confirmed)
{
    if (!storageNotice.noticeIssue || !peer || !view) return 1;
    if (storageNotice.peer!=peer) {
        storageNotice.peer=peer;
        show(view,storageNotice.noticeIssue);
        return 0;
    }
    if (!confirmed(view)) return 0;
    storageNotice.noticeIssue=0;storageNotice.peer=0;
    return 1;
}
