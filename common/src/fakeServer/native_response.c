#include "fakeServer/native_response.h"
#include "fakeServer/fake_server_client.h"
#include "fakeServer/fake_servers.h"
#include "fs_sdk.h"
#include <string.h>

_Static_assert(FS_SERVICE_QUERY_BANK==4 && FS_SERVICE_QUERY_RECORD==5 && FS_SERVICE_STAGE_BANK==8 &&
    FS_SERVICE_DOWNLOAD_BANK==6 && FS_SERVICE_CREATE_BANK==7 &&
    FS_SERVICE_UPDATE_METADATA==10,"native assembly service operation ABI");

/* Bit 0 awaits selection; bit 1 marks the active three-second notice.
   Only the UI thread changes this state; it is never persisted. */
/* 位 0 等待选择提示，位 1 表示正在显示三秒提示；仅 UI 线程修改，不落盘。 */
static u32 missingBankNotice __attribute__((section(".data.native_missing_bank_notice")))=0;

u32 NativeResponse_TakeMissingBankNotice(void)
{
    u32 notice=missingBankNotice&1u;missingBankNotice=notice?2u:0;
    return notice;
}

u32 NativeResponse_DisconnectMinimumDelay(void)
{ return missingBankNotice&2u?3000u:1500u; }

void NativeResponse_ClearMissingBankNotice(void)
{ missingBankNotice=0; }

int NativeResponse_InitializeRuntime(void)
{
    NativeSdkRuntimeView *runtime=NativeSdk_GetRuntime();
    if (!runtime) {
        /* Acquire the native local runtime once, not a network session. Keep
           this reference until process exit: disconnect/title transitions may
           still destroy SDK jobs, and later modes reuse the same runtime. */
        /* 仅首次缺失时取得原 SDK 本地运行上下文，不建立网络会话。
           引用保留到进程退出；断开／回标题后仍可能析构作业，后续模式继续复用。 */
        NativeSdk_AcquireRuntime();
        runtime=NativeSdk_GetRuntime();
    }
    return runtime && runtime->scheduler && runtime->requests &&
        NativeTransport_Start()==FS_ASYNC_ACCEPTED;
}

void NativeResponse_Complete(NativeRemoteResult *job,u32 state,const u32 *result,u32 copyResult)
{
    if (job && result && NativeResponse_InitializeRuntime())
        NativeSdk_Complete(job,state,result,copyResult);
}

static void destroyConnection(NativeConnectionJob *job)
{
    NativeResponseReleaseFn release=job->release;
    NativeTransport_Cancel((uintptr_t)job);
    release(job);
}
static void *const connectionVtable[3]={0,0,(void *)destroyConnection};

void *NativeResponse_CreateConnection(void *storage,u32 kind,NativeResponseReleaseFn release)
{
    NativeConnectionJob *job=storage;
    if (!job) return 0;
    if (kind!=2) {
        missingBankNotice=0;
        if(FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_MOVER)FakeServerClient_SetCustomOptions(0);
    }
    for (u32 i=0;i<sizeof(*job);i++) ((volatile u8 *)job)[i]=0;
    job->vtable=(void *)connectionVtable;
    job->release=release;
    job->kind=kind;
    const FsServiceRequest input={.operation=kind==2?FS_SERVICE_DISCONNECT:FS_SERVICE_CONNECT};
    if (!NativeResponse_InitializeRuntime() ||
        NativeTransport_Accept((uintptr_t)job,(uintptr_t)job,&input)!=FS_ASYNC_ACCEPTED) {
        job->reply.result=2;
    }
    return job;
}

