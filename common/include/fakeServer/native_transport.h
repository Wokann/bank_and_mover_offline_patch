#ifndef COMMON_NATIVE_SERVER_TRANSPORT_H
#define COMMON_NATIVE_SERVER_TRANSPORT_H

#include "fs_system/service_worker.h"

/* The complete runtime is process-owned, outside both native BSS and SDK
   objects. Only the UI thread accesses the request registry. */
/* 完整运行区归进程所有，独立于原 BSS 及 SDK 对象；登记表仅由界面线程访问。 */
int NativeTransport_Start(void);
int NativeTransport_Accept(uintptr_t peer,uintptr_t request,const FsServiceRequest *);
int NativeTransport_Poll(uintptr_t peer,uintptr_t request,const FsServiceRequest *,
    const void *body,uint32_t size,FsAsyncDelivery *);
int NativeTransport_PollParts(uintptr_t peer,uintptr_t request,const FsServiceRequest *,
    const void *body,uint32_t size,const void *appendInput,uint32_t appendSize,FsAsyncDelivery *);
int NativeTransport_Release(const FsAsyncDelivery *);
int NativeTransport_Request(uintptr_t peer,uintptr_t request,FsServiceRequest *);
int NativeTransport_Submitted(uintptr_t peer,uintptr_t request);
/* Bound each UI copy without lending SDK-owned storage to a running worker.
   The delivery remains pinned across frames and is released before callback. */
/* 每帧限量交付，不把 SDK 缓冲借给后台；跨帧固定响应，回调前释放。 */
int NativeTransport_CopyReply(uintptr_t peer,const FsAsyncDelivery *,void *,uint32_t size,uint32_t budget);
/* Auto-register one original wait, then poll its immutable owned task. The
   caller consumes/releases READY once before advancing its native phase. */
/* 为一次原等待登记并轮询自持副本；调用者消费及释放 READY 后才能推进原阶段。 */
int NativeTransport_PollRequest(uintptr_t peer,uintptr_t request,const FsServiceRequest *,
    const void *body,uint32_t size,FsAsyncDelivery *);
int NativeTransport_PollSource(uintptr_t peer,uintptr_t request,uint32_t operation,
    const FsSource *,uint32_t nativeResult,FsAsyncDelivery *);
void NativeTransport_Cancel(uintptr_t peer);
NativeTicketUi *NativeTransport_TicketUi(uintptr_t job,uintptr_t provider);
/* State-owned control acknowledgements. A completed tag is remembered until
   state exit so a native WAIT does not repeat a completed disk publication.
   Resume gates post-save observations before the next native update. */
/* 原状态持有的控制确认；结束前记住已完成标签，避免原等待反复发布。
   Resume 在下一次原更新前确认保存成功后的观察请求，不重复 SavePoll。 */
int NativeTransport_StateRequest(uintptr_t peer,uintptr_t tag,const FsServiceRequest *,int *result);
int NativeTransport_StatePending(uintptr_t peer,int *result);

/* Legacy service callers share the worker mutex. UI entry points try it once;
   existing SDK workers may wait. Cancellation never takes this mutex. */
/* 既有服务调用共用后台互斥量；界面入口仅尝试一次，原 SDK 工作线程可等待；取消不取锁。 */
int NativeTransport_Lock(uint32_t wait);
void NativeTransport_Unlock(void);
typedef struct NativeTransportScope { int status; } NativeTransportScope;
void NativeTransport_Leave(NativeTransportScope *);

#endif
