#include "bankdata_redirect.h"
#include "fs_system/storage_notice.h"

static void show(void *view,uint32_t issue)
{
    ((void (*)(u32,u32,s32))0x0019B6B0u)(0x00050011u,0,-1);
    MoverUi_ShowMessageLine(view,0,77+issue-1);
}

static int confirmed(void *view)
{
    return MessageView_GetResult(*(void **)((uint8_t *)view+0x5c))==4;
}

int MoverStorageNotice_Poll(void *state)
{
    void *view=*(void **)((uint8_t *)state+0x34);
    return FsStorageNotice_Poll((uintptr_t)state,view,show,confirmed);
}
