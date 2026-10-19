#include "fs_system/native_worker.h"
#include "fs_sdk.h"
#include "fs_system/diagnostic_log.h"

_Static_assert(sizeof(void *)==4,"SDK worker argument ABI is ARM32");

static int createEvent(uint32_t *handle)
{
    register int32_t result __asm__("r0")=0;
    register uint32_t event __asm__("r1")=0;
    __asm__ volatile("svc 0x17" : "+r"(result),"+r"(event) : : "r2","r3","r12","cc","memory");
    if (result<0) return 0;
    *handle=event;
    return event!=0;
}

static int createMutex(uint32_t *handle)
{
    register int32_t result __asm__("r0")=0;
    register uint32_t mutex __asm__("r1")=0;
    __asm__ volatile("svc 0x13" : "+r"(result),"+r"(mutex) : : "r2","r3","r12","cc","memory");
    if (result<0) return 0;
    *handle=mutex;
    return mutex!=0;
}

static void closeHandle(uint32_t handle)
{
    if (!handle) return;
    register uint32_t value __asm__("r0")=handle;
    __asm__ volatile("svc 0x23" : "+r"(value) : : "r1","r2","r3","r12","cc","memory");
}

static int waitHandle(uint32_t handle,uint32_t wait)
{
    register int32_t result __asm__("r0")=(int32_t)handle;
    register uint32_t low __asm__("r2")=wait?0xffffffffu:0;
    register uint32_t high __asm__("r3")=low;
    __asm__ volatile("svc 0x24" : "+r"(result),"+r"(low),"+r"(high) : : "r1","r12","cc","memory");
    return result==0;
}

int NativeWorker_Start(NativeWorker *worker,NativeWorkerEntry entry,void *argument,
    void *stack,uint32_t stackSize)
{
    if (!worker || !entry || !stack || stackSize<0x4000u ||
        ((uintptr_t)stack&7u) || (stackSize&7u) || (uintptr_t)stack+stackSize<(uintptr_t)stack)
        return 0;
    if (!__sync_bool_compare_and_swap(&worker->state,NATIVE_WORKER_NEW,NATIVE_WORKER_STARTING))
        return worker->state==NATIVE_WORKER_READY;
    if (!createEvent(&worker->wake) || !createMutex(&worker->serviceMutex)) goto failure;
    /* The SDK copies this one argument word before CreateThread returns. Its
       original no-op cleanup and call adapter are retained independently per title. */
    /* SDK 在 CreateThread 返回前复制一个参数字；分别使用两端原有的空清理及调用适配入口。 */
    const uint32_t descriptor[4]={4,NativeWorker_CopyArgument,NativeWorker_CleanupArgument,
        NativeWorker_InvokeArgument};
    void *copiedArgument=argument;
    int32_t status=NativeWorker_CreateThread(worker->thread,descriptor,entry,&copiedArgument,
        /* Both independently checked SDK creators translate parameter 0x18
           to kernel priority 0x38. Passing raw 0x38 here maps to an invalid
           priority, not a lower-priority thread. */
        /* 两端独立复核的 SDK 均把参数 0x18 转为内核优先级 0x38；
           此处直接填 0x38 会转换成无效值，不能混用 SDK 与内核参数。 */
        (uint32_t)((uintptr_t)stack+stackSize),0x18,-2,0);
    FsDiag_Record("worker.create",status,stackSize,worker->thread[0],0);
    if (status<0 || !worker->thread[0]) goto failure;
    __sync_synchronize();
    worker->state=NATIVE_WORKER_READY;
    return 1;
failure:
    closeHandle(worker->serviceMutex);
    closeHandle(worker->wake);
    worker->serviceMutex=worker->wake=0;
    __sync_synchronize();
    worker->state=NATIVE_WORKER_FAILED;
    return 0;
}

int NativeWorker_Wake(NativeWorker *worker)
{
    if (!worker || worker->state!=NATIVE_WORKER_READY) return 0;
    register int32_t result __asm__("r0")=(int32_t)worker->wake;
    __asm__ volatile("svc 0x18" : "+r"(result) : : "r1","r2","r3","r12","cc","memory");
    return result==0;
}

int NativeWorker_Wait(NativeWorker *worker)
{
    if (!worker || !worker->wake) return 0;
    return waitHandle(worker->wake,1);
}

int NativeWorker_LockService(NativeWorker *worker,uint32_t wait)
{
    return worker && worker->serviceMutex && waitHandle(worker->serviceMutex,wait);
}

int NativeWorker_UnlockService(NativeWorker *worker)
{
    if (!worker || !worker->serviceMutex) return 0;
    register int32_t result __asm__("r0")=(int32_t)worker->serviceMutex;
    __asm__ volatile("svc 0x14" : "+r"(result) : : "r1","r2","r3","r12","cc","memory");
    return result==0;
}
