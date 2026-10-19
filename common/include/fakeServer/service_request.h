#ifndef COMMON_FAKE_SERVER_SERVICE_REQUEST_H
#define COMMON_FAKE_SERVER_SERVICE_REQUEST_H

#include "async_task.h"
#include "fakeserverapi.h"
#include "fs_system/source_image.h"
#include "user_profiles.h"

/* Shared worker envelope, not a list of official remote endpoints. Server
   requests and local save-backend operations share scheduling, not ownership.
   Local file operations are dispatched before ensureServer. */
/* 共用后台封套，不是官方远端接口列表。服务请求与本地存档操作仅共享调度，
   不共享归属；本地文件操作在 ensureServer 之前分派。 */
enum FsServiceOperation {
    FS_SERVICE_OPEN=1,
    FS_SERVICE_CONNECT,
    FS_SERVICE_DISCONNECT,
    FS_SERVICE_QUERY_BANK,
    FS_SERVICE_QUERY_RECORD,
    FS_SERVICE_DOWNLOAD_BANK,
    FS_SERVICE_CREATE_BANK,
    FS_SERVICE_STAGE_BANK,
    FS_SERVICE_QUERY_METADATA,
    FS_SERVICE_UPDATE_METADATA,
    FS_SERVICE_REQUEST_TRANSACTION,
    FS_SERVICE_RECOVERY_CODES,
    FS_SERVICE_AUTHORIZE_ROLLBACK,
    FS_SERVICE_NATIVE_SAVE_EVENT,
    FS_SERVICE_TRANSACTION_EVENT,
    FS_SERVICE_INITIAL_VALUES,
    /* 17 retired: gifts use the stock save flow, without a gift-only checkpoint. */
    /* 17 已撤除：礼物沿原保存流程，不再增加专用检查点。 */
    FS_SERVICE_WRITE_TURTLE=18,
    FS_SERVICE_READ_TURTLE,
    FS_SERVICE_READ_SOURCE,
    FS_SERVICE_WRITE_SOURCE,
    FS_SERVICE_OBSERVE_SOURCE,
    FS_SERVICE_BEGIN_SOURCE_SAVE,
    FS_SERVICE_SOURCE_SAVE_RESULT,
    FS_SERVICE_SETTLED_SOURCE,
    FS_SERVICE_GUARD,
    FS_SERVICE_READ_SOURCE_IMAGE,
    FS_SERVICE_GIFT_REPLY,
    FS_SERVICE_CAMPAIGN_RESOURCE,
    FS_SERVICE_FORMAT_TURTLE,
    FS_SERVICE_UPDATE_TURTLE_RECORD,
    FS_SERVICE_UPDATE_TURTLE_LANGUAGE,
    FS_SERVICE_PREPARE_TURTLE_RECORD,
    FS_SERVICE_EXPORT_BANK,
    /* 35 retired: ticket requests complete synchronously through their SDK adapter. */
    /* 35 已撤除：票务经 SDK 适配器同步完成，保留后续操作编号。 */
    FS_SERVICE_CHECK_MOVER=36,
    FS_SERVICE_TICKET_REPLY,
    FS_SERVICE_TICKET_ACTION,
    FS_SERVICE_READ_BANK_NATIVE_SETTINGS,
    FS_SERVICE_SYNC_MOVER_LANGUAGE,
    FS_SERVICE_TITLE_USERS,
    FS_SERVICE_OPERATION_END
};

/* Value snapshots, never native Pokemon/SDK pointers. */
/* 仅值快照，不含原版宝可梦／SDK 指针。 */
typedef struct FsMoverSlotSnapshot {
    FsMoverCandidate candidate;
    FsMoverDetails details;
} FsMoverSlotSnapshot;

typedef struct FsServiceRequest {
    uint32_t operation;
    union {
        struct { FsSourceMetadata metadata; uint32_t known; } download;
        struct {
            uint32_t kind,mode,gameCode,pathSize;
            FsTransactionFingerprint expected;
            FsSourceMetadata metadata;
            uint32_t bankSize,known;
        } stage;
        struct { uint64_t dataId; FsSourceMetadata metadata; uint32_t known; } metadata;
        struct { FsTransactionParameters descriptor; uint32_t commit,force; } transaction;
        struct { uint32_t challenge,enteredCode; } rollback;
        struct { FsTransactionParameters descriptor; uint32_t event; } native;
        struct { uint32_t action; } event;
        struct { uint32_t offset,size,activeSave; } file;
        struct { uint8_t record[0x30]; uint32_t activeSave; } turtle;
        /* createIfMissing requires a complete 512-byte native snapshot body. */
        /* createIfMissing 启用时，正文必须是完整的 512 字节原档快照。 */
        struct { uint32_t languageId,useJapaneseKanji,createIfMissing; } language;
        struct { FsGiftRequest request; uint32_t pass; } gift;
        struct {
            uint32_t onlineFallback,requestId,currentDate[2],region,country;
        } campaign;
        struct {
            uint32_t operation,textEncoding,onlineFallback,beginQuery;
            uint32_t currentDate[2],region,country;
        } ticket;
        struct {
            uint32_t applet,product,onlineFallback,currentDate[2],region,country;
        } shop;
        struct { uint32_t source,count,region,country; } checks;
        struct { uint32_t action,activeUser; } users; /* 0 catalogue, 1 commit */
        struct {
            uint32_t kind,mode,gameCode,pathSize,nativeResult;
            FsTransactionFingerprint fingerprint;
        } source;
    } input;
} FsServiceRequest;

typedef struct FsServiceReply {
    uint32_t bodySize;
    union {
        FsConnectionResponse connection;
        FsRemoteStatus bank;
        FsRecordStatus record;
        FsTransactionParameters descriptor;
        FsSourceMetadata metadata;
        FsSupportResponse rollback;
        FsTransactionFingerprint fingerprint;
        uint32_t initialValues[2];
        FsUserMenu users;
    } output;
    uint32_t customOptions;
    uint32_t storageIssue;
} FsServiceReply;

/* Context and I/O tables are process-owned. A task must never retain a remote
   object, SDK job, UI state, borrowed upload or a pointer to a selected game. */
/* 上下文及 I/O 表由进程持有；任务不保存远端对象、SDK 作业、界面状态、借用的
   上传正文或当前选择游戏的指针。 */
typedef struct FsServiceContext {
    FsServer *server;
    const FsStorageIo *storage;
    const FsTransactionIo *transactions;
    uint32_t capacity;
    uint32_t owner;
    /* Local file backend shares scheduling only, not server connection policy. */
    /* 本地文件后端仅共享调度，不共享服务器连接策略。 */
    FsLocalSource *localSource;
} FsServiceContext;

/* Serialized worker entry. NDS staging body = Bank bytes (optional), followed
   by exactly pathSize UTF-16 bytes. All pointer views are made inside this
   call and expire before it returns. This function does not deliver callbacks. */
/* 串行工作线程入口；NDS 暂存正文为可选银行正文后接 pathSize 字节 UTF-16 路径。
   指针视图只在本次调用内构造并使用，不向客户端投递回调。 */
int FsService_Execute(void *context,const void *input,uint32_t inputSize,
    void *output,uint32_t outputCapacity,uint32_t *outputSize,void *body,uint32_t bodySize);

#endif
