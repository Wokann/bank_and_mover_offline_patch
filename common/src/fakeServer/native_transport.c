#include "fakeServer/native_transport.h"
#include "fakeServer/sd_adapter.h"
#include "fake_server_context.h"
#include <string.h>

_Static_assert(FAKE_SERVER_WORKER_ADDRESS==FAKE_SERVER_CONTEXT_ADDRESS+FAKE_SERVER_CONTEXT_SIZE,
    "worker context boundary");
_Static_assert(FAKE_SERVER_TASK_BODY_ADDRESS==FAKE_SERVER_WORKER_ADDRESS+0x1000u,
    "worker control page boundary");
_Static_assert(FAKE_SERVER_WORKER_STACK_ADDRESS==FAKE_SERVER_TASK_BODY_ADDRESS+FAKE_SERVER_TASK_BODY_SIZE,
    "worker body/stack boundary");

static NativeServiceWorker *worker(void)
{
    return (NativeServiceWorker *)FAKE_SERVER_WORKER_ADDRESS;
}

static void reapCancelled(void)
{
    /* Native consumers only borrow a delivery within one UI call. A canceled
       result waiting for a short Release lock no longer has a live borrower;
       retire its pin before a later request reuses the transport buffer. */
    /* 原生消费者只在单次界面调用内借用结果；取消后仅因短锁等待释放的结果已无借用者，
       后续请求复用缓冲前回收它，不让已析构作业永久占据后台任务。 */
    FsAsyncBroker *broker=&worker()->broker;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
        FsAsyncPeer *entry=&broker->peers[i];
        if (entry->identity && entry->cancelled)
            (void)NativeService_Release(worker(),&entry->delivery);
    }
}

int NativeTransport_Start(void)
{
#ifdef FAKE_SERVER_SOURCE_CONTEXT_ADDRESS
    FsLocalSource *local=(FsLocalSource *)FAKE_SERVER_SOURCE_CONTEXT_ADDRESS;
    if (FsLocalSource_Initialize(local,&FsSdTransactions,(void *)FAKE_SERVER_SOURCE_IMAGE_ADDRESS,
        FAKE_SERVER_SOURCE_IMAGE_SIZE)!=FS_OK) return FS_ASYNC_INVALID;
#else
    FsLocalSource *local=0;
#endif
    const FsServiceContext service={
        (FsServer *)FAKE_SERVER_CONTEXT_ADDRESS,&FsSdStorage,&FsSdTransactions,
        FAKE_SERVER_CONTEXT_SIZE,FAKE_SERVER_CLIENT_OWNER,local
    };
    return NativeService_Start(worker(),&service,(void *)FAKE_SERVER_TASK_BODY_ADDRESS,
        FAKE_SERVER_TASK_BODY_SIZE,(void *)FAKE_SERVER_WORKER_STACK_ADDRESS,
        FAKE_SERVER_WORKER_STACK_SIZE);
}

int NativeTransport_Accept(uintptr_t peer,uintptr_t request,const FsServiceRequest *input)
{
    reapCancelled();
    uint32_t identity;
    return NativeService_Accept(worker(),peer,request,input,&identity);
}

int NativeTransport_Poll(uintptr_t peer,uintptr_t request,const FsServiceRequest *input,
    const void *body,uint32_t size,FsAsyncDelivery *delivery)
{
    return NativeTransport_PollParts(peer,request,input,body,size,0,0,delivery);
}

int NativeTransport_PollParts(uintptr_t peer,uintptr_t request,const FsServiceRequest *input,
    const void *body,uint32_t size,const void *appendInput,uint32_t appendSize,
    FsAsyncDelivery *delivery)
{
    reapCancelled();
    return NativeService_PollParts(worker(),peer,request,input,body,size,appendInput,appendSize,delivery);
}

int NativeTransport_Release(const FsAsyncDelivery *delivery)
{
    int result=NativeService_Release(worker(),delivery);
    if (result!=FS_ASYNC_BUSY && delivery && worker()->ui.copy.identity==delivery->identity)
        worker()->ui.copy=(__typeof__(worker()->ui.copy)){0};
    return result;
}

