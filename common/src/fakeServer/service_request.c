#include "fakeServer/service_request.h"
#include "fakeServer/fake_servers.h"
#include "fs_system/diagnostic_log.h"
#include "fs_system/storage_notice.h"
#include <stddef.h>
#include <string.h>
#if defined(__arm__)
#include "fake_server_context.h"
#include "fakeServer/sd_adapter.h"
#endif
#if defined(FAKE_SERVER_BANK_NATIVE_LANGUAGE)
#include "fs_system/bank_native_settings.h"
#endif
#if !defined(__arm__) || defined(FAKE_SERVER_SOURCE_CONTEXT_ADDRESS)
#define SERVICE_HAS_LOCAL_SOURCE 1
#else
#define SERVICE_HAS_LOCAL_SOURCE 0
#endif

static int executeRequest(void *,const void *,uint32_t,void *,uint32_t,uint32_t *,void *,uint32_t);

_Static_assert(sizeof(FsServiceRequest)<=FS_ASYNC_SMALL_CAPACITY,"service input exceeds task capacity");
_Static_assert(sizeof(FsServiceReply)<=FS_ASYNC_SMALL_CAPACITY,"service output exceeds task capacity");
_Static_assert(offsetof(FsAsyncTask,input)%_Alignof(FsServiceRequest)==0,"task request alignment");
_Static_assert(offsetof(FsAsyncTask,output)%_Alignof(FsServiceReply)==0,"task reply alignment");

static int validPath(const void *body,uint32_t size)
{
    if (!body || ((uintptr_t)body&1) || size<4 || (size&1) ||
        size>FS_TRANSACTION_PATH_CAPACITY*2u) return 0;
    const uint16_t *path=body;
    if (path[0]!='/' || path[size/2-1]) return 0;
    for (uint32_t i=0;i<size/2-1;i++) if (!path[i]) return 0;
    return 1;
}

