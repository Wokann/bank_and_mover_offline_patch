#include "bankdata_redirect.h"
#include "fakeServer/fake_server_client.h"
#include "turtle_redirect.h"
#include "unlock_mode.h"
#include "runtime_environment.h"
#include "fs_system/title_users.h"
#include <string.h>

/* Bankdata redirection and local-file transaction implementation. */
/* Bankdata 重定向与本地文件事务实现。 */

#define SERVICE_UPDATE_SCOPE() \
    NativeTransportScope serviceScope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)}; \
    if (serviceScope.status==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING

int BankdataRedirect_SourceMetadata(BankFlowView *flow,FsSourceMetadata *metadata)
{
    BankSelectedGameView *game=flow?BankGame_GetSelected(flow):0;
    if (!game) return 0;
    BankGameInfoLayout *layout=(BankGameInfoLayout *)game;
    BankGameInfoView *info=game->version>=5 && game->version<=8?
        &layout->extended.info:&layout->early.info;
    for (u32 i=0;i<sizeof(*metadata);i++) ((u8 *)metadata)[i]=0;
    metadata->formatVersion=2;
    metadata->nameFormat=(u8)info->vtable->nameFormat(info);
    metadata->trainerId=info->vtable->trainerId(info);
    metadata->gameId=(u8)info->vtable->gameId(info);
    info->vtable->copyTrainerName(info,metadata->trainerName);
    return 1;
}

void BankdataRedirect_TitleInitialize(void *view,u32 screen,u32 pane,u32 modeMessage)
{
    RuntimeEnvironment_InitializeTicket();
    BankUi_SetMessageLine(view,screen,pane,BANK_TITLE_HOME_MESSAGE);
    /* Separate child pane inherits the stock help pane's fade/visibility. */
    /* 独立子窗格继承原帮助窗格的淡入淡出和可见状态，不挤占原文本缓冲。 */
    BankUi_SetMessageLine(view,1,BANK_TITLE_LANGUAGE_PANE,BANK_TITLE_LANGUAGE_MESSAGE);
    BankUi_SetMessageLine(view,1,BANK_TITLE_MODE_PANE,modeMessage);
    BankUi_SetMessageLine(view,1,BANK_TITLE_SWITCH_PANE,BANK_TITLE_SWITCH_MESSAGE);
    TitleUsers_Initialize(view);
}

int BankdataRedirect_InitialRecordUpdate(BankStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    BankInitialRecordView *view=(BankInitialRecordView *)state;
    if (state->substate==2)
        NativeResponse_PollRecord(view->remote,0,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    else if (state->substate==6)
        NativeResponse_PollQuery(view->remote,0,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    else if (state->substate==7) {
        FsCampaignRecord body;
        FsServiceRequest input;
        uintptr_t peer=(uintptr_t)state;
        int prepared=FS_OK;
        if (NativeTransport_Request(peer,peer,&input)!=FS_ASYNC_ACCEPTED)
            prepared=FakeServerClient_PrepareCampaignRequest(&input,view->resourceId);
        FsAsyncDelivery delivery;
        int received=prepared==FS_OK?
            NativeTransport_PollRequest(peer,peer,&input,0,sizeof(body),&delivery):FS_ASYNC_INVALID;
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        int copied=0;
        if (received==FS_ASYNC_READY) {
            copied=delivery.result==FS_OK && delivery.reply.bodySize==sizeof(body) && delivery.body;
            if (copied) memcpy(&body,delivery.body,sizeof(body));
            int released=NativeTransport_Release(&delivery);
            if (released==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
            if (released!=FS_ASYNC_ACCEPTED) copied=0;
        }
        BankCampaignResourceView resource;
        for (u32 i=0;i<sizeof(resource);i++) ((u8 *)&resource)[i]=0;
        resource.resourceId=input.input.campaign.requestId;
        /* Original callback validates resource length/CRC and consumes dates. */
        /* 原回调仍核对资源长度／CRC并消费日期。 */
        if (copied)
            BankCampaign_Response(state,&resource,sizeof(body),&body);
        else BankCampaign_Response(state,0,0,0);
    }
    int result=BankdataRedirect_OriginalInitialRecordUpdate(state);
    if (result==BANK_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}

int BankdataRedirect_InitializeAccount(BankAccountTransportView *transport)
{
    if (!transport->provider) {
        void *storage=BankRemote_Allocate(0x54);
        if (!storage) return 0;
        transport->provider=BankAccount_Construct(storage);
    }
    /* The native initial-parameter request requires this flag, which also
       enables real-network monitoring. Offline mode gates both monitor
       consumers; this does not assert that a system session exists. */
    /* 原初始参数请求依赖此标志，但它也启用真实网络监控；离线在两个监控消费者
       分流，不将该标志视为已建立系统会话。 */
    transport->initialized=1;
    return transport->provider!=0;
}

int BankdataRedirect_UnbindAccount(BankAccountTransportView *transport)
{
    NativeTransport_Cancel((uintptr_t)transport);
    if (transport->provider) {
        void (**table)(void *)=*(void (***) (void *))transport->provider;
        table[1](transport->provider);
        transport->provider=0;
    }
    return 1;
}

int BankdataRedirect_CreateUpdate(BankStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    if (state->substate==5) {
        BankAccountTransportView *account=*(BankAccountTransportView **)0x003AB938u;
        NativeRemoteResult *job=account?account->request:0;
        NativeResponse_PollInitial((uintptr_t)account,job,BankAccount_ParseInitial,
            BankAccount_BuildResult,BankRemote_CompleteJob);
    } else if (state->substate==7) {
        NativeResponse_PollCreate(((BankCreateStateView *)state)->remote,0,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    }
    return BankdataRedirect_OriginalCreateUpdate(state);
}

int BankdataRedirect_DownloadUpdate(BankStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    BankDownloadStateView *view=(BankDownloadStateView *)state;
    if (state->substate==2) {
        FsSourceMetadata metadata;
        int known=BankdataRedirect_SourceMetadata(state->bankdataFlow,&metadata);
        NativeResponse_PollDownload(view->remote,0,known?&metadata:0,
            BankRemote_Allocate, BankRemote_BuildResult,BankRemote_CompleteJob);
    } else if (state->substate==4) {
        FsSourceMetadata metadata;
        int known=BankdataRedirect_SourceMetadata(state->bankdataFlow,&metadata);
        NativeResponse_PollMetadataUpdate(view->remote,0,known?&metadata:0,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    }
    *TurtleRedirect_SaveActiveStorage=1;
    int result=BankdataRedirect_OriginalDownloadUpdate(state);
    *TurtleRedirect_SaveActiveStorage=0;
    return result;
}

/* UI-owned immutable snapshot: the original callback adjusts session flags
   after this hook. No UI object is retained by the worker. The transient native
   heap allocation is released once the process-owned task has copied it. */
/* 界面持有不变快照；原回调在钩子后还会调整会话标志。后台不保留界面对象，
   任务取得自持副本后立即释放临时原生堆分配。 */
static struct { uintptr_t peer; u8 *snapshot; } downloadedBank
    __attribute__((section(".data.download_capture")))={0};
enum { BANK_DOWNLOAD_EXPORT_REQUEST=0x404 };
#define BankSnapshot_GetHeap ((void *(*)(u32))0x00235BFCu)
#define BankSnapshot_Allocate ((void *(*)(u32,void *))0x00235BC8u)
#define BankSnapshot_Free ((void (*)(void *))0x00236A90u)

__attribute__((used,noinline))
void BankdataRedirect_CancelDownloaded(uintptr_t peer)
{
    if (!peer || downloadedBank.peer!=peer) return;
    NativeTransport_Cancel(peer);
    if (downloadedBank.snapshot) BankSnapshot_Free(downloadedBank.snapshot);
    downloadedBank=(__typeof__(downloadedBank)){0};
}

__attribute__((used,noinline,section(".text.offline")))
int BankdataRedirect_CaptureDownloaded(BankStateView *state,BankRuntimeObjectView *object)
{
    BankdataRedirect_CancelDownloaded(downloadedBank.peer);
    /* Native loading already expanded legacy data. Empty banks remain valid;
       all format/transaction decisions belong to the worker endpoint. */
    /* 原版装载已展开旧格式，空银行同样可导出；格式及事务决策仍在后台服务。 */
    if (!state || !object || object->vtable!=BankFile_Vtable) return 0;
    void *heap=BankSnapshot_GetHeap(23);
    u8 *snapshot=heap?BankSnapshot_Allocate(BANK_FILE_SIZE,heap):0;
    if (!snapshot) return 0;
    memcpy(snapshot,&object->record,BANK_FILE_SIZE);
    downloadedBank.peer=(uintptr_t)state;
    downloadedBank.snapshot=snapshot;
    return 1;
}

__attribute__((used,noinline))
int BankdataRedirect_ExportDownloadUpdate(BankStateView *state)
{
    if (downloadedBank.peer==(uintptr_t)state) {
        NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
        if (scope.status==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        int status=FS_IO_ERROR;
        if (scope.status==FS_ASYNC_ACCEPTED) {
            FsServiceRequest input={.operation=FS_SERVICE_EXPORT_BANK};
            FsAsyncDelivery delivery;
            int received=NativeTransport_PollRequest((uintptr_t)state,BANK_DOWNLOAD_EXPORT_REQUEST,
                &input,downloadedBank.snapshot,BANK_FILE_SIZE,&delivery);
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_READY) {
                if (downloadedBank.snapshot) BankSnapshot_Free(downloadedBank.snapshot);
                downloadedBank.snapshot=0;
            }
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
            if (received==FS_ASYNC_READY) {
                int released=NativeTransport_Release(&delivery);
                if (released==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
                if (released==FS_ASYNC_ACCEPTED) status=delivery.result;
            }
        }
        BankdataRedirect_CancelDownloaded((uintptr_t)state);
        *BankDownload_CaptureStatusStorage=status==FS_OK?
            BANK_DOWNLOAD_CAPTURE_COMPLETE:BANK_DOWNLOAD_CAPTURE_FAILED;
    }
    /* The download's original metadata, success/error and cleanup remain
       native; this local acknowledgement cannot approve an online request. */
    /* 下载原元数据、成功／错误及收尾不变；本地发布确认不会替联网请求判定成功。 */
    int result=BankdataRedirect_OriginalDownloadUpdate(state);
    if (result==BANK_STATE_UPDATE_FINISHED) BankdataRedirect_CancelDownloaded((uintptr_t)state);
    return result;
}

static void pollStage(BankStateView *state,NativeRemoteView *remote)
{
    FsSourceMetadata metadata;
    const FsSource source={FS_TRANSACTION_NATIVE_SOURCE,0,0,0,0,0,&metadata};
    NativeUploadView *body=remote?remote->upload:0;
    int known=state && BankdataRedirect_SourceMetadata(state->bankdataFlow,&metadata);
    u32 size=body && body->begin && body->end>=body->begin?(u32)(body->end-body->begin):0;
    NativeResponse_PollStage(remote,known?&source:0,body?body->begin:0,size,
        BankRemote_BuildResult,BankRemote_CompleteJob);
}

int BankdataRedirect_BindRemote(NativeRemoteView *remote)
{
    return NativeResponse_Bind(remote,BankdataRedirect_OriginalBindRemote);
}

int BankdataRedirect_TransactionProvider(NativeRemoteResult *job,NativeRemoteView *remote,
    u32 commit,u32 force)
{
    return NativeResponse_TransactionReply(job,remote,0,commit,force,
        BankRemote_BuildResult);
}

int BankdataRedirect_RecoveryTokenProvider(NativeRemoteResult *job,NativeRemoteView *remote,
    u32 challenge)
{
    return NativeResponse_RecoveryTokenReply(job,remote,0,challenge,
        BankRemote_Allocate,BankRemote_BuildResult);
}

int BankdataRedirect_NoSaveUpdate(BankStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    if (state->substate==2)
        NativeResponse_PollAccepted(((BankRemoteStateView *)state)->remote,BankRemote_Allocate,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    return BankdataRedirect_OriginalNoSaveUpdate(state);
}

int BankdataRedirect_TransactionRecoveryUpdate(BankStateView *state)
{
    SERVICE_UPDATE_SCOPE();
    NativeRemoteView *remote=((BankRemoteStateView *)state)->remote;
    if (state->substate==2)
        NativeResponse_PollMetadata(remote,0,BankRemote_Allocate,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    else if (state->substate==7) NativeResponse_PollAccepted(remote,BankRemote_Allocate,
        BankRemote_BuildResult,BankRemote_CompleteJob);
    return BankdataRedirect_OriginalTransactionRecoveryUpdate(state);
}

int BankdataRedirect_SaveUpdate(BankStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    int status=FS_IO_ERROR;
    int pending=NativeTransport_StatePending((uintptr_t)state,&status);
    if (pending==FS_ASYNC_BUSY || pending==FS_ASYNC_ACCEPTED) return BANK_STATE_UPDATE_WAITING;
    if (pending!=FS_ASYNC_STALE && status!=FS_OK)
        state->substate=state->substate==3?19:20;
    u32 previous=state->substate;
    NativeRemoteView *remote=((BankSaveRemoteView *)state)->remote;
    if (previous==2) pollStage(state,remote);
    else if (previous==10 || previous==12)
        NativeResponse_PollAccepted(remote,BankRemote_Allocate,BankRemote_BuildResult,BankRemote_CompleteJob);
    if (previous==13) {
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        if (!state->sharedData) state->substate=20;
        else {
            input.input.native.descriptor=*(FsTransactionParameters *)state->sharedData;
            input.input.native.event=FS_NATIVE_CLEANUP_STARTING;
            int received=NativeTransport_StateRequest((uintptr_t)state,0x100u+previous,&input,&status);
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
            if (status!=FS_OK) state->substate=20;
        }
    }
    *TurtleRedirect_SaveActiveStorage=1;
    int result=BankdataRedirect_OriginalSaveUpdate(state);
    *TurtleRedirect_SaveActiveStorage=0;
    /* Observe transitions caused by the real SavePoll; do not call it twice. */
    /* 只观察真实 SavePoll 产生的转换，不额外重复轮询原保存作业。 */
    int event=-1;
    if (previous==4 && state->substate==5) event=FS_NATIVE_SOURCE_SAVED;
    else if (previous==8 && state->substate==11) event=FS_NATIVE_SOURCE_RESTORED;
    else if (previous==14 && state->substate==15) event=FS_NATIVE_CLEANUP_SAVED;
    if (event>=0) {
        if (!state->sharedData) {
            state->substate=20;
            return BANK_STATE_UPDATE_WAITING;
        }
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=*(FsTransactionParameters *)state->sharedData;
        input.input.native.event=(uint32_t)event;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x200u+(u32)event,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        if (status!=FS_OK) {
            state->substate=20;
            return BANK_STATE_UPDATE_WAITING;
        }
    }
    if (result==BANK_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}

int BankdataRedirect_ForceRollbackUpdate(BankStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    if (state->substate==2 || state->substate==6)
        NativeResponse_PollAccepted(((BankForceRollbackView *)state)->job,BankRemote_Allocate,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    if (state->substate==5) {
        BankForceRollbackView *view=(BankForceRollbackView *)state;
        /* Native keyboard acceptance precedes the request. The service verifies
           the entered code again against its own challenge/reservation. */
        /* 原键盘验证后才发请求；服务再次核对输入与自己签发的挑战和预留。 */
        FsServiceRequest input={.operation=FS_SERVICE_AUTHORIZE_ROLLBACK};
        input.input.rollback.challenge=view->challenge;
        input.input.rollback.enteredCode=view->enteredCode;
        FsAsyncDelivery delivery;
        int received=NativeTransport_PollRequest((uintptr_t)state,1,&input,0,0,&delivery);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        if (received==FS_ASYNC_READY && NativeTransport_Release(&delivery)==FS_ASYNC_BUSY)
            return BANK_STATE_UPDATE_WAITING;
        /* The subsequent stock rollback request performs the final service
           authorization/result check, including rejected or failed approval. */
        /* 后续原回滚请求仍由服务端最终核对授权并返回结果，授权拒绝或失败不强制放行。 */
    }
    int result=BankdataRedirect_OriginalForceRollbackUpdate(state);
    if (result==BANK_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}

int BankdataRedirect_SourceRecoveryUpdate(BankStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    SERVICE_UPDATE_SCOPE();
    /* No fixed two-second display gate: service replies and the native
       callback drive progress. Keep native request/save waits unchanged. */
    /* 不再添加固定两秒显示门槛；按服务响应和原回调推进，保留原请求及保存等待。 */
    int status=FS_IO_ERROR;
    int pending=NativeTransport_StatePending((uintptr_t)state,&status);
    if (pending==FS_ASYNC_ACCEPTED || pending==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
    if (pending!=FS_ASYNC_STALE && status!=FS_OK) state->substate=9;
    u32 previous=state->substate;
    BankSourceRecoveryView *view=(BankSourceRecoveryView *)state;
    if (previous==4)
        NativeResponse_PollAccepted(((BankRemoteStateView *)state)->remote,BankRemote_Allocate,
            BankRemote_BuildResult,BankRemote_CompleteJob);
    if (previous==5) {
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=view->transaction;
        input.input.native.event=FS_NATIVE_CLEANUP_STARTING;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x100u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        if (status!=FS_OK) state->substate=9;
    }
    *TurtleRedirect_SaveActiveStorage=1;
    int result=BankdataRedirect_OriginalSourceRecoveryUpdate(state);
    *TurtleRedirect_SaveActiveStorage=0;
    if (previous==6 && state->substate==7) {
        FsServiceRequest input={.operation=FS_SERVICE_NATIVE_SAVE_EVENT};
        input.input.native.descriptor=view->transaction;
        input.input.native.event=FS_NATIVE_CLEANUP_SAVED;
        int received=NativeTransport_StateRequest((uintptr_t)state,0x200u,&input,&status);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
        if (status!=FS_OK) { state->substate=9; return BANK_STATE_UPDATE_WAITING; }
    }
    if (result==BANK_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}
