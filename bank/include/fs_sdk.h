#ifndef BANK_OFFLINE_PATCH_FS_SDK_H
#define BANK_OFFLINE_PATCH_FS_SDK_H

/* Bank SDK address map, consumed through the shared fs_helpers.h. */
/* Bank SDK 地址表，由共用 fs_helpers.h 引用。 */
#define FSUSER_HandleSlot ((volatile u32 *)0x00390100u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x0022D338u)
#define FSFILE_Read ((FSFILE_ReadFn)0x001658C8u)
#define FSFILE_Close ((FSFILE_CloseFn)0x00165920u)
#define FSFILE_Write ((FSFILE_WriteFn)0x0016594Cu)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001659ACu)
#define SRV_GetServiceHandle ((SRV_GetServiceHandleFn)0x0023514Cu)
#define RUNTIME_ENVIRONMENT_STORAGE_ADDRESS 0x003FAFE0u
/* Heap lookup and paired automatic release; allocation uses its nonfatal raw
   virtual entry, not the public allocator's infinite OOM handler. */
/* 堆查找及配对自动释放；分配使用可返回失败的底层虚函数，不进入 OOM 无限等待。 */
#define FsHeap_Get ((void *(*)(u32))0x00235BFCu)
#define FsHeap_Release ((void (*)(void *))0x00236228u)

/* Native local SDK runtime; these entries do not initiate authentication. */
/* 原 SDK 本地运行上下文入口，不发起身份验证或联网连接。 */
#define NativeSdk_GetRuntime ((NativeSdkRuntimeGetFn)0x002296B4u)
#define NativeSdk_AcquireRuntime ((NativeSdkRuntimeAcquireFn)0x0021A1D8u)
#define NativeSdk_Complete ((NativeSdkCompleteFn)0x0021F544u)
#define NativeSdk_RpcTextEncoding ((volatile const u32 *)0x003C5D90u)

/* Checked low-level SDK thread creation reports errors rather than invoking
   the higher-level fatal-error path; its trampoline initializes TLS/IPC. */
/* 已复核的底层 SDK 建线程入口返回错误，不进入高层 fatal 分支；跳板初始化 TLS／IPC。 */
#define NativeWorker_CreateThread ((NativeWorkerCreateFn)0x002320D4u)
#define NativeWorker_CopyArgument 0x0012117Cu
#define NativeWorker_CleanupArgument 0x00121198u
#define NativeWorker_InvokeArgument 0x0012118Cu

#endif
