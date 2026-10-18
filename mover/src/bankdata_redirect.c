#include "bankdata_redirect.h"
#include "fakeServer/fake_server_client.h"
#include "nds_sources.h"
#include "runtime_environment.h"
#include "fs_system/title_users.h"

/* Local Bankdata loading and transfer-slot preservation. */
/* 本地 Bankdata 载入与传送槽保留。 */

#define SERVICE_UPDATE_SCOPE() \
    NativeTransportScope serviceScope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)}; \
    if (serviceScope.status==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING

void BankdataRedirect_TitleInitialize(void *view,u32 screen,u32 pane,u32 modeMessage)
{
    RuntimeEnvironment_InitializeTicket();
    MoverUi_SetMessageLine(view,screen,pane,39); /* Native HOME; 原版 HOME。 */
    MoverUi_SetMessageLine(view,1,MOVER_TITLE_MODE_PANE,modeMessage);
    MoverUi_SetMessageLine(view,1,MOVER_TITLE_SWITCH_PANE,MOVER_TITLE_SWITCH_MESSAGE);
    TitleUsers_Initialize(view);
}

int BankdataRedirect_EligibilityUpdate(MoverStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    MoverRemoteCheckView *view=(MoverRemoteCheckView *)state;
    if (state->substate==2)
        NativeResponse_PollDownload(view->remote,0,0,
            MoverRemote_Allocate,MoverRemote_BuildResult,MoverRemote_CompleteJob);
    else if (state->substate==4) {
        FsSourceMetadata metadata;
        int known=BankdataRedirect_SourceMetadata(state->bankdataFlow,&metadata);
        NativeResponse_PollMetadataUpdate(view->remote,0,known?&metadata:0,
            MoverRemote_BuildResult,MoverRemote_CompleteJob);
    }
    return BankdataRedirect_OriginalEligibilityUpdate(state);
}

int BankdataRedirect_SourceMetadata(MoverFlowView *flow,FsSourceMetadata *metadata)
{
    if (!flow || !metadata) return 0;
    MoverMemory_Clear(metadata,sizeof(*metadata));
    metadata->formatVersion=2;
    metadata->gameId=0x18;
    if (flow->vcArchive && flow->vcSave) {
        metadata->trainerId=flow->vcTrainerId;
        metadata->nameFormat=flow->vcNameFormat;
        MoverMemory_Copy(metadata->trainerName,flow->vcTrainerName,sizeof(metadata->trainerName));
    } else {
        metadata->trainerId=MoverNds_GetDisplayValue(flow);
        metadata->nameFormat=(u8)MoverNds_GetDisplayFlag(flow);
        MoverMemory_Copy(metadata->trainerName,MoverNds_GetDisplayName(flow),sizeof(metadata->trainerName));
    }
    return 1;
}

static void pollStage(MoverStateView *state,NativeRemoteView *remote)
{
    MoverFlowView *flow=state?state->bankdataFlow:0;
    FsSourceMetadata metadata;
    FsSource source;
    int known=flow && BankdataRedirect_SourceMetadata(flow,&metadata) &&
        NdsSources_GetSelectedSource(&source);
    if (known) source.metadata=&metadata;
    NativeUploadView *body=remote?remote->upload:0;
    u32 size=body && body->begin && body->end>=body->begin?(u32)(body->end-body->begin):0;
    NativeResponse_PollStage(remote,known?&source:0,body?body->begin:0,size,
        MoverRemote_BuildResult,MoverRemote_CompleteJob);
}

int BankdataRedirect_BindRemote(NativeRemoteView *remote)
{
    return NativeResponse_Bind(remote,BankdataRedirect_OriginalBindRemote);
}

int BankdataRedirect_TransactionProvider(NativeRemoteResult *job,NativeRemoteView *remote,
    u32 commit,u32 force)
{
    return NativeResponse_TransactionReply(job,remote,0,commit,force,
        MoverRemote_BuildResult);
}

int BankdataRedirect_RemoteCheckUpdate(MoverStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    MoverRemoteCheckView *view=(MoverRemoteCheckView *)state;
    if (state->substate==2)
        NativeResponse_PollQuery(view->remote,0,
            MoverRemote_BuildResult,MoverRemote_CompleteJob);
    return BankdataRedirect_OriginalRemoteCheckUpdate(state);
}

int BankdataRedirect_NoSaveUpdate(MoverStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    if (state->substate==2)
        NativeResponse_PollAccepted(((MoverRemoteCheckView *)state)->remote,MoverRemote_Allocate,
            MoverRemote_BuildResult,MoverRemote_CompleteJob);
    return BankdataRedirect_OriginalNoSaveUpdate(state);
}

int BankdataRedirect_TransactionRecoveryUpdate(MoverStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    NativeRemoteView *remote=((MoverTransactionRecoveryView *)state)->remote;
    if (state->substate==2)
        NativeResponse_PollMetadata(remote,0,MoverRemote_Allocate,
            MoverRemote_BuildResult,MoverRemote_CompleteJob);
    else if (state->substate==5 || state->substate==8)
        NativeResponse_PollAccepted(remote,MoverRemote_Allocate,MoverRemote_BuildResult,MoverRemote_CompleteJob);
    return BankdataRedirect_OriginalTransactionRecoveryUpdate(state);
}

int BankdataRedirect_SaveUpdate(MoverStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    int status=FS_IO_ERROR;
    int pending=NativeTransport_StatePending((uintptr_t)state,&status);
    if (pending==FS_ASYNC_ACCEPTED || pending==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
    if (pending!=FS_ASYNC_STALE && status!=FS_OK) state->substate=21;
    u32 previous=state->substate;
    NativeRemoteView *remote=((MoverRemoteCheckView *)state)->remote;
    if (*CombinePatch_ModeStorage==0 && state->substate==2) pollStage(state,remote);
    else if (*CombinePatch_ModeStorage==0 && (state->substate==11 || state->substate==13))
        NativeResponse_PollAccepted(remote,MoverRemote_Allocate,MoverRemote_BuildResult,MoverRemote_CompleteJob);
    if (*CombinePatch_ModeStorage==0 && previous==14) {
        if (!state->sharedData) state->substate=21;
        else {
            FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
            input.input.native.descriptor=*(FsTransactionParameters *)state->sharedData;
            input.input.native.event=FS_NATIVE_CLEANUP_STARTING;
            int received=NativeTransport_StateRequest((uintptr_t)state,0x100u+previous,&input,&status);
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
            if (status!=FS_OK) state->substate=21;
        }
    }
    /* Complete the local decision before the native success message. */
    /* 在原版成功提示出现前完成本地决定。 */
    if (*CombinePatch_ModeStorage==0 && state->substate==16) {
        FsServiceRequest input={.operation=FS_SERVICE_TRANSACTION_EVENT};
        input.input.event.action=FS_TRANSACTION_FINISH;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x300u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
        if (status!=FS_OK) state->substate=21;
        else {
            int settled=NdsSources_SaveCompleted();
            if (!settled) return MOVER_STATE_UPDATE_WAITING;
            if (settled<0) state->substate=21;
        }
    }
    int result=BankdataRedirect_OriginalSaveUpdate(state);
    if (*CombinePatch_ModeStorage==0 && previous==5 && state->substate==10) {
        /* The native SDK just proved the first physical/VC save succeeded.
           Its local file wrapper no longer sends a fictitious source RPC.
           Hand off this one real checkpoint before the native commit request.
           SD images have already acknowledged their verified publication. */
        /* 原 SDK 已确认首次卡带／VC 保存成功；文件包装不再伪装来源请求。
           在原提交请求前仅交接这一真实检查点；SD 镜像已确认验证后的发布。 */
        FsSource source;
        if (!NdsSources_GetSelectedSource(&source) || !state->sharedData) {
            state->substate=21;return MOVER_STATE_UPDATE_WAITING;
        }
        if (source.kind==FS_TRANSACTION_NATIVE_SOURCE) {
            FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
            input.input.native.descriptor=*(FsTransactionParameters *)state->sharedData;
            input.input.native.event=FS_NATIVE_SOURCE_SAVED;
            int received=NativeTransport_StateRequest((uintptr_t)state,0x205u,&input,&status);
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
            if (status!=FS_OK) { state->substate=21;return MOVER_STATE_UPDATE_WAITING; }
        }
    }
    if (*CombinePatch_ModeStorage==0 && previous==15 && state->substate==16) {
        if (!state->sharedData) {
            state->substate=21;
            return MOVER_STATE_UPDATE_WAITING;
        }
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=*(FsTransactionParameters *)state->sharedData;
        input.input.native.event=FS_NATIVE_CLEANUP_SAVED;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x200u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
        if (status!=FS_OK) {
            state->substate=21;
            return MOVER_STATE_UPDATE_WAITING;
        }
    }
    if (NdsSources_PublicationFailed() && result!=MOVER_STATE_UPDATE_FINISHED) {
        /* A publication error may follow an effective rename. Do not let the
           native first-save failure branch roll back the remote transaction. */
        /* 发布报错仍可能已经重命名生效；不能再走原版首次保存失败的远端回滚分支。 */
        state->substate=21;
        return MOVER_STATE_UPDATE_WAITING;
    }
    if (result==MOVER_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}

int BankdataRedirect_SourceRecoveryUpdate(MoverStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    int status=FS_IO_ERROR;
    int pending=NativeTransport_StatePending((uintptr_t)state,&status);
    if (pending==FS_ASYNC_ACCEPTED || pending==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
    if (pending!=FS_ASYNC_STALE && status!=FS_OK) state->substate=11;
    u32 previous=state->substate;
    MoverSourceRecoveryView *view=(MoverSourceRecoveryView *)state;
    if (previous==3 || previous==5)
        NativeResponse_PollAccepted(((MoverSourceRecoveryView *)state)->job,MoverRemote_Allocate,
            MoverRemote_BuildResult,MoverRemote_CompleteJob);
    if (previous==6) {
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=view->transaction;
        input.input.native.event=FS_NATIVE_CLEANUP_STARTING;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x100u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
        if (status!=FS_OK) state->substate=11;
    }
    if (state->substate==9) {
        FsServiceRequest input={.operation=FS_SERVICE_TRANSACTION_EVENT};
        input.input.event.action=FS_TRANSACTION_FINISH;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x300u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
        if (status!=FS_OK) state->substate=11;
        else {
            int settled=NdsSources_SaveCompleted();
            if (!settled) return MOVER_STATE_UPDATE_WAITING;
            if (settled<0) state->substate=11;
        }
    }
    int result=BankdataRedirect_OriginalSourceRecoveryUpdate(state);
    if (previous==8 && state->substate==9) {
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=view->transaction;
        input.input.native.event=FS_NATIVE_CLEANUP_SAVED;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x200u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return MOVER_STATE_UPDATE_WAITING;
        if (status!=FS_OK) { state->substate=11; return MOVER_STATE_UPDATE_WAITING; }
    }
    if (result==MOVER_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}
