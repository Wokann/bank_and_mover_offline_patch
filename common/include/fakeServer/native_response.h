#ifndef COMMON_NATIVE_SERVER_RESPONSE_H
#define COMMON_NATIVE_SERVER_RESPONSE_H

#include "patch_types.h"
#include "types.h"
#include "fakeserverapi.h"
#include "native_transport.h"

/* Views shared by the independently checked Bank/Mover remote objects. These
   are transport/SDK containers, not client state-machine objects. */
/* 两端独立复核后相同的远端对象视图；仅描述传输／SDK 容器，不描述业务状态机。 */
typedef struct NativeRemoteDescriptor {
    void *vtable;
    u32 reserved04;
    u32 dataId[2];
    u32 currentVersion;
    u32 updateVersion;
    u32 size;
    u32 reserved1C;
    u32 token[2];
} NativeRemoteDescriptor;

typedef struct NativeRemoteView {
    void *vtable;
    u8 initialized;
    u8 busy;
    u8 reserved06[2];
    void *callback;
    u32 reserved0C;
    void *provider;
    void *request;
    u32 allocator;
    u8 transportMode;
    u8 reserved1D[3];
    u32 replyMode;
    NativeRemoteDescriptor *descriptor;
    u32 createdDataId[2];
    void *upload;
    void *download;
    void *metadata;
    void *tokens;
    u32 transactionOwner;
    u32 fileStatus;
    void *support;
} NativeRemoteView;

typedef struct NativeRemoteResult {
    u8 reserved00[0x10];
    u8 state;
    u8 reserved11[0x27];
    u32 result[3];
} NativeRemoteResult;

typedef struct NativeMetadataIdView {
    void *vtable;
    u32 reserved04;
    u32 dataId[2];
} NativeMetadataIdView;

/* Query replies expose a byte vector at +0x34, unlike the stream used by
   metadata-update requests. The original SDK object remains its owner. */
/* 查询响应使用 +0x34 的字节向量，不是更新请求使用的流；对象仍归原 SDK 管理。 */
typedef struct NativeMetadataReplyView {
    NativeMetadataIdView identity;
    u8 reserved10[0x20];
    const u8 *allocation;
    const u8 *begin;
    const u8 *end;
    const u8 *capacity;
} NativeMetadataReplyView;

typedef struct NativeSupportReplyView {
    void *vtable;
    u32 code;
} NativeSupportReplyView;

typedef struct NativeRecoveryTokensView {
    u32 *begin;
    u32 *end;
    u32 *capacity;
} NativeRecoveryTokensView;

typedef struct NativeDownloadReplyView {
    void *vtable;
    u8 *begin;
    u8 *end;
    u8 *capacity;
    u32 length;
} NativeDownloadReplyView;

typedef struct NativeUploadView {
    void *vtable;
    const u8 *begin;
    const u8 *end;
    const u8 *capacity;
} NativeUploadView;

typedef void *(*NativeResponseAllocateFn)(u32);
typedef void (*NativeResponseReleaseFn)(void *);

/* The offline system-session carrier occupies the native allocation. Only
   its destructor and error field are consumed by the original client states;
   no NNID/system applet is constructed for a local service connection. */
/* 离线系统会话载体沿用原分配尺寸；原状态仅消费析构和错误字段。
   本地服务连接不创建真实 NNID／系统联网小程序。 */
typedef struct NativeConnectionJob {
    void *vtable;
    NativeResponseReleaseFn release;
    FsConnectionResponse reply;
    u8 reserved10[0x25D];
    u8 kind;
    u8 reserved26E[0x0E];
    u32 errorCode;
    u8 reserved280[0x30];
} NativeConnectionJob;
void *NativeResponse_CreateConnection(void *,u32,NativeResponseReleaseFn);
u32 NativeResponse_PollConnection(NativeConnectionJob *);
/* One-shot local Mover disconnect reason; never serialized or sent online. */
/* 离线 Mover 断开提示的一次性原因；不落盘，也不发送到真实服务器。 */
u32 NativeResponse_TakeMissingBankNotice(void);
u32 NativeResponse_DisconnectMinimumDelay(void);
void NativeResponse_ClearMissingBankNotice(void);

typedef int (*NativeRemoteBindFn)(NativeRemoteView *,const void *);
typedef void (*NativeRemoteResultBuildFn)(u32 *,const u8 *);
typedef void (*NativeRemoteResponseFn)(NativeRemoteView *,const NativeRemoteResult *);
typedef void (*NativeSdkCompleteFn)(NativeRemoteResult *,u32,const u32 *,u32);

