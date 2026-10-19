#include "fakeServer/request_broker.h"

enum { PEER_WAITING,PEER_QUEUED,PEER_DELIVERING };

static void clearPeer(FsAsyncPeer *peer)
{
    peer->peer=peer->request=0;
    peer->identity=0;
    peer->phase=PEER_WAITING;
    peer->cancelled=0;
    peer->token=(FsAsyncToken){0};
}

static void cancelPeer(FsAsyncBroker *broker,FsAsyncPeer *peer)
{
    if (peer->phase!=PEER_WAITING)
        (void)FsAsyncTask_Cancel(&broker->task,&peer->token);
    if (peer->phase==PEER_DELIVERING) peer->cancelled=1;
    else clearPeer(peer);
}

static FsAsyncPeer *find(FsAsyncBroker *broker,uint32_t identity)
{
    if (broker && identity)
        for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++)
            if (broker->peers[i].identity==identity) return &broker->peers[i];
    return 0;
}

int FsAsyncBroker_Initialize(FsAsyncBroker *broker,void *body,uint32_t capacity)
{
    if (!broker || (uintptr_t)broker%_Alignof(FsAsyncBroker)) return FS_ASYNC_INVALID;
    int status=FsAsyncTask_Initialize(&broker->task,body,capacity);
    if (status!=FS_ASYNC_ACCEPTED) return status;
    broker->nextIdentity=0;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) clearPeer(&broker->peers[i]);
    return FS_ASYNC_ACCEPTED;
}

int FsAsyncBroker_Accept(FsAsyncBroker *broker,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *input,uint32_t *identity)
{
    if (!broker || (uintptr_t)broker%_Alignof(FsAsyncBroker) ||
        !input || (uintptr_t)input%_Alignof(FsServiceRequest) ||
        !identity || (uintptr_t)identity%_Alignof(uint32_t)) return FS_ASYNC_INVALID;
    if (identity) *identity=0;
    if (!peer || !request ||
        input->operation<FS_SERVICE_OPEN || input->operation>=FS_SERVICE_OPERATION_END ||
        broker->nextIdentity==0xffffffffu) return FS_ASYNC_INVALID;
    FsAsyncPeer *slot=0;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
        FsAsyncPeer *entry=&broker->peers[i];
        /* A native remote has at most one outstanding request. A new send is
           also its exact lifetime boundary, even when the allocator reused
           the job address or an earlier canceled job was never polled again. */
        /* 原远端对象最多持有一个未完成请求；新发送也是精确生命周期边界，
           包括作业地址重用及旧取消请求不再被轮询的情况。 */
        /* The title catalogue/commit is a session boundary. Cancel all older
           consumers before it can select another user. Running work finishes
           under the old selection; the serialized title job follows it. */
        /* 标题用户查询／确认是会话边界，先取消旧消费者；正在执行的任务仍在旧用户
           下完成，串行的标题任务随后才切换用户，旧结果不再投递给新会话。 */
        if (entry->identity && (entry->peer==peer || input->operation==FS_SERVICE_TITLE_USERS))
            cancelPeer(broker,entry);
        if (!entry->identity && !slot) slot=entry;
    }
    if (!slot) return FS_ASYNC_BUSY;
    slot->peer=peer;
    slot->request=request;
    slot->identity=++broker->nextIdentity;
    slot->input=*input;
    *identity=slot->identity;
    return FS_ASYNC_ACCEPTED;
}

uint32_t FsAsyncBroker_Find(const FsAsyncBroker *broker,uintptr_t peer,uintptr_t request,
    uint32_t operation)
{
    if (broker && peer && request)
        for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
            const FsAsyncPeer *entry=&broker->peers[i];
            if (!entry->cancelled && entry->identity && entry->peer==peer &&
                entry->request==request && entry->input.operation==operation)
                return entry->identity;
        }
    return 0;
}

int FsAsyncBroker_Prepare(FsAsyncBroker *broker,uint32_t identity,const FsServiceRequest *input)
{
    FsAsyncPeer *entry=find(broker,identity);
    if (!entry || entry->cancelled) return FS_ASYNC_STALE;
    if (!input || (uintptr_t)input%_Alignof(FsServiceRequest) ||
        input->operation!=entry->input.operation) return FS_ASYNC_INVALID;
    if (entry->phase!=PEER_WAITING) return FS_ASYNC_BUSY;
    entry->input=*input;
    return FS_ASYNC_ACCEPTED;
}