static int validateBody(const FsServiceRequest *request,const void *body,uint32_t size)
{
    switch (request->operation) {
    case FS_SERVICE_TITLE_USERS:
        return request->input.users.action==0?(body && size>=sizeof(FsUserEntry)):
            request->input.users.action==1 && !(size%sizeof(FsUserEntry));
    case FS_SERVICE_DOWNLOAD_BANK:
        return body && size==FS_BANK_SIZE && request->input.download.known<=1;
    case FS_SERVICE_CREATE_BANK:
    case FS_SERVICE_EXPORT_BANK:
        return body && size==FS_BANK_SIZE;
    case FS_SERVICE_UPDATE_METADATA:
        return !size && request->input.metadata.known==1;
    case FS_SERVICE_STAGE_BANK:
    {
        uint32_t bank=request->input.stage.bankSize,path=request->input.stage.pathSize;
        if (request->input.stage.known>1 || request->input.stage.mode>3 ||
            request->input.stage.kind>FS_TRANSACTION_NDS_FILE ||
            (bank && bank!=FS_BANK_SIZE) || bank>size || path!=size-bank) return 0;
        if (request->input.stage.kind==FS_TRANSACTION_NDS_FILE) {
            return body && validPath((const uint8_t *)body+bank,path);
        }
        return body && bank==FS_BANK_SIZE && !path;
    }
    case FS_SERVICE_WRITE_TURTLE:
        return body && size==0x200 && request->input.file.activeSave<=1;
    case FS_SERVICE_UPDATE_TURTLE_RECORD:
        return !size && request->input.turtle.activeSave<=1;
    case FS_SERVICE_UPDATE_TURTLE_LANGUAGE:
        return (request->input.language.createIfMissing?(body && size==0x200):!size) &&
            request->input.language.languageId &&
            request->input.language.languageId<=10 && request->input.language.languageId!=6 &&
            request->input.language.useJapaneseKanji<=1 && request->input.language.createIfMissing<=1;
    case FS_SERVICE_SYNC_MOVER_LANGUAGE:
        return !size && request->input.language.languageId && request->input.language.languageId<=10 &&
            request->input.language.languageId!=6 && request->input.language.useJapaneseKanji<=1 &&
            !request->input.language.createIfMissing;
    case FS_SERVICE_READ_BANK_NATIVE_SETTINGS:
        return body && size==0x200 && !request->input.file.offset && request->input.file.size==size;
    case FS_SERVICE_PREPARE_TURTLE_RECORD:
        return body && size==sizeof(request->input.turtle.record);
    case FS_SERVICE_READ_TURTLE:
        return body && request->input.file.size==size && size &&
            request->input.file.offset<=0x200 && size<=0x200-request->input.file.offset;
    case FS_SERVICE_READ_SOURCE:
    case FS_SERVICE_WRITE_SOURCE:
        return body && request->input.file.size==size && size;
    case FS_SERVICE_READ_SOURCE_IMAGE:
        return body && !request->input.file.offset && request->input.file.size==size && size==0x80000u;
    case FS_SERVICE_GIFT_REPLY:
        return body && size==FS_GIFT_MAX_SIZE;
    case FS_SERVICE_CAMPAIGN_RESOURCE:
        return body && size==sizeof(FsCampaignRecord) && request->input.campaign.onlineFallback<=1;
    case FS_SERVICE_TICKET_REPLY:
        return body && size==FS_TICKET_REPLY_CAPACITY &&
            request->input.ticket.textEncoding<=FS_TICKET_TEXT_UTF8 &&
            request->input.ticket.onlineFallback<=1 && request->input.ticket.beginQuery<=1;
    case FS_SERVICE_TICKET_ACTION:
        return !size && request->input.shop.onlineFallback<=1 &&
            (request->input.shop.applet==5 || request->input.shop.applet==6 ||
             (request->input.shop.applet==7 && request->input.shop.product<FS_TICKET_PRODUCT_COUNT));
    case FS_SERVICE_CHECK_MOVER:
        return body && !((uintptr_t)body%_Alignof(FsMoverSlotSnapshot)) &&
            size==FS_MOVER_SLOT_COUNT*sizeof(FsMoverSlotSnapshot) &&
            request->input.checks.count==FS_MOVER_SLOT_COUNT &&
            (request->input.checks.source==FS_MOVER_GEN5 || request->input.checks.source==FS_MOVER_VC);
    case FS_SERVICE_OBSERVE_SOURCE:
    case FS_SERVICE_BEGIN_SOURCE_SAVE:
    case FS_SERVICE_SOURCE_SAVE_RESULT:
    case FS_SERVICE_SETTLED_SOURCE:
        return request->input.source.mode<=3 && request->input.source.pathSize==size &&
            (request->input.source.kind==FS_TRANSACTION_NDS_FILE?validPath(body,size):
                (request->input.source.kind==FS_TRANSACTION_NATIVE_SOURCE && !size));
    default:
        return !size;
    }
}

static int ensureServer(FsServiceContext *context)
{
    if (FsServer_Connected(context->server)) return FS_OK;
    FsConfigError error;
    int connected=FakeServerApi_Connect(context->server,context->capacity,context->owner,0,
        context->storage,context->transactions,&error);
    FsDiag_Record("server.connect.result",connected,context->owner,
        FsServer_Connected(context->server),0);
    (void)connected;
    /* As in the existing client Get, pending recovery does not make the local
       storage disappear. The requested endpoint still checks its own policy,
       authority and transaction state; no success is fabricated here. */
    /* 与既有客户端 Get 一致：待恢复不等于本地存储消失；目标接口仍自行核对策略、
       权威记录及事务状态，此处不伪造业务成功。 */
    return FsServer_Connected(context->server)?FS_OK:FS_INVALID;
}

static int stageBank(FsServiceContext *context,const FsServiceRequest *request,
    FsServiceReply *reply,void *body)
{
    const uint32_t bankSize=request->input.stage.bankSize;
    FsSource source={request->input.stage.kind,request->input.stage.gameCode,
        request->input.stage.pathSize?(const uint16_t *)((const uint8_t *)body+bankSize):0,
        request->input.stage.pathSize,request->input.stage.mode,
        request->input.stage.kind==FS_TRANSACTION_NDS_FILE?&request->input.stage.expected:0,
        request->input.stage.known?&request->input.stage.metadata:0};
    int result=FsServer_Stage(context->server,bankSize?body:0,bankSize,&source);
    return result==FS_OK?FsServer_DescribeTransaction(context->server,&reply->output.descriptor):result;
}