u32 NativeResponse_PollConnection(NativeConnectionJob *job)
{
    if (!job) return 2;
    if (!job->reply.result) {
        const FsServiceRequest input={.operation=job->kind==2?FS_SERVICE_DISCONNECT:FS_SERVICE_CONNECT};
        FsAsyncDelivery delivery;
        int received=NativeTransport_Poll((uintptr_t)job,(uintptr_t)job,&input,0,0,&delivery);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
        if (received==FS_ASYNC_READY) {
            uint32_t options=delivery.result==FS_OK?delivery.reply.customOptions:0;
            int released=NativeTransport_Release(&delivery);
            if (released==FS_ASYNC_BUSY) return 0;
            job->reply=released==FS_ASYNC_ACCEPTED && delivery.result==FS_OK?delivery.reply.output.connection:
                (FsConnectionResponse){2,0};
            if(FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_MOVER && job->kind!=2)
                FakeServerClient_SetCustomOptions(job->reply.result==1?options:0);
        } else job->reply=(FsConnectionResponse){2,0};
        job->errorCode=job->reply.errorCode;
        if (job->kind==2 && job->reply.result==1) FakeServerClient_EndSession();
    }
    return job->reply.result;
}

int NativeResponse_Ticket(NativeRemoteResult *job,const NativeRpcReaderView *request,
    NativeRpcParserFn const *parsers,NativeRemoteResultBuildFn build,
    NativeSdkCompleteFn complete)
{
    if (!job || !request || !request->buffer || !parsers || !build || !complete ||
        !NativeResponse_InitializeRuntime()) return 0;
    const NativeRpcBufferView *input=request->buffer;
    /* Original serialization: four-byte envelope, extended service ID 200,
       four-byte SDK request ID, then the little-endian operation number. */
    /* 原序列化依次为四字节包头、扩展服务编号 200、四字节 SDK 请求编号及操作编号。 */
    if (!input->data || input->offset>input->size || input->size-input->offset<15)
        return 0;
    const u8 *bytes=input->data+input->offset;
    u32 operation=(u32)bytes[11]|((u32)bytes[12]<<8)|
        ((u32)bytes[13]<<16)|((u32)bytes[14]<<24);
    u8 body[FS_TICKET_REPLY_CAPACITY],success=0;
    uint32_t size=0;
    u32 result[3];
    /* Original ticket helpers may immediately enter an unconditional SDK
       wait. Complete here, never queue delivery for a later UI update. */
    /* 原票务辅助函数可能紧接着无条件等待 SDK；必须在此完成，不能等界面下帧交付。 */
    /* Serialization is a transport observation, not an entitlement decision.
       Native startup selects UTF-8; isolated readers can also use UTF-16. */
    /* 编码是传输层观察值，不是使用权决策。原启动选择 UTF-8，独立读取器亦可使用 UTF-16。 */
    /* The service adapter owns its lock. Unsupported operations return before
       taking it, so a native catalog wait never waits for the UI/worker lock. */
    /* 服务接口自行持锁；未支持操作在取锁前返回，原目录等待不依赖界面或后台锁。 */
    int status=FakeServerClient_TicketReply(operation,body,sizeof(body),&size,*NativeSdk_RpcTextEncoding);
    if (status==FS_OK && size<=sizeof(body) && operation && operation<=10 && parsers[operation-1]) {
        success=NativeResponse_Decode(job,parsers[operation-1],body,size);
    }
    build(result,&success);
    /* Only the SDK request is completed. Dates/vectors are decoded by its
       registered native parser; client job/state transitions remain native. */
    /* 仅完成 SDK 请求；日期及列表交给已注册的原解析器，客户端作业与状态推进保留原版。 */
    complete(job,success?2:3,result,1);
    return 1;
}