int FsAsyncBroker_Submitted(const FsAsyncBroker *broker,uintptr_t peer,uintptr_t request)
{
    if (broker && peer && request)
        for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
            const FsAsyncPeer *entry=&broker->peers[i];
            if (!entry->cancelled && entry->identity && entry->peer==peer && entry->request==request)
                return entry->phase!=PEER_WAITING;
        }
    return 0;
}

int FsAsyncBroker_Poll(FsAsyncBroker *broker,uint32_t identity,FsAsyncWork work,void *context,
    const void *bodyInput,uint32_t bodySize,FsAsyncDelivery *delivery)
{
    return FsAsyncBroker_PollParts(broker,identity,work,context,bodyInput,bodySize,0,0,delivery);
}

int FsAsyncBroker_PollParts(FsAsyncBroker *broker,uint32_t identity,FsAsyncWork work,void *context,
    const void *bodyInput,uint32_t bodySize,const void *appendInput,uint32_t appendSize,
    FsAsyncDelivery *delivery)
{
    FsAsyncPeer *entry=find(broker,identity);
    if (!entry || entry->cancelled) return FS_ASYNC_STALE;
    if (!work || !delivery) return FS_ASYNC_INVALID;
    if (entry->phase==PEER_WAITING) {
        int status=FsAsyncTask_SubmitParts(&broker->task,identity,work,context,&entry->input,
            sizeof(entry->input),bodyInput,bodySize,appendInput,appendSize,&entry->token);
        if (status==FS_ASYNC_ACCEPTED) entry->phase=PEER_QUEUED;
        return status;
    }
    if (entry->phase==PEER_QUEUED) {
        uint32_t outputSize=0;
        FsAsyncDelivery *value=&entry->delivery;
        int status=FsAsyncTask_Take(&broker->task,&entry->token,&value->reply,sizeof(value->reply),
            &outputSize,&value->result,&value->body,&value->bodySize);
        if (status!=FS_ASYNC_READY) return status;
        value->identity=identity;
        value->token=entry->token;
        /* A platform failure has no server payload. Do not recycle bytes from
           a previous request or reinterpret request completion as approval. */
        /* 平台失败没有服务正文；不能复用前次数据，也不把请求完成解释为业务批准。 */
        if (outputSize!=sizeof(value->reply)) {
            value->reply=(FsServiceReply){0};
            if (value->result>=0) value->result=FS_INVALID;
        }
        if (value->result!=FS_OK) value->reply.bodySize=0;
        if (value->reply.bodySize>value->bodySize ||
            (!value->body && value->reply.bodySize)) {
            value->reply.bodySize=0;
            value->result=FS_INVALID;
        }
        entry->phase=PEER_DELIVERING;
    }
    *delivery=entry->delivery;
    return FS_ASYNC_READY;
}

int FsAsyncBroker_Release(FsAsyncBroker *broker,const FsAsyncDelivery *delivery)
{
    FsAsyncPeer *entry=delivery?find(broker,delivery->identity):0;
    if (!entry || entry->phase!=PEER_DELIVERING ||
        delivery->token.owner!=entry->token.owner ||
        delivery->token.generation!=entry->token.generation) return FS_ASYNC_STALE;
    int status=FsAsyncTask_Release(&broker->task,&entry->token);
    if (status!=FS_ASYNC_BUSY) clearPeer(entry);
    return status;
}

int FsAsyncBroker_CancelPeer(FsAsyncBroker *broker,uintptr_t peer)
{
    if (!broker || !peer) return FS_ASYNC_INVALID;
    int result=FS_ASYNC_STALE;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
        FsAsyncPeer *entry=&broker->peers[i];
        if (entry->identity && entry->peer==peer) {
            cancelPeer(broker,entry);
            result=FS_ASYNC_ACCEPTED;
        }
    }
    return result;
}

int FsAsyncBroker_CancelRequest(FsAsyncBroker *broker,uintptr_t request)
{
    if (!broker || !request) return FS_ASYNC_INVALID;
    int result=FS_ASYNC_STALE;
    for (uint32_t i=0;i<FS_ASYNC_PEER_CAPACITY;i++) {
        FsAsyncPeer *entry=&broker->peers[i];
        if (entry->identity && entry->request==request) {
            cancelPeer(broker,entry);
            result=FS_ASYNC_ACCEPTED;
        }
    }
    return result;
}