/* The native SDK context owns synchronization and outstanding requests. */
/* 原 SDK 上下文拥有同步对象及未完成请求，不由伪服务器重新构造。 */
typedef struct NativeSdkRuntimeView {
    u8 reserved00[0x0C];
    void *scheduler;
    void *requests;
} NativeSdkRuntimeView;
typedef NativeSdkRuntimeView *(*NativeSdkRuntimeGetFn)(void);
typedef void (*NativeSdkRuntimeAcquireFn)(void);

int NativeResponse_InitializeRuntime(void);
void NativeResponse_Complete(NativeRemoteResult *,u32,const u32 *,u32);

/* Borrowed packet-reader views, never installed in SDK-owned containers. */
/* 借用的报文读取视图，不安装进 SDK 所有的容器。 */
typedef struct NativeRpcBufferView {
    u32 reserved00[3];
    const u8 *data;
    u32 size;
    u32 capacity;
    u32 offset;
    u32 reserved1C;
} NativeRpcBufferView;
typedef struct NativeRpcReaderView {
    void *vtable;
    u8 failed;
    u8 reserved05[3];
    NativeRpcBufferView *buffer;
    u32 position;
    u32 reserved10[12];
} NativeRpcReaderView;
typedef void (*NativeRpcParserFn)(NativeRpcReaderView *,NativeRemoteResult *);
typedef void (*NativeInitialResultBuildFn)(u32 *,const u32 *);
void NativeResponse_PollInitial(uintptr_t peer,NativeRemoteResult *,NativeRpcParserFn,
    NativeInitialResultBuildFn,NativeSdkCompleteFn);

int NativeResponse_Bind(NativeRemoteView *,NativeRemoteBindFn);
int NativeResponse_AcceptOperation(NativeRemoteView *,NativeRemoteResult *,u32 operation);
void NativeResponse_CancelRemote(NativeRemoteView *);
void NativeResponse_DownloadResponse(NativeRemoteView *,const NativeRemoteResult *,NativeRemoteResponseFn);
void NativeResponse_BuildResult(u32 *,int status,NativeRemoteResultBuildFn);
void NativeResponse_SetDescriptor(NativeRemoteView *,const FsTransactionParameters *);
int NativeResponse_AcceptMetadata(NativeRemoteView *,const NativeMetadataIdView *,NativeRemoteResult *);
int NativeResponse_TransactionReply(NativeRemoteResult *,NativeRemoteView *,FsServer *,
    u32 commit,u32 force,NativeRemoteResultBuildFn);
int NativeResponse_RecoveryTokenReply(NativeRemoteResult *,NativeRemoteView *,FsServer *,
    u32 challenge,NativeResponseAllocateFn,NativeRemoteResultBuildFn);
void NativeResponse_PollAccepted(NativeRemoteView *,NativeResponseAllocateFn,
    NativeRemoteResultBuildFn,NativeSdkCompleteFn);
void NativeResponse_PollMetadata(NativeRemoteView *,FsServer *,NativeResponseAllocateFn,
    NativeRemoteResultBuildFn,NativeSdkCompleteFn);
void NativeResponse_PollQuery(NativeRemoteView *,FsServer *,NativeRemoteResultBuildFn,
    NativeSdkCompleteFn);
void NativeResponse_PollRecord(NativeRemoteView *,FsServer *,NativeRemoteResultBuildFn,
    NativeSdkCompleteFn);
void NativeResponse_PollDownload(NativeRemoteView *,FsServer *,const FsSourceMetadata *,
    NativeResponseAllocateFn,NativeRemoteResultBuildFn,NativeSdkCompleteFn);
void NativeResponse_PollMetadataUpdate(NativeRemoteView *,FsServer *,const FsSourceMetadata *,
    NativeRemoteResultBuildFn,NativeSdkCompleteFn);
int NativeResponse_Ticket(NativeRemoteResult *,const NativeRpcReaderView *,
    NativeRpcParserFn const *parsers,NativeRemoteResultBuildFn,NativeSdkCompleteFn);
int NativeResponse_QueueTicket(uintptr_t peer,NativeRemoteResult *,const NativeRpcReaderView *,
    NativeRpcParserFn const *,NativeRemoteResultBuildFn,NativeSdkCompleteFn);
void NativeResponse_PollTicket(uintptr_t peer,NativeRemoteResult *,NativeRpcParserFn const *,
    NativeRemoteResultBuildFn,NativeSdkCompleteFn);
int NativeResponse_Decode(NativeRemoteResult *,NativeRpcParserFn,const void *,u32);
void NativeResponse_PollCreate(NativeRemoteView *,FsServer *,NativeRemoteResultBuildFn,
    NativeSdkCompleteFn);
void NativeResponse_PollStage(NativeRemoteView *,const FsSource *,const void *bank,u32 size,
    NativeRemoteResultBuildFn,NativeSdkCompleteFn);

#endif
