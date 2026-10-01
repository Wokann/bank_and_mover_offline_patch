#include "offline_flow.h"

/* Local replacements for Mover connection, validation, and save states. */
/* Mover 连接、验证与保存状态的本地替代实现。 */
__attribute__((used,noinline,section(".text.offline.01_network")))
int OfflinePatch_NetworkUpdate(MoverStateView *state)
{
    MoverFlowView *flow;
    MoverNetworkContextView *network;
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,MOVER_NETWORK_DISPLAY_DELAY_MS))
        return MOVER_STATE_UPDATE_WAITING;
    flow=state->networkFlow;
    network=flow?flow->networkContext:0;
    if (network) network->connected=MOVER_NETWORK_CONNECTED;
    state->phase=MOVER_NETWORK_PHASE_COMPLETE;
    return MOVER_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline.02_disconnect")))
int OfflinePatch_DisconnectUpdate(MoverStateView *state)
{
    MoverFlowView *flow;
    MoverNetworkContextView *network;
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,MOVER_NETWORK_DISPLAY_DELAY_MS))
        return MOVER_STATE_UPDATE_WAITING;
    flow=state->networkFlow;
    network=flow?flow->networkContext:0;
    if (network) network->connected=MOVER_NETWORK_DISCONNECTED;
    state->phase=MOVER_NETWORK_PHASE_COMPLETE;
    return MOVER_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline.04_remote_check")))
int OfflinePatch_RemoteCheckUpdate(MoverStateView *state)
{
    state->phase=MOVER_REMOTE_CHECK_PHASE_COMPLETE;
    return MOVER_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline.06_no_transfer")))
int OfflinePatch_NoTransferUpdate(MoverStateView *state)
{
    /* Preserve the stock connection-screen timer, but never create or wait for
       the final remote no-transfer transaction. */
    /* 保留原版连接界面的计时，但不创建或等待最后的不传送远端事务。 */
    if (!StateTimer_HasElapsed(state,MOVER_NETWORK_DISPLAY_DELAY_MS))
        return MOVER_STATE_UPDATE_WAITING;
    state->phase=MOVER_NO_TRANSFER_PHASE_COMPLETE;
    return MOVER_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline.07_save_delay")))
int OfflinePatch_SaveDisplayDelayUpdate(MoverStateView *state)
{
    /* The local stage write is synchronous. Reuse the stock callback byte as
       a private marker and keep the save message alive for at least two seconds. */
    /* 本地暂存写入是同步的。复用原版回调字节作为私有标记，让保存提示至少显示两秒。 */
    if (!state->saveDelayMarker) {
        StateTimer_Reset(state);
        state->saveDelayMarker=1;
        return MOVER_STATE_UPDATE_WAITING;
    }
    if (!StateTimer_HasElapsed(state,MOVER_LOCAL_SAVE_DISPLAY_DELAY_MS))
        return MOVER_STATE_UPDATE_WAITING;
    state->saveDelayMarker=0;
    state->substate=MOVER_SAVE_SUBSTATE_LOCAL_COMPLETE;
    return MOVER_STATE_UPDATE_FINISHED;
}
