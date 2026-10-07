#include "unlock_mode.h"

#define UNLOCK_CODE_MODULUS 100000000u
#define UNLOCK_CODE_RECIPROCAL 0x55E63B89u

__attribute__((used,noinline,section(".text.unlock.0_candidate")))
u32 UnlockMode_TryGetFirstCandidateCode(
    const u32 *candidateBegin, const u32 *candidateEnd, u32 *displayCode)
{
    if (candidateBegin == 0 || candidateEnd == 0 || candidateBegin >= candidateEnd ||
        displayCode == 0) {
        return 0;
    }

    /* The stock input callback accepts each returned value modulo 100,000,000.
       Its binary implements the constant division with reciprocal multiply,
       so use the same arithmetic and let the UI add leading zeroes. */
    /* 原版输入回调按模 100,000,000 验证每个返回值。其二进制通过倒数乘法实现
       常数除法，因此这里使用相同算法，前导零仍交给界面补齐。 */
    u32 candidate=*candidateBegin;
    u64 reciprocalProduct=(u64)candidate*UNLOCK_CODE_RECIPROCAL;
    u32 quotient=(u32)(reciprocalProduct>>32)>>25;
    *displayCode=candidate-quotient*UNLOCK_CODE_MODULUS;
    return 1;
}

__attribute__((used,noinline,section(".text.unlock.1_connection")))
int UnlockMode_PostSelectionUpdate(UnlockStateView *state,UnlockNativeUpdateFn nativeUpdate)
{
    if (state->substate == UNLOCK_EXIT_NOTICE_SUBSTATE) {
        if (MessageView_GetResult(state->view->messageView) != UNLOCK_MESSAGE_CONFIRMED) {
            return UNLOCK_UPDATE_PENDING;
        }
        state->result=UNLOCK_RESULT_DISCONNECT;
        return UNLOCK_UPDATE_COMPLETE;
    }

    /* Finish the native selected-game transaction before reporting either
       automatic recovery, forced unlock, or an already-normal server state. */
    /* 先完成原版所选游戏事务，再提示自动恢复、强制解锁或服务器原本正常。 */
    int result=nativeUpdate(state);
    if (result == UNLOCK_UPDATE_COMPLETE && state->result == UNLOCK_RESULT_CONTINUE) {
        u8 recovery=*UnlockMode_RecoveryStorage;
        u32 message=UNLOCK_NO_RECOVERY_MESSAGE;
        if (recovery == UNLOCK_RECOVERY_AUTOMATIC) {
            message=UNLOCK_RECOVERED_MESSAGE;
        } else if (recovery == UNLOCK_RECOVERY_FORCED) {
            message=UNLOCK_FORCED_RECOVERED_MESSAGE;
        }
        /* Use the native transition from the waiting view to an acknowledged
           message, including stopping its looping sound and loading pane. */
        /* 按原版方式从等待界面转为按键确认提示，同时停止循环音效并关闭加载窗格。 */
        WaitingSound_Stop(UNLOCK_WAITING_SOUND_ID,0,-1);
        Ui_SetPaneVisible(state->view->messageView,0,UNLOCK_WAITING_PANE_ID,0);
        Ui_ApplyAnimation(state->view->messageView,0,0,0);
        BankUi_ShowMessageLine(state->view,0,message,1);
        state->substate=UNLOCK_EXIT_NOTICE_SUBSTATE;
        return UNLOCK_UPDATE_PENDING;
    }
    return result;
}

__attribute__((used,noinline,section(".text.unlock.2_recovery")))
int UnlockMode_RecoveryUpdate(UnlockStateView *state,UnlockNativeUpdateFn nativeUpdate)
{
    u32 previousSubstate=state->substate;
    int result=nativeUpdate(state);

    /* Native success here follows an accepted automatic commit or rollback,
       not just matching the descriptor. Report recovery only after state 17
       also finishes the selected-game transaction. */
    /* 此处原版成功来自已接受的自动提交或回滚，而非仅描述匹配；等 state 17
       也完成所选游戏事务后，才显示恢复成功。 */
    if (result == UNLOCK_UPDATE_COMPLETE && state->result == UNLOCK_RESULT_CONTINUE) {
        *UnlockMode_RecoveryStorage=UNLOCK_RECOVERY_AUTOMATIC;
    }

    /* Only the confirmed mismatch notice requests a new game selection.
       Network errors and the official force-unlock branch keep native results. */
    /* 仅在不匹配提示确认后请求重新选择游戏；联网错误和官方强制解锁分支保持原结果。 */
    if (previousSubstate == UNLOCK_MISMATCH_NOTICE_SUBSTATE &&
        result == UNLOCK_UPDATE_PENDING &&
        state->substate == UNLOCK_RECOVERY_ERROR_SUBSTATE) {
        state->result=UNLOCK_RESULT_RESELECT_GAME;
        return UNLOCK_UPDATE_COMPLETE;
    }
    return result;
}
