#ifndef BANK_OFFLINE_PATCH_UNLOCK_MODE_H
#define BANK_OFFLINE_PATCH_UNLOCK_MODE_H

#include "patch_types.h"

enum UnlockFlowConstant {
    UNLOCK_UPDATE_PENDING = 0,
    UNLOCK_UPDATE_COMPLETE = 1,
    UNLOCK_RESULT_DISCONNECT = 3,
    UNLOCK_RESULT_CONTINUE = 4,
    UNLOCK_RESULT_RESELECT_GAME = 19,
    UNLOCK_MESSAGE_CONFIRMED = 4,
    UNLOCK_EXIT_NOTICE_SUBSTATE = 10,
    UNLOCK_MISMATCH_NOTICE_SUBSTATE = 10,
    UNLOCK_RECOVERY_ERROR_SUBSTATE = 12,
    UNLOCK_NO_RECOVERY_MESSAGE = 118,
    UNLOCK_RECOVERED_MESSAGE = 119,
    UNLOCK_FORCED_RECOVERED_MESSAGE = 120,
    UNLOCK_RECOVERY_NONE = 0,
    UNLOCK_RECOVERY_FORCED = 1,
    UNLOCK_RECOVERY_AUTOMATIC = 2,
    UNLOCK_WAITING_SOUND_ID = 0x00050011,
    UNLOCK_WAITING_PANE_ID = 0x97
};

typedef struct UnlockMessageView {
    u8 reserved00[0x5C];
    void *messageView;
} UnlockMessageView;

/* Shared prefix of the native connection and transaction-recovery states. */
/* 原版连接与事务恢复状态共同使用的前缀布局。 */
typedef struct UnlockStateView {
    u8 reserved00[0x10];
    u32 substate;
    u8 reserved14[0x1C];
    u8 result;
    u8 reserved31[3];
    u32 callbackVtable;
    UnlockMessageView *view;
} UnlockStateView;

typedef int (*UnlockNativeUpdateFn)(UnlockStateView *);
typedef void (*BankUi_ShowMessageLineFn)(UnlockMessageView *,u32,u32,u32);
typedef int (*MessageView_GetResultFn)(void *);
typedef void (*WaitingSound_StopFn)(u32,u32,s32);
typedef void (*Ui_SetPaneVisibleFn)(void *,u32,u32,u32);
typedef void (*Ui_ApplyAnimationFn)(void *,u32,u32,u32);

#define BankUi_ShowMessageLine ((BankUi_ShowMessageLineFn)0x001D6650u)
#define MessageView_GetResult ((MessageView_GetResultFn)0x001D6600u)
#define WaitingSound_Stop ((WaitingSound_StopFn)0x001D6200u)
#define Ui_SetPaneVisible ((Ui_SetPaneVisibleFn)0x001E7084u)
#define Ui_ApplyAnimation ((Ui_ApplyAnimationFn)0x001E69ACu)
#define UnlockMode_RecoveryStorage ((volatile u8 *)0x003FAFF6u)

int UnlockMode_PostSelectionUpdate(UnlockStateView *state,UnlockNativeUpdateFn nativeUpdate);
int UnlockMode_RecoveryUpdate(UnlockStateView *state,UnlockNativeUpdateFn nativeUpdate);

/* Return the first server-provided unlock candidate in the same eight-digit
   form used by the stock validation callback. */
/* 按原版验证回调使用的八位格式返回服务器提供的第一个解锁候选值。 */
u32 UnlockMode_TryGetFirstCandidateCode(
    const u32 *candidateBegin, const u32 *candidateEnd, u32 *displayCode);

#endif
