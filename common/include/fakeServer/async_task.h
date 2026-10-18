#ifndef COMMON_FAKE_SERVER_ASYNC_TASK_H
#define COMMON_FAKE_SERVER_ASYNC_TASK_H

#include <stdint.h>

#define FS_ASYNC_SMALL_CAPACITY 128u

enum FsAsyncResult {
    FS_ASYNC_INVALID=-1,
    FS_ASYNC_STALE=-2,
    FS_ASYNC_BUSY=0,
    FS_ASYNC_ACCEPTED=1,
    FS_ASYNC_READY=2
};

enum FsAsyncState {
    FS_ASYNC_IDLE,
    FS_ASYNC_QUEUED,
    FS_ASYNC_RUNNING,
    FS_ASYNC_DONE,
    FS_ASYNC_DELIVERING
};

typedef struct FsAsyncToken {
    uintptr_t owner;
    uint32_t generation;
} FsAsyncToken;

/* This callback runs only on the serialized service worker. Input, output and
   body belong to the task, not an SDK/UI object; it must obey both capacities. */
/* 回调仅在串行服务线程运行。输入、输出和正文归任务所有，不借用 SDK／界面对象；
   回调必须遵守输出与正文的容量限制。 */
typedef int (*FsAsyncWork)(void *context,const void *input,uint32_t inputSize,
    void *output,uint32_t outputCapacity,uint32_t *outputSize,void *body,uint32_t bodySize);

typedef struct FsAsyncTask {
    volatile uint32_t lock;
    uint32_t state;
    uint32_t generation;
    uintptr_t owner;
    volatile uint32_t cancelledGeneration;
    FsAsyncWork work;
    void *context;
    uint8_t *body;
    uint32_t bodyCapacity;
    uint32_t bodySize;
    uint32_t inputSize;
    uint32_t outputSize;
    int result;
    _Alignas(8) uint8_t input[FS_ASYNC_SMALL_CAPACITY];
    _Alignas(8) uint8_t output[FS_ASYNC_SMALL_CAPACITY];
} FsAsyncTask;

/* Initialize once, before starting the worker. Storage and body must remain
   alive until process exit or a proved worker shutdown, never a UI destructor. */
/* 仅在线程启动前初始化一次。控制块和正文保持到进程退出／确认线程已结束，
   不随界面析构释放，不在未决任务上重新初始化。 */
int FsAsyncTask_Initialize(FsAsyncTask *,void *body,uint32_t capacity);

/* All UI-side methods are nonblocking. bodyInput==NULL reserves an output
   body without copying old bytes. Tokens are never reused. owner identifies an
   object lifetime, not just an address that a later SDK allocation may reuse. */
/* 所有界面侧方法均不阻塞。bodyInput 为 NULL 时仅预留输出正文；令牌不重用。
   owner 标识对象的一次生命周期，不能仅使用之后可能被 SDK 分配器重用的地址。 */
int FsAsyncTask_Submit(FsAsyncTask *,uintptr_t owner,FsAsyncWork,void *context,
    const void *input,uint32_t inputSize,const void *bodyInput,uint32_t bodySize,FsAsyncToken *);
/* Copy both pieces before QUEUED/wake publication. Used for Bank + NDS path;
   the second piece must never be appended after the worker can start. */
/* 两段正文均在排队及唤醒前复制；用于银行正文＋NDS 路径，不允许后台启动后再补路径。 */
int FsAsyncTask_SubmitParts(FsAsyncTask *,uintptr_t owner,FsAsyncWork,void *context,
    const void *input,uint32_t inputSize,const void *bodyInput,uint32_t bodySize,
    const void *appendInput,uint32_t appendSize,FsAsyncToken *);
/* Worker-only: unlike the UI methods it waits for the short control lock so
   that an already consumed kernel wake cannot strand a queued task. */
/* 仅工作线程调用；允许等待短控制锁，防止已消耗内核唤醒后遗失排队任务。 */
int FsAsyncTask_Run(FsAsyncTask *);
/* Fail a queued task when the platform cannot execute it; a running operation
   cannot be declared failed by another thread or rolled back implicitly. */
/* 平台无法执行时可将排队任务返回失败；不能由其他线程宣告正在运行的操作失败或擅自回滚。 */
int FsAsyncTask_FailQueued(FsAsyncTask *,int result);

/* Taking a result pins the body until Release. Cancellation cannot overwrite
   an input being executed or a result already being copied by its consumer. */
/* 取得结果后正文保持到 Release；取消不能覆盖执行中的输入或消费者正在复制的结果。 */
int FsAsyncTask_Take(FsAsyncTask *,const FsAsyncToken *,void *output,uint32_t capacity,
    uint32_t *outputSize,int *result,const void **body,uint32_t *bodySize);
int FsAsyncTask_Release(FsAsyncTask *,const FsAsyncToken *);
int FsAsyncTask_Cancel(FsAsyncTask *,const FsAsyncToken *);
int FsAsyncTask_CancelOwner(FsAsyncTask *,uintptr_t owner);

#endif
