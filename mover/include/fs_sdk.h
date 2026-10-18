#ifndef MOVER_OFFLINE_PATCH_FS_SDK_H
#define MOVER_OFFLINE_PATCH_FS_SDK_H

/* Mover SDK address map, consumed through the shared fs_helpers.h. */
/* Mover SDK 地址表，由共用 fs_helpers.h 引用。 */
#define FSUSER_HandleSlot ((volatile u32 *)0x00311F80u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x001DF448u)
#define FSFILE_Read ((FSFILE_ReadFn)0x0015930Cu)
#define FSFILE_Close ((FSFILE_CloseFn)0x00159364u)
#define FSFILE_Write ((FSFILE_WriteFn)0x00159390u)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001593F0u)
#define SRV_GetServiceHandle ((SRV_GetServiceHandleFn)0x001E596Cu)
#define RUNTIME_ENVIRONMENT_STORAGE_ADDRESS 0x00363FE0u
/* Heap lookup and paired automatic release; allocation uses its nonfatal raw
   virtual entry, not the public allocator's infinite OOM handler. */
/* 堆查找及配对自动释放；分配使用可返回失败的底层虚函数，不进入 OOM 无限等待。 */
#define FsHeap_Get ((void *(*)(u32))0x001E6394u)
#define FsHeap_Release ((void (*)(void *))0x001E6C00u)

/* Independently checked Mover entries; never reuse Bank addresses. */
/* 独立复核的 Mover 入口，不使用 Bank 地址。 */
#define NativeSdk_GetRuntime ((NativeSdkRuntimeGetFn)0x001DBD24u)
#define NativeSdk_AcquireRuntime ((NativeSdkRuntimeAcquireFn)0x001DAF2Cu)
#define NativeSdk_Complete ((NativeSdkCompleteFn)0x001D2214u)
#define NativeSdk_RpcTextEncoding ((volatile const u32 *)0x00343778u)

/* Independently checked thread/TLS entries; not Bank's SDK addresses. */
/* 独立复核的线程／TLS 入口，不套用 Bank 的 SDK 地址。 */
#define NativeWorker_CreateThread ((NativeWorkerCreateFn)0x001E3F58u)
#define NativeWorker_CopyArgument 0x0011F098u
#define NativeWorker_CleanupArgument 0x0011F0B4u
#define NativeWorker_InvokeArgument 0x0011F0A8u

#endif
