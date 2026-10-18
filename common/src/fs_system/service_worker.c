#include "fs_system/service_worker.h"
#include "fakeServer/fake_servers.h"

enum { SERVICE_NEW,SERVICE_STARTING,SERVICE_READY,SERVICE_FAILED };

_Static_assert(sizeof(NativeServiceWorker)<=0x1000u,"service worker control page");

static void platformFailure(NativeServiceWorker *worker)
{
    __atomic_store_n(&worker->platform.state,NATIVE_WORKER_FAILED,__ATOMIC_RELEASE);
    /* Only a queued task may be failed externally. A started operation keeps
       ownership until it returns; an effective publish cannot be undone here. */
    /* 外部只能使排队任务失败；已经启动的操作保持所有权到返回，不能擅自撤销有效发布。 */
    (void)FsAsyncTask_FailQueued(&worker->broker.task,FS_IO_ERROR);
}

static void serviceMain(void *opaque)
{
    NativeServiceWorker *worker=opaque;
    for (;;) {
        if (!NativeWorker_Wait(&worker->platform)) { platformFailure(worker); return; }
        if (!NativeWorker_LockService(&worker->platform,1)) { platformFailure(worker); return; }
        (void)FsAsyncTask_Run(&worker->broker.task);
        if (!NativeWorker_UnlockService(&worker->platform)) { platformFailure(worker); return; }
    }
}

int NativeService_Start(NativeServiceWorker *worker,const FsServiceContext *service,
    void *body,uint32_t bodyCapacity,void *stack,uint32_t stackSize)
{
    if (!worker || (uintptr_t)worker%_Alignof(NativeServiceWorker) || !service ||
        (uintptr_t)service%_Alignof(FsServiceContext) || !service->server ||
        !service->storage || !service->transactions || service->capacity<sizeof(FsServer) ||
        (service->owner!=FS_TRANSACTION_OWNER_BANK && service->owner!=FS_TRANSACTION_OWNER_MOVER) ||
        !body || bodyCapacity<FS_BANK_SIZE)
        return FS_ASYNC_INVALID;
    if (!__sync_bool_compare_and_swap(&worker->initialized,SERVICE_NEW,SERVICE_STARTING))
        return __atomic_load_n(&worker->initialized,__ATOMIC_ACQUIRE)==SERVICE_READY?
            FS_ASYNC_ACCEPTED:FS_ASYNC_INVALID;
    worker->service=*service;
    if (FsAsyncBroker_Initialize(&worker->broker,body,bodyCapacity)!=FS_ASYNC_ACCEPTED ||
        !NativeWorker_Start(&worker->platform,serviceMain,worker,stack,stackSize)) {
        __atomic_store_n(&worker->initialized,SERVICE_FAILED,__ATOMIC_RELEASE);
        return FS_ASYNC_INVALID;
    }
    __atomic_store_n(&worker->initialized,SERVICE_READY,__ATOMIC_RELEASE);
    return FS_ASYNC_ACCEPTED;
}

static int ready(const NativeServiceWorker *worker)
{
    return worker && !((uintptr_t)worker%_Alignof(NativeServiceWorker)) &&
        __atomic_load_n(&worker->initialized,__ATOMIC_ACQUIRE)==SERVICE_READY;
}

int NativeService_Accept(NativeServiceWorker *worker,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *input,uint32_t *identity)
{
    return ready(worker)?FsAsyncBroker_Accept(&worker->broker,peer,request,input,identity):FS_ASYNC_INVALID;
}

int NativeService_Poll(NativeServiceWorker *worker,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *input,const void *body,uint32_t bodySize,FsAsyncDelivery *delivery)
{
    return NativeService_PollParts(worker,peer,request,input,body,bodySize,0,0,delivery);
}

int NativeService_PollParts(NativeServiceWorker *worker,uintptr_t peer,uintptr_t request,
    const FsServiceRequest *input,const void *body,uint32_t bodySize,
    const void *appendInput,uint32_t appendSize,FsAsyncDelivery *delivery)
{
    if (!ready(worker) || !input || (uintptr_t)input%_Alignof(FsServiceRequest)) return FS_ASYNC_INVALID;
    uint32_t identity=FsAsyncBroker_Find(&worker->broker,peer,request,input->operation);
    if (!identity) return FS_ASYNC_STALE;
    int prepared=FsAsyncBroker_Prepare(&worker->broker,identity,input);
    if (prepared!=FS_ASYNC_ACCEPTED && prepared!=FS_ASYNC_BUSY) return prepared;
    /* Retry a failed wake's queued failure if its short task lock was busy.
       Never leave a failed platform request waiting forever or run it on UI. */
    /* 唤醒失败时，若短任务锁占用，下次重试排队失败投递；不永久等待，不改在界面执行。 */
    if (__atomic_load_n(&worker->platform.state,__ATOMIC_ACQUIRE)==NATIVE_WORKER_FAILED)
        (void)FsAsyncTask_FailQueued(&worker->broker.task,FS_IO_ERROR);
    int result=FsAsyncBroker_PollParts(&worker->broker,identity,FsService_Execute,&worker->service,
        body,bodySize,appendInput,appendSize,delivery);
    if (result==FS_ASYNC_ACCEPTED && !NativeWorker_Wake(&worker->platform)) platformFailure(worker);
    return result;
}

int NativeService_Release(NativeServiceWorker *worker,const FsAsyncDelivery *delivery)
{
    return ready(worker)?FsAsyncBroker_Release(&worker->broker,delivery):FS_ASYNC_STALE;
}

int NativeService_CancelPeer(NativeServiceWorker *worker,uintptr_t peer)
{
    return ready(worker)?FsAsyncBroker_CancelPeer(&worker->broker,peer):FS_ASYNC_STALE;
}

int NativeService_CancelRequest(NativeServiceWorker *worker,uintptr_t request)
{
    return ready(worker)?FsAsyncBroker_CancelRequest(&worker->broker,request):FS_ASYNC_STALE;
}

int NativeService_Lock(NativeServiceWorker *worker,uint32_t wait)
{
    if (!ready(worker) ||
        __atomic_load_n(&worker->platform.state,__ATOMIC_ACQUIRE)==NATIVE_WORKER_FAILED)
        return FS_ASYNC_INVALID;
    if (!NativeWorker_LockService(&worker->platform,wait)) return FS_ASYNC_BUSY;
    if (__atomic_load_n(&worker->platform.state,__ATOMIC_ACQUIRE)==NATIVE_WORKER_FAILED) {
        (void)NativeWorker_UnlockService(&worker->platform);
        return FS_ASYNC_INVALID;
    }
    return FS_ASYNC_ACCEPTED;
}

int NativeService_Unlock(NativeServiceWorker *worker)
{
    if (!ready(worker)) return 0;
    if (!NativeWorker_UnlockService(&worker->platform)) {
        /* A failed UI/legacy release must not leave a queued request waiting
           on an apparently healthy worker forever. Running effects keep
           ownership; only queued work is failed by platformFailure. */
        /* 界面／旧入口释放失败不能让排队请求永等；已运行的操作仍持有其结果，
           platformFailure 只使尚未执行的排队任务失败。 */
        platformFailure(worker);
        return 0;
    }
    return 1;
}
