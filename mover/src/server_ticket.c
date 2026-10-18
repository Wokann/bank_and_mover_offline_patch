#include "server_ticket.h"
#include "fakeServer/fake_server_client.h"
#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(MoverTicketJobView,rpcClient)==0x24,"native ticket RPC client");
_Static_assert(offsetof(MoverTicketJobView,rpcJob)==0x28,"native ticket SDK job");
_Static_assert(offsetof(MoverTicketJobView,expiryDate)==0x70,"native ticket expiry date");

__attribute__((used,noinline,section(".text.offline.03_ticket")))
int ServerTicket_Send(void *provider,NativeRemoteResult *job,const NativeRpcReaderView *request)
{
    if ((uintptr_t)provider<0x10u) return 0;
    return NativeResponse_QueueTicket((uintptr_t)provider-0x10u,job,request,MoverTicket_Parsers,
        MoverTicket_Result,MoverTicket_Complete);
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
int ServerTicket_CancelProvider(void *provider)
{
    NativeTransport_Cancel((uintptr_t)provider);
    return 1;
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
int ServerTicket_Poll(MoverTicketJobView *job,u32 *result,u32 minimumDays)
{
    if (job) NativeResponse_PollTicket((uintptr_t)job->rpcClient,job->rpcJob,
        MoverTicket_Parsers,MoverTicket_Result,MoverTicket_Complete);
    return ServerTicket_OriginalPoll(job,result,minimumDays);
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
s32 ServerTicket_CampaignResult(MoverTicketStateView *state)
{
    if (!state || !state->job || !state->job->rpcClient) return (s32)0x80000000u;
    uintptr_t peer=(uintptr_t)state->job->rpcClient;
    FsServiceRequest input;
    if (FakeServerClient_PrepareCampaignRequest(&input,state->campaignRequestId)!=FS_OK)
        return (s32)0x80000000u;
    /* Reuse the native provider lifetime: its unbind cancels both ticket and
       campaign work, before the original state/job is destroyed. */
    /* 活动沿用原提供者的生命周期；原解绑在状态／作业析构前取消票务和活动请求。 */
    NativeTransport_Cancel(peer);
    int accepted=NativeTransport_Accept(peer,(uintptr_t)state,&input);
    if (accepted!=FS_ASYNC_ACCEPTED) return (s32)0x80000000u;
    FsAsyncDelivery delivery;
    int queued=NativeTransport_Poll(peer,(uintptr_t)state,&input,0,sizeof(FsCampaignRecord),&delivery);
    if (queued!=FS_ASYNC_ACCEPTED && queued!=FS_ASYNC_BUSY && queued!=FS_ASYNC_READY) {
        NativeTransport_Cancel(peer);return (s32)0x80000000u;
    }
    return 0;
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
s32 ServerTicket_UpdateState(MoverTicketStateView *state)
{
    if (state && state->substate==6 && state->job) {
        uintptr_t peer=(uintptr_t)state->job->rpcClient;
        FsServiceRequest input;
        if (NativeTransport_Request(peer,(uintptr_t)state,&input)==FS_ASYNC_ACCEPTED &&
            input.operation==FS_SERVICE_CAMPAIGN_RESOURCE) {
            FsAsyncDelivery delivery;
            int received=NativeTransport_Poll(peer,(uintptr_t)state,&input,0,sizeof(FsCampaignRecord),&delivery);
            if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
            FsCampaignRecord campaign;
            int copied=0;
            if (received==FS_ASYNC_READY) {
                copied=delivery.result==FS_OK && delivery.reply.bodySize==sizeof(campaign) && delivery.body;
                if (copied) memcpy(&campaign,delivery.body,sizeof(campaign));
                int released=NativeTransport_Release(&delivery);
                if (released==FS_ASYNC_BUSY) return 0;
                if (released!=FS_ASYNC_ACCEPTED) copied=0;
            }
            MoverCampaignSourceView source={0};
            source.requestId=input.input.campaign.requestId;
            /* The original callback still validates CRC, time and request ID. */
            /* 原回调仍核对 CRC、时间和请求编号，不直接设置免费或使用权标志。 */
            MoverTicketState_CampaignCallback(state,&source,copied?sizeof(campaign):0,copied?&campaign:0);
        }
    }
    return ServerTicket_OriginalState(state);
}
/* Select once per constructed job so poll, campaign and cleanup keep one backend.
   The startup cache selects server results before sending any request, only
   for a missing emulator interface; online failures never switch the backend.
   Native result handling and ownership remain unchanged. */
/* 每个已构造作业仅选择一次，轮询、活动与清理不会中途切换后端。
   发送请求前读取启动缓存，仅模拟器缺接口时选择伪服务器；联网失败不切换后端。
   保留原生结果处理和对象所有权。 */
__attribute__((used,noinline,section(".text.offline.03_ticket")))
int ServerTicket_InitializeSelected(MoverTicketJobView *job,
    MoverTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback)
{
    (void)shared;
    int local=FakeServerClient_SelectTicket(mode==TICKET_MODE_OFFLINE,
        allowEmulatorFallback==TICKET_ALLOW_EMULATOR_FALLBACK);
    if (local && !NativeResponse_InitializeRuntime()) return 0;
    return TicketJob_Initialize(job);
}
