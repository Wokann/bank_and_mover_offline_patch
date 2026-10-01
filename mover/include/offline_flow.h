#ifndef MOVER_OFFLINE_PATCH_OFFLINE_FLOW_H
#define MOVER_OFFLINE_PATCH_OFFLINE_FLOW_H

#include "mover_state.h"
#include "patch_types.h"

enum MoverOfflineFlowConstant {
    MOVER_NETWORK_DISPLAY_DELAY_MS = 1500,
    MOVER_LOCAL_SAVE_DISPLAY_DELAY_MS = 2000
};

/* Stock timer entry points used by local state updates. */
/* 本地状态更新使用的原版计时器入口。 */
typedef int (*StateTimer_HasElapsedFn)(void *,u32);
typedef void (*StateTimer_ResetFn)(void *);

#define StateTimer_HasElapsed ((StateTimer_HasElapsedFn)0x0019B758u)
#define StateTimer_Reset ((StateTimer_ResetFn)0x00233844u)

int OfflinePatch_NetworkUpdate(MoverStateView *state);
int OfflinePatch_DisconnectUpdate(MoverStateView *state);
int OfflinePatch_RemoteCheckUpdate(MoverStateView *state);
int OfflinePatch_NoTransferUpdate(MoverStateView *state);
int OfflinePatch_SaveDisplayDelayUpdate(MoverStateView *state);

#endif