int NativeResponse_QueueTicket(uintptr_t peer,NativeRemoteResult *job,const NativeRpcReaderView *request,
    NativeRpcParserFn const *parsers,NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    if (!peer || !job || !request || !request->buffer || !parsers || !build || !complete ||
        !NativeResponse_InitializeRuntime()) return 0;
    const NativeRpcBufferView *buffer=request->buffer;
    if (!buffer->data || buffer->offset>buffer->size || buffer->size-buffer->offset<15) return 0;
    const u8 *data=buffer->data+buffer->offset;
    u32 operation=(u32)data[11]|((u32)data[12]<<8)|((u32)data[13]<<16)|((u32)data[14]<<24);
    /* The server's unimplemented operations have always failed. Finish that
       same response inline; do not queue an unsupported catalog and deadlock
       the original helper's unconditional synchronous wait. */
    /* 服务未实现的操作仍失败；立即完成相同响应，不能排队后卡住原目录的无条件等待。 */
    if (!FsTicket_QueryOperation(operation))
        return NativeResponse_Ticket(job,request,parsers,build,complete);
    FsServiceRequest input;
    int status=FakeServerClient_PrepareTicketRequest(&input,operation,*NativeSdk_RpcTextEncoding);
    if (status==FS_OK && NativeTransport_Accept(peer,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED) {
        FakeServerClient_TicketRequestAccepted(&input);
        FsAsyncDelivery delivery;
        int queued=NativeTransport_Poll(peer,(uintptr_t)job,&input,0,FS_TICKET_REPLY_CAPACITY,&delivery);
        if (queued==FS_ASYNC_ACCEPTED || queued==FS_ASYNC_BUSY || queued==FS_ASYNC_READY) return 1;
        NativeTransport_Cancel(peer);
    }
    u8 success=0;u32 result[3];build(result,&success);complete(job,3,result,1);
    return 1;
}

void NativeResponse_PollTicket(uintptr_t peer,NativeRemoteResult *job,NativeRpcParserFn const *parsers,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    if (!peer || !job || job->state>1 || !parsers || !build || !complete) return;
    FsServiceRequest input;
    if (NativeTransport_Request(peer,(uintptr_t)job,&input)!=FS_ASYNC_ACCEPTED ||
        input.operation!=FS_SERVICE_TICKET_REPLY) return;
    FsAsyncDelivery delivery;
    int received=NativeTransport_Poll(peer,(uintptr_t)job,&input,0,FS_TICKET_REPLY_CAPACITY,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return;
    u8 body[FS_TICKET_REPLY_CAPACITY],success=0;u32 length=0;
    int status=FS_IO_ERROR;
    if (received==FS_ASYNC_READY) {
        status=delivery.result;length=delivery.reply.bodySize;
        if (status==FS_OK && length<=sizeof(body) && (!length || delivery.body))
            memcpy(body,delivery.body,length);
        else status=FS_IO_ERROR;
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return;
        if (released!=FS_ASYNC_ACCEPTED) status=FS_IO_ERROR;
    }
    u32 operation=input.input.ticket.operation;
    if (status==FS_OK && operation && operation<=10 && parsers[operation-1])
        success=NativeResponse_Decode(job,parsers[operation-1],body,length);
    u32 result[3];build(result,&success);complete(job,success?2:3,result,1);
}

int NativeResponse_Decode(NativeRemoteResult *job,NativeRpcParserFn parser,const void *body,u32 size)
{
    if (!job || !parser || (!body && size)) return 0;
    NativeRpcBufferView buffer;
    NativeRpcReaderView reader;
    for (u32 i=0;i<sizeof(buffer);i++) ((volatile u8 *)&buffer)[i]=0;
    for (u32 i=0;i<sizeof(reader);i++) ((volatile u8 *)&reader)[i]=0;
    buffer.size=size; buffer.capacity=size; buffer.data=body;
    reader.buffer=&buffer;
    parser(&reader,job);
    return !reader.failed && reader.position==size;
}

void NativeResponse_PollInitial(uintptr_t peer,NativeRemoteResult *job,NativeRpcParserFn parser,
    NativeInitialResultBuildFn build,NativeSdkCompleteFn complete)
{
    if (!peer || !job || job->state>1 || !parser || !build || !complete) return;
    FsServiceRequest input={.operation=FS_SERVICE_INITIAL_VALUES},snapshot;
    int status=FS_IO_ERROR;
    if (NativeTransport_Request(peer,(uintptr_t)job,&snapshot)!=FS_ASYNC_ACCEPTED) {
        int accepted=NativeTransport_Accept(peer,(uintptr_t)job,&input);
        if (accepted==FS_ASYNC_BUSY) return;
        if (accepted!=FS_ASYNC_ACCEPTED) goto failed;
    }
    FsAsyncDelivery delivery;
    int received=NativeTransport_Poll(peer,(uintptr_t)job,&input,0,0,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return;
    if (received==FS_ASYNC_READY) {
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return;
        if (released==FS_ASYNC_ACCEPTED && delivery.result==FS_OK &&
            NativeResponse_Decode(job,parser,delivery.reply.output.initialValues,
                sizeof(delivery.reply.output.initialValues))) status=FS_OK;
    }
failed: ;
    u32 code=status==FS_OK?0x10001u:0,result[3];
    build(result,&code);
    complete(job,code?2:3,result,1);
}

int NativeResponse_Bind(NativeRemoteView *remote,NativeRemoteBindFn original)
{
    /* Native binding only constructs SDK-owned containers and reads +0x0C of
       this provider view. Local requests never dereference that provider. */
    /* 原绑定仅构造 SDK 所有的容器并读取提供者 +0x0C；本地请求不调用该提供者。 */
    volatile u32 provider[4];
    for (u32 i=0;i<4;i++) provider[i]=0;
    if (!remote || !original || !NativeResponse_InitializeRuntime() ||
        !original(remote,(const void *)provider)) return 0;
    return remote->descriptor && remote->metadata && remote->tokens && remote->support;
}

int NativeResponse_AcceptOperation(NativeRemoteView *remote,NativeRemoteResult *job,u32 operation)
{
    const FsServiceRequest input={.operation=operation};
    int accepted=remote && job &&
        NativeTransport_Accept((uintptr_t)remote,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED;
    if (accepted && operation==FS_SERVICE_DOWNLOAD_BANK) {
        missingBankNotice=0;
    }
    return accepted;
}

void NativeResponse_CancelRemote(NativeRemoteView *remote)
{
    NativeRemoteResult *job=remote?remote->request:0;
    FsServiceRequest input;
    int download=job && job->state<=1 &&
        NativeTransport_Request((uintptr_t)remote,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED &&
        input.operation==FS_SERVICE_DOWNLOAD_BANK;
    NativeTransport_Cancel((uintptr_t)remote);
    if (download) {
        /* Unbind must finish the locally provided SDK request too: there is
           no HTTP provider left to send its cancellation. Match native unbind
           by detaching the UI observer before notifications. The original
           completion path clears busy/request, deletes the job, and our
           canceled-download wrapper frees its vector. Running service work
           retains only its own copies, never these destroyed SDK objects. */
        /* 解绑不能只取消后台任务：本地请求没有 HTTP 提供者补发 SDK 取消完成。
           与原解绑一致，通知前先脱离界面回调，再由原完成链清除 busy/request、
           析构作业，并由下载取消包装析构向量。未结束的服务只持有自有副本。 */
        remote->callback=0;
        const u32 result[3]={0,0,0};
        NativeResponse_Complete(job,4,result,1);
    }
}

void NativeResponse_DownloadResponse(NativeRemoteView *remote,const NativeRemoteResult *job,
    NativeRemoteResponseFn original)
{
    /* On cancellation the stock callback delegates stream cleanup to its
       network provider. That provider was not constructed for local SD I/O.
       Retire only our native download container, through its original virtual
       destructor, after the unchanged cancellation observer has run. Detach
       before notification: the observer may end the remote object's lifetime.
       Success/error already destroy the vector in the original callback. */
    /* 原取消回调将流清理交给网络提供者，但本地 SD 请求没有构造该提供者。
       仅补回下载容器的清理：先脱离远端对象（通知可能结束其生命周期），再执行
       原取消通知，最后调用原虚析构。成功／失败仍由原回调析构，不重复释放。 */
    NativeDownloadReplyView *cancelled=remote && job && job->state==4?remote->download:0;
    if (cancelled) remote->download=0;
    original(remote,job);
    if (cancelled) {
        NativeResponseReleaseFn destroy=((NativeResponseReleaseFn *)cancelled->vtable)[1];
        destroy(cancelled);
    }
}

/* Called only from the original UI wait. A successful queue is not a service
   response; do not complete the SDK job until the worker result is available. */
/* 仅在原界面等待阶段调用；成功排队不是服务响应，后台返回后才完成原 SDK 作业。 */
static int receiveParts(NativeRemoteView *remote,const FsServiceRequest *input,
    const void *body,u32 size,const void *appendInput,u32 appendSize,
    FsAsyncDelivery *delivery,int *status)
{
    NativeRemoteResult *job=remote?(NativeRemoteResult *)remote->request:0;
    if (!remote || !remote->initialized || !remote->busy || !job || job->state) return 0;
    int result=NativeTransport_PollParts((uintptr_t)remote,(uintptr_t)job,input,body,size,
        appendInput,appendSize,delivery);
    if (result==FS_ASYNC_ACCEPTED || result==FS_ASYNC_BUSY) return 0;
    if (result==FS_ASYNC_READY) *status=delivery->result;
    else {
        *delivery=(FsAsyncDelivery){0};
        *status=FS_IO_ERROR;
    }
    return 1;
}

static int receive(NativeRemoteView *remote,const FsServiceRequest *input,
    const void *body,u32 size,FsAsyncDelivery *delivery,int *status)
{
    return receiveParts(remote,input,body,size,0,0,delivery,status);
}

static int releaseReply(const FsAsyncDelivery *delivery,int *status)
{
    if (!delivery->identity) return 1;
    int released=NativeTransport_Release(delivery);
    if (released==FS_ASYNC_BUSY) return 0;
    /* A canceled/invalidated delivery is not a successful transport result. */
    /* 被取消或失效的交付不能沿用先前成功结果，只有已确认释放才可完成原作业。 */
    if (released!=FS_ASYNC_ACCEPTED) *status=FS_IO_ERROR;
    return 1;
}

void NativeResponse_BuildResult(u32 *result,int status,NativeRemoteResultBuildFn build)
{
    u8 success=status==FS_OK;
    build(result,&success);
    /* The original error converter maps these service results to EF1B/EF14.
       Keep the SDK-owned diagnostic fields from its native result builder. */
    /* 原错误转换器将这两个服务结果映射为 EF1B／EF14；诊断字段保留原 SDK 构造值。 */
    if (status==FS_TRANSACTION_MISMATCH) result[0]=0x8069000Bu;
    else if (status==FS_REMOTE_NOT_FOUND) result[0]=0x80690004u;
}

void NativeResponse_SetDescriptor(NativeRemoteView *remote,
    const FsTransactionParameters *reply)
{
    NativeRemoteDescriptor *descriptor=remote->descriptor;
    descriptor->dataId[0]=(u32)reply->dataId;
    descriptor->dataId[1]=(u32)(reply->dataId>>32);
    descriptor->currentVersion=reply->currentVersion;
    descriptor->updateVersion=reply->updateVersion;
    descriptor->size=reply->size;
    descriptor->token[0]=(u32)reply->token;
    descriptor->token[1]=(u32)(reply->token>>32);
}

void NativeResponse_PollQuery(NativeRemoteView *remote,FsServer *server,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    const FsServiceRequest input={.operation=FS_SERVICE_QUERY_BANK};
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,0,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    FsRemoteStatus reply=delivery.reply.output.bank;
    if (status==FS_OK && reply.bankReady && remote->descriptor) {
        FsTransactionParameters parameters;
        parameters.dataId=reply.dataId;
        parameters.currentVersion=reply.currentVersion;
        parameters.updateVersion=reply.updateVersion;
        parameters.size=reply.size;
        parameters.reserved14=0;
        parameters.token=reply.transactionPassword;
        NativeResponse_SetDescriptor(remote,&parameters);
        remote->replyMode=reply.replyMode;
        remote->transactionOwner=reply.transactionOwner;
    } else if (status==FS_OK) status=FS_INVALID;
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    /* Finish the real SDK job after its native caller has entered the wait
       state. Its registered observer owns delivery, cleanup and destruction. */
    /* 原调用方进入等待后才完成真实 SDK 作业，由已注册的原观察者负责投递、清理及析构。 */
    complete(job,status==FS_OK?2:3,result,1);
}

void NativeResponse_PollRecord(NativeRemoteView *remote,FsServer *server,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    const FsServiceRequest input={.operation=FS_SERVICE_QUERY_RECORD};
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,0,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    FsRecordStatus reply=delivery.reply.output.record;
    NativeSupportReplyView *support=remote->support;
    if (status==FS_OK && support) {
        remote->fileStatus=reply.fileStatus;
        support->code=reply.supportCode;
    } else if (status==FS_OK) status=FS_INVALID;
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    complete(job,status==FS_OK?2:3,result,1);
}

void NativeResponse_PollDownload(NativeRemoteView *remote,FsServer *server,
    const FsSourceMetadata *metadata,NativeResponseAllocateFn allocate,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    FsServiceRequest input={.operation=FS_SERVICE_DOWNLOAD_BANK};
    if (metadata) { input.input.download.metadata=*metadata; input.input.download.known=1; }
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,FS_BANK_SIZE,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    NativeDownloadReplyView *body=remote->download;
    if (status==FS_OK) {
        if (!body || !allocate || delivery.reply.bodySize!=FS_BANK_SIZE || !delivery.body) status=FS_INVALID;
        else {
            if (!body->begin) {
                body->begin=allocate(FS_BANK_SIZE);
                body->capacity=body->begin?body->begin+FS_BANK_SIZE:0;
            }
            if (!body->begin || body->capacity<body->begin ||
                (u32)(body->capacity-body->begin)<FS_BANK_SIZE) status=FS_LIMIT;
            else {
                int copied=NativeTransport_CopyReply((uintptr_t)remote,&delivery,body->begin,FS_BANK_SIZE,0x10000u);
                if (copied==FS_ASYNC_BUSY) return;
                if (copied!=FS_ASYNC_READY) status=FS_INVALID;
                else {
                    body->end=body->begin+FS_BANK_SIZE;
                    body->length=FS_BANK_SIZE;
                    if (FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_BANK && remote->descriptor)
                        NativeResponse_SetDescriptor(remote,&delivery.reply.output.descriptor);
                }
            }
        }
    }
    uint32_t options=status==FS_OK?delivery.reply.customOptions:0;
    if (!releaseReply(&delivery,&status)) return;
    if(FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_MOVER)
        FakeServerClient_SetCustomOptions(status==FS_OK?options:0);
    u32 result[3];
    NativeResponse_BuildResult(result,status,build);
    /* The native SDK response handler owns the allocated body and delivers it
       to the original file consumer. It also frees it on either outcome. */
    /* 原 SDK 响应处理器拥有正文缓冲并交给原文件消费者，成功与失败都由原析构释放。 */
    /* An absent Bank body is a failed download, not a pending/locked server.
       Keep the SDK's ordinary failure result and the native exit route. */
    /* 银行正文缺失属于下载失败，不表示服务器被锁定；保留 SDK 普通失败及原退出路线。 */
    if (FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_MOVER)
        missingBankNotice=status==FS_MISSING;
    complete(job,status==FS_OK?2:3,result,1);
}

void NativeResponse_PollMetadataUpdate(NativeRemoteView *remote,FsServer *server,
    const FsSourceMetadata *metadata,NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    FsServiceRequest input={.operation=FS_SERVICE_UPDATE_METADATA};
    NativeRemoteDescriptor *id=remote?remote->descriptor:0;
    if (id) input.input.metadata.dataId=(u64)id->dataId[0]|((u64)id->dataId[1]<<32);
    if (id && metadata) {
        input.input.metadata.metadata=*metadata;
        input.input.metadata.known=1;
    }
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,0,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    if (status==FS_OK && (!id || !metadata)) status=FS_INVALID;
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    complete(job,status==FS_OK?2:3,result,1);
}

void NativeResponse_PollCreate(NativeRemoteView *remote,FsServer *server,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    const FsServiceRequest input={.operation=FS_SERVICE_CREATE_BANK};
    NativeUploadView *body=remote?remote->upload:0;
    u32 size=body && body->begin && body->end>=body->begin?(u32)(body->end-body->begin):0;
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,body?body->begin:0,size,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    if (status==FS_OK) {
        remote->createdDataId[0]=(u32)delivery.reply.output.descriptor.dataId;
        remote->createdDataId[1]=(u32)(delivery.reply.output.descriptor.dataId>>32);
    }
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    complete(job,status==FS_OK?2:3,result,1);
}

void NativeResponse_PollStage(NativeRemoteView *remote,const FsSource *source,
    const void *bank,u32 size,NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    FsServiceRequest input={.operation=FS_SERVICE_STAGE_BANK};
    const void *path=0;u32 pathSize=0;
    if (source && remote && remote->descriptor) {
        input.input.stage.kind=source->kind;
        input.input.stage.mode=source->mode;
        input.input.stage.gameCode=source->gameCode;
        input.input.stage.bankSize=size;
        input.input.stage.pathSize=source->pathSize;
        if (source->expected) input.input.stage.expected=*source->expected;
        if (source->metadata) {
            input.input.stage.metadata=*source->metadata;
            input.input.stage.known=1;
        }
        path=source->path;pathSize=source->pathSize;
    } else { bank=0;size=0; }
    FsAsyncDelivery delivery;int status;
    if (!receiveParts(remote,&input,bank,size,path,pathSize,&delivery,&status)) return;
    NativeRemoteResult *job=remote->request;
    if (status==FS_OK && remote->descriptor)
        NativeResponse_SetDescriptor(remote,&delivery.reply.output.descriptor);
    else if (status==FS_OK) status=FS_INVALID;
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    complete(job,status==FS_OK?2:3,result,1);
}

int NativeResponse_AcceptMetadata(NativeRemoteView *remote,const NativeMetadataIdView *request,
    NativeRemoteResult *job)
{
    NativeMetadataReplyView *reply=remote?remote->metadata:0;
    if (!request || !reply || !job) return 0;
    reply->identity.dataId[0]=request->dataId[0];
    reply->identity.dataId[1]=request->dataId[1];
    FsServiceRequest input={.operation=FS_SERVICE_QUERY_METADATA};
    input.input.metadata.dataId=(u64)request->dataId[0]|((u64)request->dataId[1]<<32);
    return NativeTransport_Accept((uintptr_t)remote,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED;
}

int NativeResponse_TransactionReply(NativeRemoteResult *job,NativeRemoteView *remote,
    FsServer *server,u32 commit,u32 force,NativeRemoteResultBuildFn build)
{
    (void)server; (void)build;
    if (!job || !remote || !remote->descriptor) return 0;
    NativeRemoteDescriptor *id=remote->descriptor;
    FsTransactionParameters request;
    request.dataId=(u64)id->dataId[0]|((u64)id->dataId[1]<<32);
    request.currentVersion=id->currentVersion;
    request.updateVersion=id->updateVersion;
    request.size=id->size;
    request.reserved14=0;
    request.token=(u64)id->token[0]|((u64)id->token[1]<<32);
    FsServiceRequest input={.operation=FS_SERVICE_REQUEST_TRANSACTION};
    input.input.transaction.descriptor=request;
    input.input.transaction.commit=commit;
    input.input.transaction.force=force;
    return NativeTransport_Accept((uintptr_t)remote,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED;
}

void NativeResponse_PollAccepted(NativeRemoteView *remote,NativeResponseAllocateFn allocate,
    NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    NativeRemoteResult *job=remote?(NativeRemoteResult *)remote->request:0;
    if (!remote || !remote->initialized || !remote->busy || !job || job->state) return;
    FsServiceRequest input;
    if (NativeTransport_Request((uintptr_t)remote,(uintptr_t)job,&input)!=FS_ASYNC_ACCEPTED) return;
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,0,&delivery,&status)) return;
    if (input.operation==FS_SERVICE_RECOVERY_CODES && status==FS_OK) {
        NativeRecoveryTokensView *tokens=remote->tokens;
        if (!tokens || !allocate) status=FS_INVALID;
        else {
            if (!tokens->begin) {
                tokens->begin=allocate(sizeof(delivery.reply.output.rollback.candidates));
                tokens->end=tokens->begin;
                tokens->capacity=tokens->begin?tokens->begin+3:0;
            }
            if (!tokens->begin || tokens->capacity<tokens->begin ||
                (u32)(tokens->capacity-tokens->begin)<3) status=FS_LIMIT;
            else {
                for (u32 i=0;i<3;i++) tokens->begin[i]=delivery.reply.output.rollback.candidates[i];
                tokens->end=tokens->begin+3;
            }
        }
    }
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    complete(job,2,result,1);
}

int NativeResponse_RecoveryTokenReply(NativeRemoteResult *job,NativeRemoteView *remote,
    FsServer *server,u32 challenge,NativeResponseAllocateFn allocate,NativeRemoteResultBuildFn build)
{
    (void)server; (void)allocate; (void)build;
    if (!job || !remote || !remote->tokens) return 0;
    FsServiceRequest input={.operation=FS_SERVICE_RECOVERY_CODES};
    input.input.rollback.challenge=challenge;
    return NativeTransport_Accept((uintptr_t)remote,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED;
}

void NativeResponse_PollMetadata(NativeRemoteView *remote,FsServer *server,
    NativeResponseAllocateFn allocate,NativeRemoteResultBuildFn build,NativeSdkCompleteFn complete)
{
    (void)server;
    NativeRemoteResult *job=remote?(NativeRemoteResult *)remote->request:0;
    if (!remote || !remote->initialized || !remote->busy || !job || job->state) return;
    FsServiceRequest input;
    if (NativeTransport_Request((uintptr_t)remote,(uintptr_t)job,&input)!=FS_ASYNC_ACCEPTED ||
        input.operation!=FS_SERVICE_QUERY_METADATA) return;
    FsAsyncDelivery delivery; int status;
    if (!receive(remote,&input,0,0,&delivery,&status)) return;
    NativeMetadataReplyView *reply=remote->metadata;
    if (status==FS_OK) {
        if (!reply || !allocate) status=FS_INVALID;
        else {
            if (!reply->begin) {
                reply->begin=allocate(sizeof(FsSourceMetadata));
                reply->allocation=reply->begin;
                reply->capacity=reply->begin?reply->begin+sizeof(FsSourceMetadata):0;
            }
            if (!reply->begin || reply->capacity<reply->begin ||
                (u32)(reply->capacity-reply->begin)<sizeof(FsSourceMetadata)) status=FS_LIMIT;
            else {
                memcpy((void *)reply->begin,&delivery.reply.output.metadata,sizeof(FsSourceMetadata));
                reply->end=reply->begin+sizeof(FsSourceMetadata);
            }
        }
    }
    u32 result[3];
    if (!releaseReply(&delivery,&status)) return;
    NativeResponse_BuildResult(result,status,build);
    /* The original metadata callback consumes the vector; the original SDK
       container owns its allocation and releases it during finalization. */
    /* 原元数据回调消费向量；分配归原 SDK 容器所有，由原收尾析构释放。 */
    complete(job,status==FS_OK?2:3,result,1);
}
