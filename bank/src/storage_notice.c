#include "bankdata_redirect.h"
#include "unlock_mode.h"
#include "fs_system/storage_notice.h"

static void show(void *view,uint32_t issue)
{
    WaitingSound_Stop(UNLOCK_WAITING_SOUND_ID,0,-1);
    BankUi_ShowMessageLine(view,0,132+issue-1,1);
}

static int confirmed(void *view)
{
    return MessageView_GetResult(((UnlockMessageView *)view)->messageView)==UNLOCK_MESSAGE_CONFIRMED;
}

int BankStorageNotice_Poll(void *state)
{
    void *view=*(void **)((uint8_t *)state+0x34);
    return FsStorageNotice_Poll((uintptr_t)state,view,show,confirmed);
}