int NativeTransport_Request(uintptr_t peer,uintptr_t request,FsServiceRequest *input)
{
    if (!input || !peer || !request) return FS_ASYNC_INVALID;
    const FsAsyncBroker *broker=&worker()->broker;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
        const FsAsyncPeer *entry=&broker->peers[i];
        if (!entry->cancelled && entry->identity && entry->peer==peer && entry->request==request) {
            *input=entry->input;
            return FS_ASYNC_ACCEPTED;
        }
    }
    return FS_ASYNC_STALE;
}

int NativeTransport_Submitted(uintptr_t peer,uintptr_t request)
{
    return FsAsyncBroker_Submitted(&worker()->broker,peer,request);
}

int NativeTransport_CopyReply(uintptr_t peer,const FsAsyncDelivery *delivery,void *target,
    uint32_t size,uint32_t budget)
{
    if (!peer || !delivery || !delivery->identity || !delivery->body || !target || !budget ||
        size>delivery->reply.bodySize || size>delivery->bodySize) return FS_ASYNC_INVALID;
    __typeof__(worker()->ui.copy) *copy=&worker()->ui.copy;
    if (copy->identity!=delivery->identity || copy->peer!=peer || copy->target!=(uintptr_t)target)
        *copy=(__typeof__(*copy)){peer,(uintptr_t)target,delivery->identity,0};
    if (copy->position>size) return FS_ASYNC_INVALID;
    uint32_t count=size-copy->position;
    if (count>budget) count=budget;
    memcpy((uint8_t *)target+copy->position,(const uint8_t *)delivery->body+copy->position,count);
    copy->position+=count;
    return copy->position==size?FS_ASYNC_READY:FS_ASYNC_BUSY;
}

void NativeTransport_Cancel(uintptr_t peer)
{
    if (worker()->ticket.job && (worker()->ticket.job==peer || worker()->ticket.provider==peer)) {
        uintptr_t job=worker()->ticket.job;
        worker()->ticket=(NativeTicketUi){0};
        if (job!=peer) (void)NativeService_CancelPeer(worker(),job);
    }
    if (worker()->ui.peer==peer) worker()->ui=(__typeof__(worker()->ui)){0};
    else if (worker()->ui.copy.peer==peer) worker()->ui.copy=(__typeof__(worker()->ui.copy)){0};
    (void)NativeService_CancelPeer(worker(),peer);
    reapCancelled();
}

NativeTicketUi *NativeTransport_TicketUi(uintptr_t job,uintptr_t provider)
{
    if (!job || !provider) return 0;
    NativeTicketUi *state=&worker()->ticket;
    if (state->job && state->job!=job)
        NativeTransport_Cancel(state->provider?state->provider:state->job);
    state->job=job;state->provider=provider;
    return state;
}

int NativeTransport_PollRequest(uintptr_t peer,uintptr_t request,const FsServiceRequest *input,
    const void *body,uint32_t size,FsAsyncDelivery *delivery)
{
    if (!peer || !request || !input || !delivery) return FS_ASYNC_INVALID;
    FsServiceRequest snapshot;
    if (NativeTransport_Request(peer,request,&snapshot)!=FS_ASYNC_ACCEPTED) {
        int accepted=NativeTransport_Accept(peer,request,input);
        if (accepted!=FS_ASYNC_ACCEPTED) return accepted;
    } else if (snapshot.operation!=input->operation) return FS_ASYNC_INVALID;
    return NativeTransport_Poll(peer,request,input,body,size,delivery);
}

