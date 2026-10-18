#ifndef COMMON_FS_SERVICE_WORKER_H
#define COMMON_FS_SERVICE_WORKER_H

#include "fakeServer/request_broker.h"
#include "native_worker.h"

/* Process-owned storage. Native SDK addresses only exist in the UI registry;
   the worker sees FsServiceContext and copies of request/body values. */
/* 存储归进程所有；原 SDK 地址仅在界面登记表中使用，后台只接触服务上下文与请求副本。 */
typedef struct NativeTicketUi {
    uintptr_t job,provider;
    uint32_t purchasePhase;
} NativeTicketUi;

typedef struct NativeServiceWorker {
    volatile uint32_t initialized;
    NativeWorker platform;
    FsAsyncBroker broker;
    FsServiceContext service;
    /* UI-only acknowledgements inside one original client state. They cache
       completed checkpoints, not file validity or server decisions. */
    /* 仅界面使用、限一个原状态生命周期的确认缓存，不缓存文件有效性或自行决定事务。 */
    struct {
        uintptr_t peer,pending;
        struct { uintptr_t tag; int result; } replies[8];
        struct { uintptr_t peer,target; uint32_t identity,position; } copy;
    } ui;
    /* Transport-only checkout phase; no entitlement or policy cache. */
    /* 仅结账传输阶段，不缓存使用权或策略。 */
    NativeTicketUi ticket;
} NativeServiceWorker;

int NativeService_Start(NativeServiceWorker *,const FsServiceContext *,
    void *body,uint32_t bodyCapacity,void *stack,uint32_t stackSize);
int NativeService_Accept(NativeServiceWorker *,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *,uint32_t *identity);
int NativeService_Poll(NativeServiceWorker *,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *,const void *body,uint32_t bodySize,FsAsyncDelivery *);
int NativeService_PollParts(NativeServiceWorker *,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *,const void *body,uint32_t bodySize,
    const void *appendInput,uint32_t appendSize,FsAsyncDelivery *);
int NativeService_Release(NativeServiceWorker *,const FsAsyncDelivery *);
int NativeService_CancelPeer(NativeServiceWorker *,uintptr_t peer);
int NativeService_CancelRequest(NativeServiceWorker *,uintptr_t request);

/* Existing SDK workers serialize complete service operations with the same
   mutex. UI callers use wait=0 and defer the original state update when busy;
   neither cancellation nor a UI destructor waits for this mutex/thread.
   Lock returns ACCEPTED, BUSY, or INVALID for failed/uninitialized platform;
   failure must not be treated as an endless UI wait. Unlock remains available
   to the lock owner even if another thread marked the platform failed. */
/* 既有 SDK 工作线程以同一互斥量保护完整服务操作；界面只非阻塞尝试，忙时保留
   原等待状态。取消及界面析构不等待此互斥量或线程结束。Lock 区分成功、忙和平台
   失败，失败不能变成永久等待；已取得锁仍须释放，即便其他线程标记平台失败。 */
int NativeService_Lock(NativeServiceWorker *,uint32_t wait);
int NativeService_Unlock(NativeServiceWorker *);

#endif
