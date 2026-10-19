#include "fakeServer/async_task.h"
#include "fs_system/diagnostic_log.h"
#include "fakeServer/service_request.h"
#include <string.h>

_Static_assert(sizeof(FsAsyncTask)<0x1000u,"task control must fit its reserved page");

static void copy(void *destination,const void *source,uint32_t size)
{
    memcpy(destination,source,size);
}

static int lock(FsAsyncTask *task)
{
    return task && !__sync_lock_test_and_set(&task->lock,1);
}

static void unlock(FsAsyncTask *task)
{
    __sync_lock_release(&task->lock);
}

static int matches(const FsAsyncTask *task,const FsAsyncToken *token)
{
    return token && token->owner && task->state!=FS_ASYNC_IDLE &&
        token->owner==task->owner && token->generation==task->generation;
}

static int cancelled(const FsAsyncTask *task)
{
    return __atomic_load_n(&task->cancelledGeneration,__ATOMIC_ACQUIRE)>=task->generation;
}

static void requestCancel(FsAsyncTask *task,uint32_t generation)
{
    uint32_t previous=__atomic_load_n(&task->cancelledGeneration,__ATOMIC_ACQUIRE);
    while (previous<generation && !__atomic_compare_exchange_n(&task->cancelledGeneration,
        &previous,generation,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) { }
}

static int cancelWithoutLock(FsAsyncTask *task,uintptr_t owner,uint32_t generation)
{
    uint32_t observed=__atomic_load_n(&task->generation,__ATOMIC_ACQUIRE);
    if (!owner || (generation && generation!=observed) || !observed ||
        __atomic_load_n(&task->owner,__ATOMIC_ACQUIRE)!=owner ||
        __atomic_load_n(&task->state,__ATOMIC_ACQUIRE)==FS_ASYNC_IDLE) return FS_ASYNC_STALE;
    /* Cancellation survives a briefly held UI/result lock. Recording an old
       generation is harmless; the monotonic marker cannot cancel a newer job. */
    /* 短暂锁占用不能丢失析构取消；旧代标记不会取消后续新作业，标记单调且不回退。 */
    requestCancel(task,observed);
    return FS_ASYNC_ACCEPTED;
}

static void retire(FsAsyncTask *task)
{
    __atomic_store_n(&task->state,FS_ASYNC_IDLE,__ATOMIC_RELEASE);
    __atomic_store_n(&task->owner,0,__ATOMIC_RELEASE);
    task->work=0;
    task->context=0;
    task->inputSize=task->outputSize=task->bodySize=0;
}

int FsAsyncTask_Initialize(FsAsyncTask *task,void *body,uint32_t capacity)
{
    if (!task || (uintptr_t)task%_Alignof(FsAsyncTask) || (!body && capacity)) return FS_ASYNC_INVALID;
    uint8_t *bytes=(uint8_t *)task;
    for (uint32_t i=0;i<sizeof(*task);i++) bytes[i]=0;
    task->body=body;
    task->bodyCapacity=capacity;
    return FS_ASYNC_ACCEPTED;
}

int FsAsyncTask_Submit(FsAsyncTask *task,uintptr_t owner,FsAsyncWork work,void *context,
    const void *input,uint32_t inputSize,const void *bodyInput,uint32_t bodySize,FsAsyncToken *token)
{
    return FsAsyncTask_SubmitParts(task,owner,work,context,input,inputSize,bodyInput,
        bodySize,0,0,token);
}

int FsAsyncTask_SubmitParts(FsAsyncTask *task,uintptr_t owner,FsAsyncWork work,void *context,
    const void *input,uint32_t inputSize,const void *bodyInput,uint32_t bodySize,
    const void *appendInput,uint32_t appendSize,FsAsyncToken *token)
{
    if (!task || !owner || !work || !token || inputSize>FS_ASYNC_SMALL_CAPACITY ||
        (!input && inputSize) || bodySize>task->bodyCapacity ||
        appendSize>task->bodyCapacity-bodySize || (!appendInput && appendSize) ||
        (!task->body && (bodySize || appendSize)))
        return FS_ASYNC_INVALID;
    if (!lock(task)) return FS_ASYNC_BUSY;
    int result=FS_ASYNC_BUSY;
    if (task->state==FS_ASYNC_IDLE) {
        if (task->generation==0xffffffffu) result=FS_ASYNC_INVALID;
        else {
            /* Publish the new lifetime before its generation. Otherwise an
               old-owner cancellation could sample the new generation with
               the previous owner, then cancel a later QUEUED publication. */
            /* 先写新生命周期标识再写代号，避免旧 owner 的取消读到新代号配旧 owner，
               在随后观察到 QUEUED 时错误取消新请求。 */
            __atomic_store_n(&task->owner,owner,__ATOMIC_RELEASE);
            __atomic_store_n(&task->generation,task->generation+1,__ATOMIC_RELEASE);
            task->work=work;
            task->context=context;
            task->inputSize=inputSize;
            task->bodySize=bodySize+appendSize;
            task->outputSize=0;
            task->result=0;
            for (uint32_t i=0;i<sizeof(task->output);i++) task->output[i]=0;
            if (inputSize) copy(task->input,input,inputSize);
            if (bodyInput && bodySize) copy(task->body,bodyInput,bodySize);
            if (appendSize) copy(task->body+bodySize,appendInput,appendSize);
            token->owner=owner;
            token->generation=task->generation;
            __atomic_store_n(&task->state,FS_ASYNC_QUEUED,__ATOMIC_RELEASE);
            result=FS_ASYNC_ACCEPTED;
        }
    }
    unlock(task);
    return result;
}

int FsAsyncTask_Run(FsAsyncTask *task)
{
    if (!task) return FS_ASYNC_INVALID;
    /* A consumed wake must not lose a queued job just because UI polling held
       the tiny result lock. Only this worker-side entry waits for that lock. */
    /* 不能因界面轮询短暂占锁而消耗唤醒却遗失排队任务；仅工作线程入口等待此锁。 */
    while (!lock(task)) { }
    if (task->state!=FS_ASYNC_QUEUED) { unlock(task); return FS_ASYNC_BUSY; }
    if (cancelled(task)) { retire(task); unlock(task); return FS_ASYNC_READY; }
    __atomic_store_n(&task->state,FS_ASYNC_RUNNING,__ATOMIC_RELEASE);
    unlock(task);

    uint32_t size=0;
    FsDiag_Checkpoint("request.begin",0,
        task->inputSize==sizeof(FsServiceRequest)?((const FsServiceRequest *)task->input)->operation:0,
        task->bodySize,task->generation);
    FsDiag_Flush();
    int result=task->work(task->context,task->input,task->inputSize,task->output,
        sizeof(task->output),&size,task->body,task->bodySize);
    FsDiag_Checkpoint("request.end",result,
        task->inputSize==sizeof(FsServiceRequest)?((const FsServiceRequest *)task->input)->operation:0,
        size,task->generation);
    FsDiag_Flush();
    /* Only the worker may publish DONE. Other callers only alter cancelled;
       neither cancellation nor a new submission can recycle RUNNING storage. */
    /* 仅工作线程发布 DONE；其他调用方只能标记取消，不能回收 RUNNING 的存储。 */
    while (!lock(task)) { }
    if (cancelled(task)) retire(task);
    else {
        task->outputSize=size<=sizeof(task->output)?size:0;
        task->result=size<=sizeof(task->output)?result:FS_ASYNC_INVALID;
        __atomic_store_n(&task->state,FS_ASYNC_DONE,__ATOMIC_RELEASE);
    }
    unlock(task);
    return FS_ASYNC_READY;
}

int FsAsyncTask_FailQueued(FsAsyncTask *task,int result)
{
    if (!task || result>=0) return FS_ASYNC_INVALID;
    if (!lock(task)) return FS_ASYNC_BUSY;
    int status=FS_ASYNC_BUSY;
    if (task->state==FS_ASYNC_QUEUED) {
        if (cancelled(task)) retire(task);
        else {
            task->result=result;
            task->outputSize=0;
            __atomic_store_n(&task->state,FS_ASYNC_DONE,__ATOMIC_RELEASE);
        }
        status=FS_ASYNC_READY;
    }
    unlock(task);
    return status;
}

int FsAsyncTask_Take(FsAsyncTask *task,const FsAsyncToken *token,void *output,uint32_t capacity,
    uint32_t *outputSize,int *result,const void **body,uint32_t *bodySize)
{
    if (!task || !token || !outputSize || !result || !body || !bodySize)
        return FS_ASYNC_INVALID;
    if (!lock(task)) return FS_ASYNC_BUSY;
    int status=FS_ASYNC_STALE;
    if (matches(task,token)) {
        status=FS_ASYNC_BUSY;
        if (task->state==FS_ASYNC_DONE) {
            if (cancelled(task)) { retire(task); status=FS_ASYNC_STALE; }
            else if (task->outputSize>capacity || (!output && task->outputSize)) status=FS_ASYNC_INVALID;
            else {
                if (task->outputSize) copy(output,task->output,task->outputSize);
                *outputSize=task->outputSize;
                *result=task->result;
                *body=task->body;
                *bodySize=task->bodySize;
                __atomic_store_n(&task->state,FS_ASYNC_DELIVERING,__ATOMIC_RELEASE);
                status=FS_ASYNC_READY;
            }
        }
    }
    unlock(task);
    return status;
}

int FsAsyncTask_Release(FsAsyncTask *task,const FsAsyncToken *token)
{
    if (!task || !token) return FS_ASYNC_INVALID;
    if (!lock(task)) return FS_ASYNC_BUSY;
    int status=FS_ASYNC_STALE;
    if (matches(task,token) && task->state==FS_ASYNC_DELIVERING) {
        status=cancelled(task)?FS_ASYNC_STALE:FS_ASYNC_ACCEPTED;
        retire(task);
    }
    unlock(task);
    return status;
}

static void cancel(FsAsyncTask *task)
{
    requestCancel(task,task->generation);
    if (task->state!=FS_ASYNC_RUNNING && task->state!=FS_ASYNC_DELIVERING) retire(task);
}

int FsAsyncTask_Cancel(FsAsyncTask *task,const FsAsyncToken *token)
{
    if (!task || !token) return FS_ASYNC_INVALID;
    if (!lock(task)) return cancelWithoutLock(task,token->owner,token->generation);
    int status=FS_ASYNC_STALE;
    if (matches(task,token)) { cancel(task); status=FS_ASYNC_ACCEPTED; }
    unlock(task);
    return status;
}

int FsAsyncTask_CancelOwner(FsAsyncTask *task,uintptr_t owner)
{
    if (!task || !owner) return FS_ASYNC_INVALID;
    if (!lock(task)) return cancelWithoutLock(task,owner,0);
    int status=FS_ASYNC_STALE;
    if (task->state!=FS_ASYNC_IDLE && task->owner==owner) {
        cancel(task); status=FS_ASYNC_ACCEPTED;
    }
    unlock(task);
    return status;
}
