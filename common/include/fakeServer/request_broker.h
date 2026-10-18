#ifndef COMMON_FAKE_SERVER_REQUEST_BROKER_H
#define COMMON_FAKE_SERVER_REQUEST_BROKER_H

#include "service_request.h"

#define FS_ASYNC_PEER_CAPACITY 8u

typedef struct FsAsyncDelivery {
    uint32_t identity;
    FsAsyncToken token;
    FsServiceReply reply;
    const void *body;
    uint32_t bodySize;
    int result;
} FsAsyncDelivery;

typedef struct FsAsyncPeer {
    uintptr_t peer,request;
    uint32_t identity,phase,cancelled;
    FsAsyncToken token;
    FsServiceRequest input;
    FsAsyncDelivery delivery;
} FsAsyncPeer;

/* The UI-side registry holds opaque addresses, never worker-owned SDK views.
   Accept is called once for each native request, not inferred from an address
   that an allocator may reuse. The worker only sees the task's copies. */
/* 界面侧登记表只保存不透明地址；每个原生请求各接受一次，不能凭可复用地址推定
   生命周期。后台仅看到任务副本，不读取登记表中的 SDK／界面对象。 */
typedef struct FsAsyncBroker {
    FsAsyncTask task;
    uint32_t nextIdentity;
    FsAsyncPeer peers[FS_ASYNC_PEER_CAPACITY];
} FsAsyncBroker;

int FsAsyncBroker_Initialize(FsAsyncBroker *,void *body,uint32_t capacity);
int FsAsyncBroker_Accept(FsAsyncBroker *,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *,uint32_t *identity);
uint32_t FsAsyncBroker_Find(const FsAsyncBroker *,uintptr_t peer,uintptr_t request,
    uint32_t operation);
int FsAsyncBroker_Submitted(const FsAsyncBroker *,uintptr_t peer,uintptr_t request);
/* Add values observed by the UI at the first wait boundary, before Submit.
   An already submitted request is immutable and cannot be reconfigured. */
/* 在首次等待边界补入界面取得的值；提交后输入固定，不随后续界面或配置变化。 */
int FsAsyncBroker_Prepare(FsAsyncBroker *,uint32_t identity,const FsServiceRequest *);

/* UI-only, nonblocking. ACCEPTED means newly queued: the platform must wake
   the worker. BUSY means wait; READY pins the copied response until Release.
   Body input is borrowed only during this call, never retained in the registry. */
/* 仅界面侧调用且不阻塞。ACCEPTED 表示刚排队、平台须唤醒线程；BUSY 继续等待；
   READY 固定结果到 Release。正文输入只在本次调用中借用，不存入登记表。 */
int FsAsyncBroker_Poll(FsAsyncBroker *,uint32_t identity,FsAsyncWork,void *context,
    const void *bodyInput,uint32_t bodySize,FsAsyncDelivery *);
int FsAsyncBroker_PollParts(FsAsyncBroker *,uint32_t identity,FsAsyncWork,void *context,
    const void *bodyInput,uint32_t bodySize,const void *appendInput,uint32_t appendSize,
    FsAsyncDelivery *);
int FsAsyncBroker_Release(FsAsyncBroker *,const FsAsyncDelivery *);

/* Call before native unbind/job destruction. Cancellation does not join or
   pretend to undo a running publication. A delivery already taken still must
   be released by its UI consumer, even if a callback cancels its identity. */
/* 原解绑／作业析构前调用；不等待后台，也不假装撤销已生效的发布。已取得的结果
   即便被回调取消，界面消费者也必须释放；取消不能提前复用其正文。 */
int FsAsyncBroker_CancelPeer(FsAsyncBroker *,uintptr_t peer);
int FsAsyncBroker_CancelRequest(FsAsyncBroker *,uintptr_t request);

#endif
