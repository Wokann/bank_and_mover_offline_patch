#ifndef COMMON_FS_NATIVE_WORKER_H
#define COMMON_FS_NATIVE_WORKER_H

#include <stdint.h>

enum NativeWorkerState { NATIVE_WORKER_NEW,NATIVE_WORKER_STARTING,NATIVE_WORKER_READY,NATIVE_WORKER_FAILED };

typedef struct NativeWorker {
    volatile uint32_t state;
    uint32_t thread[2];
    uint32_t wake;
    uint32_t serviceMutex;
} NativeWorker;

typedef void (*NativeWorkerEntry)(void *);
typedef int32_t (*NativeWorkerCreateFn)(uint32_t *thread,const uint32_t *descriptor,
    NativeWorkerEntry,const void *argument,uint32_t stackTop,uint32_t priority,int32_t affinity,uint32_t cleanup);

/* All storage and the aligned stack remain owned until process exit. Start
   uses the checked SDK trampoline for TLS/IPC initialization, not bare SVC 8. */
/* 存储和对齐栈保持到进程退出；启动使用已复核的 SDK 跳板初始化 TLS／IPC，
   不直接以裸 SVC 8 运行服务代码。 */
int NativeWorker_Start(NativeWorker *,NativeWorkerEntry,void *,void *stack,uint32_t stackSize);
int NativeWorker_Wake(NativeWorker *);
int NativeWorker_Wait(NativeWorker *);
/* wait==0 is a nonblocking UI attempt; only worker threads may pass wait==1. */
/* wait 为 0 时是界面侧非阻塞尝试；仅工作线程可以传入 wait 为 1。 */
int NativeWorker_LockService(NativeWorker *,uint32_t wait);
int NativeWorker_UnlockService(NativeWorker *);

#endif