int NativeTransport_PollSource(uintptr_t peer,uintptr_t request,uint32_t operation,
    const FsSource *source,uint32_t nativeResult,FsAsyncDelivery *delivery)
{
    if (!source || (operation!=FS_SERVICE_OBSERVE_SOURCE && operation!=FS_SERVICE_BEGIN_SOURCE_SAVE &&
        operation!=FS_SERVICE_SOURCE_SAVE_RESULT && operation!=FS_SERVICE_SETTLED_SOURCE)) return FS_ASYNC_INVALID;
    FsServiceRequest input={.operation=operation};
    input.input.source.kind=source->kind;
    input.input.source.mode=source->mode;
    input.input.source.gameCode=source->gameCode;
    input.input.source.pathSize=source->pathSize;
    input.input.source.nativeResult=nativeResult;
    if (source->expected) input.input.source.fingerprint=*source->expected;
    return NativeTransport_PollRequest(peer,request,&input,source->path,source->pathSize,delivery);
}

int NativeTransport_StateRequest(uintptr_t peer,uintptr_t tag,
    const FsServiceRequest *input,int *result)
{
    if (result) *result=FS_IO_ERROR;
    if (!peer || !tag || !input || !result ||
        (input->operation!=FS_SERVICE_NATIVE_SAVE_EVENT &&
         input->operation!=FS_SERVICE_TRANSACTION_EVENT)) return FS_ASYNC_INVALID;
    if (worker()->ui.peer!=peer) {
        if (worker()->ui.peer) NativeTransport_Cancel(worker()->ui.peer);
        worker()->ui.peer=peer;
    }
    int slot=-1;
    for (uint32_t i=0;i<8;i++) {
        if (worker()->ui.replies[i].tag==tag) {
            *result=worker()->ui.replies[i].result;
            return FS_ASYNC_READY;
        }
        if (!worker()->ui.replies[i].tag && slot<0) slot=(int)i;
    }
    if (slot<0) return FS_ASYNC_INVALID;
    if (worker()->ui.pending && worker()->ui.pending!=tag) return FS_ASYNC_BUSY;
    FsServiceRequest snapshot;
    if (NativeTransport_Request(peer,tag,&snapshot)!=FS_ASYNC_ACCEPTED) {
        int accepted=NativeTransport_Accept(peer,tag,input);
        if (accepted!=FS_ASYNC_ACCEPTED) return accepted;
    } else if (snapshot.operation!=input->operation) return FS_ASYNC_INVALID;
    worker()->ui.pending=tag;
    FsAsyncDelivery delivery;
    int received=NativeTransport_Poll(peer,tag,input,0,0,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return received;
    if (received==FS_ASYNC_READY) {
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return FS_ASYNC_BUSY;
        if (released==FS_ASYNC_ACCEPTED) *result=delivery.result;
    }
    worker()->ui.pending=0;
    worker()->ui.replies[slot].tag=tag;
    worker()->ui.replies[slot].result=*result;
    return FS_ASYNC_READY;
}

int NativeTransport_StatePending(uintptr_t peer,int *result)
{
    if (worker()->ui.peer!=peer || !worker()->ui.pending) return FS_ASYNC_STALE;
    FsServiceRequest input;
    if (NativeTransport_Request(peer,worker()->ui.pending,&input)!=FS_ASYNC_ACCEPTED) {
        worker()->ui.pending=0;
        if (result) *result=FS_IO_ERROR;
        return FS_ASYNC_INVALID;
    }
    return NativeTransport_StateRequest(peer,worker()->ui.pending,&input,result);
}

int NativeTransport_Lock(uint32_t wait)
{
    /* Even startup settings must take the actual mutex. A bypassed scope must
       not later release a mutex constructed by a nested native request. */
    /* 启动设置也取得真实锁，不能用免锁作用域在嵌套请求建锁后误释放不属于自己的锁。 */
    if (!__atomic_load_n(&worker()->initialized,__ATOMIC_ACQUIRE) &&
        NativeTransport_Start()!=FS_ASYNC_ACCEPTED) return FS_ASYNC_INVALID;
    return NativeService_Lock(worker(),wait);
}

void NativeTransport_Unlock(void)
{
    if (__atomic_load_n(&worker()->initialized,__ATOMIC_ACQUIRE))
        (void)NativeService_Unlock(worker());
}

void NativeTransport_Leave(NativeTransportScope *scope)
{
    if (scope && scope->status==FS_ASYNC_ACCEPTED) NativeTransport_Unlock();
}
