#include "offline_flow.h"

/* Local replacements for connection and save-display state updates. */
/* 连接与保存提示状态的本地替代实现。 */
__attribute__((noinline))
static int updateNetworkFlag(BankStateView *state,u8 connected)
{
    BankFlowView *flow;
    BankNetworkContextView *network;
    if (!StateTimer_HasElapsed(state,BANK_NETWORK_DISPLAY_DELAY_MS))
        return BANK_STATE_UPDATE_WAITING;
    flow=state->networkFlow;
    network=flow?flow->networkContext:0;
    if (network) network->connected=connected;
    state->phase=BANK_STATE_PHASE_COMPLETE;
    return BANK_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_NetworkUpdate(BankStateView *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    return updateNetworkFlag(state,BANK_NETWORK_CONNECTED);
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_PostSelectionConnectionUpdate(BankStateView *state)
{
    /* This stock screen represents the server transaction after game checking.
       Its initializer does not start a timer, so start a local 2-second delay
       before reporting completion without a remote job. */
    /* 此原版界面表示游戏检查后的服务器事务。其初始化函数不会启动计时器，
       因此先启动本地 2 秒延时，再在不创建远端作业的情况下报告完成。 */
    if (!state->postSelectionDelayMarker) {
        StateTimer_Reset(state);
        state->postSelectionDelayMarker=1;
        return BANK_STATE_UPDATE_WAITING;
    }
    if (!StateTimer_HasElapsed(state,BANK_LOCAL_DISPLAY_DELAY_MS))
        return BANK_STATE_UPDATE_WAITING;
    state->postSelectionDelayMarker=0;

    /* Report completion through the stock state transition. Its native exit
       path owns the wait UI, animation and sound cleanup. */
    /* 通过原版状态转换报告完成；等待界面、动画及声音均由其原生退出路径负责清理。 */
    state->phase=BANK_STATE_PHASE_COMPLETE;
    return BANK_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_DisconnectUpdate(BankStateView *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    return updateNetworkFlag(state,BANK_NETWORK_DISCONNECTED);
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_SaveDisplayDelayUpdate(BankStateView *state)
{
    /* The local write is synchronous. Reuse the stock callback byte as a
       private marker and keep the save message alive for at least two seconds. */
    /* 本地写入是同步的。复用原版回调字节作为私有标记，让保存提示至少显示两秒。 */
    if (!state->saveDelayMarker) {
        StateTimer_Reset(state);
        state->saveDelayMarker=1;
        return BANK_STATE_UPDATE_WAITING;
    }
    if (!StateTimer_HasElapsed(state,BANK_LOCAL_DISPLAY_DELAY_MS))
        return BANK_STATE_UPDATE_WAITING;
    state->saveDelayMarker=0;
    state->substate=BANK_SAVE_LOCAL_COMPLETE_SUBSTATE;
    return BANK_STATE_UPDATE_FINISHED;
}