static int observeSnapshot(uint32_t index,FsMoverCandidate *candidate,FsMoverDetails *details,void *user)
{
    if (index>=FS_MOVER_SLOT_COUNT) return FS_INVALID;
    const FsMoverSlotSnapshot *slots=user;
    *candidate=slots[index].candidate;
    *details=slots[index].details;
    return FS_OK;
}

static int executeRequest(void *opaque,const void *input,uint32_t inputSize,
    void *output,uint32_t outputCapacity,uint32_t *outputSize,void *body,uint32_t bodySize)
{
    if (outputSize) *outputSize=0;
    FsServiceContext *context=opaque;
    const FsServiceRequest *request=input;
    FsServiceReply *reply=output;
    if (!context || !context->server || context->capacity<sizeof(FsServer) ||
        !context->storage || !context->transactions ||
        (context->owner!=FS_TRANSACTION_OWNER_BANK && context->owner!=FS_TRANSACTION_OWNER_MOVER) ||
        !request || inputSize!=sizeof(*request) || (uintptr_t)request%_Alignof(FsServiceRequest) ||
        !reply || outputCapacity<sizeof(*reply) || (uintptr_t)reply%_Alignof(FsServiceReply) ||
        !outputSize || (!body && bodySize)) return FS_INVALID;
    if (request->operation<FS_SERVICE_OPEN || request->operation>=FS_SERVICE_OPERATION_END ||
        request->operation==17 || request->operation==35)
        return FS_UNSUPPORTED;
    if (!validateBody(request,body,bodySize)) return FS_INVALID;
    if (request->operation==FS_SERVICE_EXPORT_BANK && context->owner!=FS_TRANSACTION_OWNER_BANK)
        return FS_UNSUPPORTED;
    for (uint32_t i=0;i<sizeof(*reply);i++) ((uint8_t *)reply)[i]=0;
    *outputSize=sizeof(*reply);
    /* Title account selection may repair published fixed-schema configuration,
       but must not connect/recover the server, provision a Mover profile or
       migrate legacy Bank data. */
    if(request->operation==FS_SERVICE_TITLE_USERS) {
#if defined(__arm__)
        if(context->storage!=&FsSdStorage)return FS_UNSUPPORTED;
        /* Retire source evidence even if the server never opened successfully.
           No persistent participant or interrupted transaction is removed. */
        /* 即使服务器未成功打开也清除旧来源依据；不删除任何持久化文件或未决事务。 */
#if SERVICE_HAS_LOCAL_SOURCE
        if(context->localSource)FsLocalSource_Discard(context->localSource);
#endif
        FsServer_Disconnect(context->server);
        FsSd_ResetUser();
        int result=FsSd_UserMenu(request->input.users.action,request->input.users.activeUser,
            body,bodySize,&reply->output.users,&reply->bodySize);
        /* Even a title catalogue refresh may discover a different auto/manual
           mapping from startup. Retire only volatile old-session state, so
           later settings reads and server opens use the displayed account. */
        /* 目录刷新也可能发现映射已变化；仅丢弃旧会话内存，防止显示新用户却仍读旧路径。 */
        return result;
#else
        return FS_UNSUPPORTED;
#endif
    }
    /* Cross-title Bank access is a Mover-local save operation, not a server
       endpoint. Commit/close/reopen native settings before touching SD. */
    /* 跨软件 Bank 访问属于 Mover 本地存档操作，不是伪服务器接口；原档提交／关闭／
       重开校验成功后才能操作 SD，任何原档失败均不放行 SD 写入。 */
    if (request->operation==FS_SERVICE_READ_BANK_NATIVE_SETTINGS ||
        request->operation==FS_SERVICE_SYNC_MOVER_LANGUAGE) {
        if (context->owner!=FS_TRANSACTION_OWNER_MOVER) return FS_UNSUPPORTED;
#if defined(FAKE_SERVER_BANK_NATIVE_LANGUAGE)
        if (request->operation==FS_SERVICE_READ_BANK_NATIVE_SETTINGS) {
            int result=FsBankNative_ReadSettings(body);
            if (result==FS_OK) reply->bodySize=bodySize;
            return result;
        }
        uint8_t nativeRecord[512];
        int result=FsBankNative_UpdateLanguage(request->input.language.languageId,
            request->input.language.useJapaneseKanji,nativeRecord);
        if (result!=FS_OK) return result;
        return FsSettings_SynchronizeLanguage(context->transactions,context->owner,
            request->input.language.languageId,request->input.language.useJapaneseKanji,
            nativeRecord,sizeof(nativeRecord));
#else
        return FS_UNSUPPORTED;
#endif
    }
    if (request->operation==FS_SERVICE_TICKET_REPLY) {
        if (!FsTicket_QueryOperation(request->input.ticket.operation)) return FS_UNSUPPORTED;
        if (request->input.ticket.onlineFallback) {
            FsTicketResponse ticket;
            if (!FsTicket_OnlineFallback(request->input.ticket.currentDate,&ticket)) return FS_INVALID;
            return FsTicket_OnlineReply(&ticket,request->input.ticket.operation,body,bodySize,
                &reply->bodySize,request->input.ticket.textEncoding);
        }
    }
    if (request->operation==FS_SERVICE_TICKET_ACTION &&
        (context->owner!=FS_TRANSACTION_OWNER_BANK || request->input.shop.onlineFallback))
        return FS_UNSUPPORTED;
    if (request->operation==FS_SERVICE_OPEN) {
        /* Settings/source reads may already have pinned the confirmed user.
           Keep that selection until disconnect or the next title boundary. */
        /* 设置／来源读取可能已锁定确认的用户，不能在连接时再次改选；仅标题或断开重置。 */
        FsConfigError error;
        return FakeServerApi_Connect(context->server,context->capacity,context->owner,0,
            context->storage,context->transactions,&error);
    }
    if (request->operation==FS_SERVICE_DISCONNECT) {
        FsServer_Connection(context->server,1,&reply->output.connection);
#if SERVICE_HAS_LOCAL_SOURCE
        if(context->localSource)FsLocalSource_Discard(context->localSource);
#endif
#if defined(__arm__)
        if(context->storage==&FsSdStorage)FsSd_ResetUser();
#endif
        return FS_OK;
    }
    /* Online ticket fallback is an isolated fixed campaign. It must not open
       the offline store, import Bank data or consume local purchase settings. */
    /* 联网票务回退仅使用隔离的固定活动，不打开离线存储、登记银行或读取本地购买配置。 */
    if (request->operation==FS_SERVICE_CAMPAIGN_RESOURCE && request->input.campaign.onlineFallback) {
        FsCampaign_OnlineFallback(body);
        reply->bodySize=sizeof(FsCampaignRecord);
        return FS_OK;
    }
    /* A display preview reads published settings, not transaction authority.
       Unknown journal versions must not turn existing settings into absence.
       Native record validation and all mutation guards remain independent. */
    /* 界面预览读取正式设置，不判断事务许可；未知日志版本不能把已有设置变成缺失。
       原版记录校验及所有写入保护仍独立执行。 */
    if (request->operation==FS_SERVICE_READ_TURTLE) {
        int result=FsSettings_ReadPreviewRange(context->transactions,request->input.file.offset,body,bodySize);
        if (result==FS_OK) reply->bodySize=bodySize;
        return result;
    }
    /* A confirmed title entry starts a fresh local session, even for the same
       account. Prepare first-use Bank state and settings on the worker; never
       reuse a pre-reset body or turn an I/O failure into initialization. */
    /* 标题确认进入时，即使仍是同一账户也开始新的本地会话；后台准备银行首次使用
       状态及设置，不复用重置前正文，不把 I/O 失败当作允许初始化。 */
    if (request->operation==FS_SERVICE_PREPARE_TURTLE_RECORD) {
        if (context->owner!=FS_TRANSACTION_OWNER_BANK) return FS_UNSUPPORTED;
        FsServer_Disconnect(context->server);
        /* Retire a settled, deleted Bank before any native initialization or
           settings publication can consult its obsolete baseline. */
        /* 原版初始化或设置发布可能先检查银行基线，须在此退役已结束且被清空的银行。 */
        if (!FsTransaction_PrepareBankInitialization(context->transactions)) return FS_LOCKED;
        int result=FsSettings_ReadPreviewRange(context->transactions,0,body,bodySize);
        if (result!=FS_OK && result!=FS_IO_ERROR) {
            result=FsTransaction_ReconcileTurtle(context->transactions)?
                FsSettings_ReadRange(context->transactions,0,body,bodySize):FS_LOCKED;
        }
        if (result==FS_OK) reply->bodySize=bodySize;
        return result;
    }
    /* Local storage operations do not imply a remote request. In particular
       segmented Gen5 reads/writes must never open policy or scan Bank. Only
       completed native saves hand off a durable participant checkpoint. */
    /* 本地存储操作不等于远端请求，Gen5 分段读写不加载配置或扫描银行；
       原保存实际完成后才交接持久化的参与文件检查点。 */
    switch (request->operation) {
    case FS_SERVICE_WRITE_TURTLE:
    case FS_SERVICE_FORMAT_TURTLE:
        return FsTransaction_WriteTurtle(context->transactions,context->owner,
            request->operation==FS_SERVICE_WRITE_TURTLE?body:0,
            request->operation==FS_SERVICE_WRITE_TURTLE?bodySize:0,
            request->input.file.activeSave!=0)?FS_OK:FS_IO_ERROR;
    case FS_SERVICE_UPDATE_TURTLE_RECORD:
        return FsSettings_UpdateRecord(context->transactions,context->owner,
            request->input.turtle.record,sizeof(request->input.turtle.record),
            request->input.turtle.activeSave!=0);
    case FS_SERVICE_UPDATE_TURTLE_LANGUAGE:
        if (request->input.language.createIfMissing)
            return FsSettings_SynchronizeLanguage(context->transactions,context->owner,
                request->input.language.languageId,request->input.language.useJapaneseKanji,body,bodySize);
        return FsSettings_UpdateLanguage(context->transactions,context->owner,
            request->input.language.languageId,request->input.language.useJapaneseKanji);
    case FS_SERVICE_READ_SOURCE:
    case FS_SERVICE_WRITE_SOURCE:
    case FS_SERVICE_READ_SOURCE_IMAGE:
#if SERVICE_HAS_LOCAL_SOURCE
        if (!context->localSource) return FS_UNSUPPORTED;
        if (request->operation==FS_SERVICE_WRITE_SOURCE)
            return FsLocalSource_Write(context->localSource,request->input.file.offset,body,bodySize);
        if (request->operation==FS_SERVICE_READ_SOURCE_IMAGE) {
            int result=FsLocalSource_Flush(context->localSource,body,bodySize,&reply->output.fingerprint);
            if (result==FS_OK) reply->bodySize=bodySize;
            return result;
        } else {
            int result=FsLocalSource_Read(context->localSource,request->input.file.offset,body,bodySize);
            if (result==FS_OK) reply->bodySize=bodySize;
            return result;
        }
#else
        return FS_UNSUPPORTED;
#endif
    case FS_SERVICE_OBSERVE_SOURCE:
    case FS_SERVICE_BEGIN_SOURCE_SAVE:
    case FS_SERVICE_SOURCE_SAVE_RESULT:
    case FS_SERVICE_SETTLED_SOURCE:
    {
#if SERVICE_HAS_LOCAL_SOURCE
        FsSource source={request->input.source.kind,request->input.source.gameCode,
            bodySize?(const uint16_t *)body:0,bodySize,request->input.source.mode,
            request->input.source.fingerprint.present?&request->input.source.fingerprint:0,0};
        if (source.kind==FS_TRANSACTION_NATIVE_SOURCE) return FS_OK;
        if (!context->localSource || context->owner!=FS_TRANSACTION_OWNER_MOVER) return FS_UNSUPPORTED;
        const FsTransactionIo *coordinator=FsServer_Connected(context->server)?context->server->transactions:0;
        if (request->operation==FS_SERVICE_OBSERVE_SOURCE)
            return FsLocalSource_Load(context->localSource,&source,&reply->output.fingerprint);
        if (request->operation==FS_SERVICE_BEGIN_SOURCE_SAVE)
            return FsLocalSource_Prepare(context->localSource,&source,coordinator);
        if (request->operation==FS_SERVICE_SOURCE_SAVE_RESULT)
            return FsLocalSource_Saved(context->localSource,&source,request->input.source.nativeResult,
                coordinator,&reply->output.fingerprint);
        /* A local no-op completion retains the observed selection, never an
           old server receipt for the same path. No file reads are necessary. */
        /* 本地无写入完成保留当前选择，不取同路径的旧服务收据；无需读取文件。 */
        if (!source.expected || !FsLocalSource_Contains(context->localSource,source.path,
            source.pathSize,source.gameCode)) return FS_MISSING;
        reply->output.fingerprint=*source.expected;
        return FS_OK;
#else
        return FS_UNSUPPORTED;
#endif
    }
    default:break;
    }
    int status=ensureServer(context);
    if (status!=FS_OK) return status;
    FsServer *server=context->server;
    switch (request->operation) {
    case FS_SERVICE_CHECK_MOVER:
    {
        if (context->owner!=FS_TRANSACTION_OWNER_MOVER) return FS_UNSUPPORTED;
        /* Region-scoped words belong to this request, not an earlier EC job. */
        /* 地区词表使用本次请求的主机地区，不能借用先前 EC 作业留下的位置。 */
        status=FsServer_SetShopLocation(server,request->input.checks.region,request->input.checks.country);
        if (status!=FS_OK) return status;
        /* The response reuses task storage only after preserving its inputs.
           This modest stack belongs to the 64 KiB worker, not turtle-main. */
        /* 先保留输入，再复用任务正文写响应；此栈归 64 KiB 后台线程，不占主线程栈。 */
        FsMoverSlotSnapshot slots[FS_MOVER_SLOT_COUNT];
        memcpy(slots,body,sizeof(slots));
        return FsServer_CheckBoxReply(server,request->input.checks.source,request->input.checks.count,
            observeSnapshot,slots,body,bodySize,&reply->bodySize);
    }
    case FS_SERVICE_TICKET_REPLY:
    {
        uint32_t accepted=0;
        if (request->input.ticket.beginQuery) FsServer_BeginTicketQuery(server);
        int status=FsServer_ValidateTicket(server,&accepted);
        if (status!=FS_OK || !accepted) return status!=FS_OK?status:FS_UNSUPPORTED;
        status=FsServer_SetShopLocation(server,request->input.ticket.region,request->input.ticket.country);
        FsTicketResponse ticket;
        if (status==FS_OK) status=FsServer_Ticket(server,request->input.ticket.currentDate,&ticket);
        return status==FS_OK?FsTicket_Reply(&ticket,request->input.ticket.operation,body,bodySize,
            &reply->bodySize,request->input.ticket.textEncoding):status;
    }
    case FS_SERVICE_TICKET_ACTION:
    {
        int status=FsServer_SetShopLocation(server,request->input.shop.region,request->input.shop.country);
        if (status!=FS_OK) return status;
        if (request->input.shop.applet==6) return FsServer_AddTicketFunds(server);
        if (request->input.shop.applet==7)
            return FsServer_PurchaseTicket(server,request->input.shop.currentDate,request->input.shop.product);
        FsTicketResponse ticket;FsTicketShop shop;
        status=FsServer_Ticket(server,request->input.shop.currentDate,&ticket);
        return status==FS_OK?FsServer_TicketShop(server,&shop):status;
    }
    case FS_SERVICE_CONNECT:
        FsServer_Connection(server,0,&reply->output.connection);
        /* Mover converts candidates before downloading Bank data. Deliver
           the confirmed account's conversion policy with connection success,
           not only with the later Bank body. */
        /* Mover 先转换候选再下载银行；连接成功时即交付确认用户的转换策略，
           不能等后续银行正文下载完毕才让客户端取得开关。 */
        if(context->owner==FS_TRANSACTION_OWNER_MOVER && reply->output.connection.result==1)
            reply->customOptions=server->configuration.mover.customOptions;
        return FS_OK;
    case FS_SERVICE_QUERY_BANK:
        return FsServer_QueryRemoteStatus(server,&reply->output.bank);
    case FS_SERVICE_QUERY_RECORD:
        return FsServer_QueryRecordStatus(server,&reply->output.record);
    case FS_SERVICE_DOWNLOAD_BANK:
        if (context->owner==FS_TRANSACTION_OWNER_BANK) {
            if (!request->input.download.known) return FS_INVALID;
            status=FsServer_DownloadBank(server,body,bodySize,&request->input.download.metadata,
                &reply->output.descriptor);
        } else {
            status=FsServer_ReadBank(server,body,bodySize);
            if(status==FS_OK)reply->customOptions=server->configuration.mover.customOptions;
        }
        if (status==FS_OK) reply->bodySize=bodySize;
        return status;
    case FS_SERVICE_CREATE_BANK:
        status=FsServer_CreateBank(server,body,bodySize);
        return status==FS_OK?FsServer_DescribeTransaction(server,&reply->output.descriptor):status;
    case FS_SERVICE_EXPORT_BANK:
        /* Export a frozen real download; never generate an online reply from
           local Bank/configuration data. Publication keeps the server guard. */
        /* 发布冻结的真实下载；不把本地银行／配置生成的响应送进联网流程，保留服务端未决保护。 */
        return FsServer_WriteBank(server,body,bodySize);
    case FS_SERVICE_STAGE_BANK:
        return stageBank(context,request,reply,body);
    case FS_SERVICE_QUERY_METADATA:
        return FsServer_QueryMetadataFor(server,request->input.metadata.dataId,&reply->output.metadata);
    case FS_SERVICE_UPDATE_METADATA:
        return FsServer_UpdateMetadata(server,request->input.metadata.dataId,&request->input.metadata.metadata);
    case FS_SERVICE_REQUEST_TRANSACTION:
        return FsServer_RequestTransaction(server,&request->input.transaction.descriptor,
            request->input.transaction.commit!=0,request->input.transaction.force!=0);
    case FS_SERVICE_RECOVERY_CODES:
        return FsServer_Support(server,request->input.rollback.challenge,&reply->output.rollback);
    case FS_SERVICE_AUTHORIZE_ROLLBACK:
        return FsServer_AuthorizeRollback(server,request->input.rollback.challenge,request->input.rollback.enteredCode);
    case FS_SERVICE_NATIVE_SAVE_EVENT:
        return FsServer_NativeSaveEvent(server,&request->input.native.descriptor,
            (enum FsNativeSaveEvent)request->input.native.event);
    case FS_SERVICE_TRANSACTION_EVENT:
        return FsServer_Transaction(server,(enum FsTransactionAction)request->input.event.action);
    case FS_SERVICE_INITIAL_VALUES:
        return FsServer_InitialValues(server,reply->output.initialValues);
    case FS_SERVICE_GIFT_REPLY:
        return FsGifts_Reply(server,&request->input.gift.request,request->input.gift.pass,
            body,bodySize,&reply->bodySize);
    case FS_SERVICE_CAMPAIGN_RESOURCE:
        status=FsServer_SetShopLocation(server,request->input.campaign.region,request->input.campaign.country);
        return status==FS_OK?FsServer_Campaign(server,request->input.campaign.currentDate,
            body,bodySize,&reply->bodySize):status;
    case FS_SERVICE_GUARD:
        return FsServer_Guard(server);
    default:
        return FS_UNSUPPORTED;
    }
}

int FsService_Execute(void *opaque,const void *input,uint32_t inputSize,
    void *output,uint32_t outputCapacity,uint32_t *outputSize,void *body,uint32_t bodySize)
{
    FsStorageNotice_BeginRequest();
    int result=executeRequest(opaque,input,inputSize,output,outputCapacity,outputSize,body,bodySize);
    if (output && outputSize && *outputSize==sizeof(FsServiceReply))
        ((FsServiceReply *)output)->storageIssue=FsStorageNotice_RequestIssue(result);
    return result;
}
