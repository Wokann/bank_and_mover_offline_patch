#ifndef BANK_OFFLINE_PATCH_OFFLINE_FLOW_H
#define BANK_OFFLINE_PATCH_OFFLINE_FLOW_H

#include "bankdata_redirect.h"
#include "patch_types.h"

enum BankOfflineFlowConstant {
    BANK_NETWORK_DISPLAY_DELAY_MS = 1500,
    BANK_LOCAL_DISPLAY_DELAY_MS = 2000,
    BANK_SAVE_LOCAL_COMPLETE_SUBSTATE = 3
};

/* Stock timer entry points used by local connection and save-display states. */
/* 本地连接与保存提示状态所使用的原版计时器入口。 */
typedef int (*StateTimer_HasElapsedFn)(void *,u32);
typedef void (*StateTimer_ResetFn)(void *);

#define StateTimer_HasElapsed ((StateTimer_HasElapsedFn)0x001D5BB0u)
#define StateTimer_Reset ((StateTimer_ResetFn)0x00229DB4u)

int OfflinePatch_NetworkUpdate(BankStateView *state);
int OfflinePatch_PostSelectionConnectionUpdate(BankStateView *state);
int OfflinePatch_DisconnectUpdate(BankStateView *state);
int OfflinePatch_SaveDisplayDelayUpdate(BankStateView *state);

#endif
