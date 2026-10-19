#ifndef BANK_OFFLINE_PATCH_BANKDATA_REDIRECT_H
#define BANK_OFFLINE_PATCH_BANKDATA_REDIRECT_H

#include "patch_types.h"
#include "fakeServer/types.h"
#include "fakeServer/native_response.h"

typedef void (*BankUi_SetMessageLineFn)(void *,u32,u32,u32);
#define BankUi_SetMessageLine ((BankUi_SetMessageLineFn)0x001E0EF8u)

enum BankTitleLanguageConstant {
    BANK_TITLE_LANGUAGE_PANE = 0x29,
    BANK_TITLE_LANGUAGE_MESSAGE = 122,
    BANK_TITLE_MODE_PANE = 0x2A,
    BANK_TITLE_SWITCH_PANE = 0x2B,
    BANK_TITLE_SWITCH_MESSAGE = 127,
    BANK_TITLE_HOME_MESSAGE = 77
};

struct BankFlowView;
void BankdataRedirect_TitleInitialize(void *view,u32 screen,u32 pane,u32 modeMessage);
int BankdataRedirect_SourceMetadata(struct BankFlowView *,FsSourceMetadata *);


/* Confirmed serialized Bankdata dimensions and field offsets. */
/* 已确认的 Bankdata 序列化尺寸与字段偏移。 */
enum BankdataRecordConstant {
    BANK_FILE_SIZE = 0xBB518,
    BANKDATA_HEADER_SIZE = 0x17C,
    BANKDATA_NAME_BUFFER_COUNT = 10,
    BANKDATA_NAME_CODE_UNIT_COUNT = 17,
    BANKDATA_FORMAT_VERSION = 2,
    BANKDATA_BOX_COUNT = 100,
    BANKDATA_SLOT_COUNT_PER_BOX = 30,
    BANKDATA_TRANSFER_SLOT_COUNT = 30,
    BANKDATA_SLOT_SIZE = 0xE8,
    BANKDATA_BOX_METADATA_SIZE = 0x26,
    BANKDATA_BOX_SIZE = 0x1B56,
    BANKDATA_BANK_SLOT_COUNT = 3000,
    BANKDATA_SOURCE_GAME_SUMMARY_COUNT = 8,
    BANKDATA_SOURCE_GAME_SUMMARY_SIZE = 0x44,
    BANKDATA_AGGREGATE_DATA_SIZE = 0x7260,
    BANKDATA_DEPOSIT_WITHDRAW_COUNTER_SIZE = 4,
    BANKDATA_TAIL_SIZE = 0x100
};

/* Only the field widths are confirmed for this eight-byte date block. */
/* 该 8 字节日期块目前只确认了字段宽度，不对各单字节含义作过度命名。 */
#pragma pack(push, 1)
typedef struct BankdataDateFields {
    u16 year;
    u8 components[4];
    u8 reserved[2];
} BankdataDateFields;

typedef struct BankdataHeader {
    u64 identityBoundValue;                                      /* 0x000 */
    u16 nameBuffers[BANKDATA_NAME_BUFFER_COUNT]
        [BANKDATA_NAME_CODE_UNIT_COUNT];                         /* 0x008 */
    u16 formatVersion;                                           /* 0x15C */
    u16 boxCount;                                                /* 0x15E */
    BankdataDateFields date;                                     /* 0x160 */
    u32 remoteContentIds[2];                                     /* 0x168 */
    u32 counters[2];                                             /* 0x170 */
    u8 flags[4];                                                 /* 0x178 */
} BankdataHeader;

typedef struct BankdataSlot {
    u8 bytes[BANKDATA_SLOT_SIZE];
} BankdataSlot;

typedef struct BankdataBox {
    BankdataSlot slots[BANKDATA_SLOT_COUNT_PER_BOX];
    u8 metadata[BANKDATA_BOX_METADATA_SIZE];
} BankdataBox;

typedef struct BankdataSourceGameSummary {
    u8 bytes[BANKDATA_SOURCE_GAME_SUMMARY_SIZE];
} BankdataSourceGameSummary;

typedef struct BankdataRecord {
    BankdataHeader header;
    BankdataBox bankBoxes[BANKDATA_BOX_COUNT];
    BankdataSlot transferSlots[BANKDATA_TRANSFER_SLOT_COUNT];
    u8 bankFormatTags[BANKDATA_BANK_SLOT_COUNT];
    u8 transferFormatTags[BANKDATA_TRANSFER_SLOT_COUNT];
    u8 reservedAlignment[2];
    BankdataSourceGameSummary sourceGameSummaries[BANKDATA_SOURCE_GAME_SUMMARY_COUNT];
    u8 aggregateData[BANKDATA_AGGREGATE_DATA_SIZE];
    u8 depositWithdrawCounters[BANKDATA_DEPOSIT_WITHDRAW_COUNTER_SIZE];
    u8 sourceSoftwareIds[BANKDATA_BANK_SLOT_COUNT];
    u64 updateTimestamps[BANKDATA_BANK_SLOT_COUNT];
    u8 tail[BANKDATA_TAIL_SIZE];
} BankdataRecord;
#pragma pack(pop)

struct BankRuntimeObjectView;

/* Partial views of the native Bank state-machine objects. Unknown spans remain
   explicit padding; only fields used by the patch are named. */
/* 原版 Bank 状态机对象的局部视图。未知区段保留为显式填充，仅命名补丁实际
   使用的字段。 */
typedef struct BankNetworkContextView {
    u8 reserved00[0x11];
    u8 connected;
} BankNetworkContextView;

typedef struct BankFlowView {
    u8 reserved00[0x24];
    BankNetworkContextView *networkContext;
    u8 reserved28[0x4C];
    void *turtleRecord;
    void *turtleStorage;
    u8 reserved7C[0x50];
    struct BankRuntimeObjectView *bankObject;
    u8 reservedD0[0x28];
    u32 metadataValue;
} BankFlowView;

typedef struct BankStateView {
    u8 reserved00[0x04];
    BankFlowView *networkFlow;
    BankFlowView *bankdataFlow;
    u8 reserved0C[0x04];
    u32 substate;
    u8 reserved14[0x14];
    void *sharedData;
    u8 reserved2C[0x04];
    u8 phase;
} BankStateView;

enum BankStatePhase {
    BANK_STATE_PHASE_ERROR = 3,
    BANK_STATE_PHASE_COMPLETE = 4
};

enum BankStateUpdateResult {
    BANK_STATE_UPDATE_WAITING = 0,
    BANK_STATE_UPDATE_FINISHED = 1
};

enum BankNetworkConnectionValue {
    BANK_NETWORK_DISCONNECTED = 0,
    BANK_NETWORK_CONNECTED = 1
};

/* Partial runtime object and first-use shared-data views. */
/* 运行时对象与首次使用共享数据的局部视图。 */
typedef struct BankRuntimeObjectView {
    u32 vtable;
    u32 reserved04;
    BankdataRecord record;
    void *metadata;
} BankRuntimeObjectView;

typedef struct BankInitialRecordView {
    u8 reserved00[0x40];
    NativeRemoteView *remote;
    u8 reserved44[4];
    u32 resourceId;
} BankInitialRecordView;

typedef struct BankDownloadStateView {
    u8 reserved00[0x3C];
    NativeRemoteView *remote;
} BankDownloadStateView;

typedef struct BankCreateStateView {
    u8 reserved00[0x40];
    NativeRemoteView *remote;
} BankCreateStateView;
typedef struct BankRemoteStateView {
    u8 reserved00[0x3C];
    NativeRemoteView *remote;
} BankRemoteStateView;
typedef struct BankSaveRemoteView {
    u8 reserved00[0x40];
    NativeRemoteView *remote;
    u8 reserved44[4];
    u8 finished;
} BankSaveRemoteView;
typedef struct BankSourceRecoveryView {
    u8 reserved00[0x3C];
    NativeRemoteView *remote;
    FsTransactionParameters transaction;
    u8 finished;
    u8 cancelled;
} BankSourceRecoveryView;
typedef struct BankAccountTransportView {
    u8 nativeInitialized;
    u8 reserved01[0x127];
    NativeRemoteResult *request;
    u8 reserved12C[8];
    void *provider;
} BankAccountTransportView;
typedef void *(*BankAccountConstructFn)(void *);
typedef void (*BankAccountResultFn)(u32 *,const u32 *);
#define BankAccount_Construct ((BankAccountConstructFn)0x001197FCu)
#define BankAccount_BuildResult ((BankAccountResultFn)0x0022A1FCu)
#define BankAccount_ParseInitial ((NativeRpcParserFn)0x0012C2E0u)

typedef struct BankCampaignResourceView {
    u8 reserved00[0x14];
    u32 resourceId;
} BankCampaignResourceView;
typedef int (*BankCampaignResponseFn)(void *,const BankCampaignResourceView *,u32,
    const FsCampaignRecord *);
#define BankCampaign_Response ((BankCampaignResponseFn)0x002ACF10u)

typedef struct BankGameInfoView BankGameInfoView;
typedef struct BankGameInfoVtable {
    void *reserved00[2];
    u32 (*nameFormat)(BankGameInfoView *);
    u32 (*gameId)(BankGameInfoView *);
    void *reserved10[3];
    u32 (*trainerId)(BankGameInfoView *);
    void *reserved20;
    void (*copyTrainerName)(BankGameInfoView *,u16 *);
} BankGameInfoVtable;
struct BankGameInfoView { BankGameInfoVtable *vtable; };
typedef union BankGameInfoLayout {
    struct { u8 reserved00[0x129AC]; BankGameInfoView info; } early;
    struct { u8 reserved00[0x74304]; BankGameInfoView info; } extended;
} BankGameInfoLayout;
typedef struct BankSelectedGameView {
    u8 reserved00[8];
    u8 version;
} BankSelectedGameView;
typedef void *(*BankGame_GetSelectedFn)(BankFlowView *);
#define BankGame_GetSelected ((BankGame_GetSelectedFn)0x00233A6Cu)

typedef struct BankForceRollbackView {
    u8 reserved00[0x3C];
    u32 challenge;
    u32 enteredCode;
    u8 cancelled;
    u8 reserved45[3];
    void *view;
    void *job;
    u8 finished;
    u8 failed;
} BankForceRollbackView;

/* Result of validating one local Bankdata candidate. */
/* 单个本地 Bankdata 候选文件的校验结果。 */
enum BankdataFileStatus {
    BANKDATA_FILE_IO_ERROR = -2,
    BANKDATA_FILE_INVALID = -1,
    BANKDATA_FILE_MISSING = 0,
    BANKDATA_FILE_VALID = 1
};

enum BankdataRecoveryConstant {
    BANKDATA_BROKEN_COPY_ATTEMPTS = 4,
    BANKDATA_COPY_BUFFER_SIZE = 0x400
};

typedef u32 (*BankFile_DeriveFlowMetadataFn)(void *);

#define BankFile_DeriveFlowMetadata ((BankFile_DeriveFlowMetadataFn)0x001D5EC0u)
#define BankFile_Vtable 0x003626FCu

int BankdataRedirect_InitialRecordUpdate(BankStateView *);
int BankdataRedirect_OriginalInitialRecordUpdate(BankStateView *);
int BankdataRedirect_DownloadUpdate(BankStateView *);
int BankdataRedirect_OriginalDownloadUpdate(BankStateView *);
int BankdataRedirect_CreateUpdate(BankStateView *);
int BankdataRedirect_OriginalCreateUpdate(BankStateView *);
int BankdataRedirect_InitializeAccount(BankAccountTransportView *);
int BankdataRedirect_UnbindAccount(BankAccountTransportView *);
enum BankDownloadCaptureStatus {
    BANK_DOWNLOAD_CAPTURE_NONE=0, BANK_DOWNLOAD_CAPTURE_COMPLETE=1,
    BANK_DOWNLOAD_CAPTURE_FAILED=2, BANK_DOWNLOAD_CAPTURE_PENDING=3
};
#define BankDownload_CaptureStatusStorage ((volatile u8 *)0x003FAFF1u)
int BankdataRedirect_CaptureDownloaded(BankStateView *,BankRuntimeObjectView *);
int BankdataRedirect_ExportDownloadUpdate(BankStateView *);
void BankdataRedirect_CancelDownloaded(uintptr_t peer);
int BankdataRedirect_BindRemote(NativeRemoteView *);
int BankdataRedirect_OriginalBindRemote(NativeRemoteView *,const void *);
int BankdataRedirect_TransactionProvider(NativeRemoteResult *,NativeRemoteView *,u32 commit,u32 force);
int BankdataRedirect_RecoveryTokenProvider(NativeRemoteResult *,NativeRemoteView *,u32 challenge);
int BankdataRedirect_NoSaveUpdate(BankStateView *);
int BankdataRedirect_OriginalNoSaveUpdate(BankStateView *);
int BankdataRedirect_TransactionRecoveryUpdate(BankStateView *);

#define BankRemote_BuildResult ((NativeRemoteResultBuildFn)0x0022A338u)
#define BankRemote_StageResponse ((NativeRemoteResponseFn)0x002A3424u)
#define BankRemote_CommitResponse ((NativeRemoteResponseFn)0x002A3268u)
#define BankRemote_RollbackResponse ((NativeRemoteResponseFn)0x002A2CB4u)
#define BankRemote_MetadataResponse ((NativeRemoteResponseFn)0x002A2A14u)
#define BankRemote_CompleteJob NativeResponse_Complete
#define BankRemote_Allocate ((NativeResponseAllocateFn)0x0022A488u)
int BankdataRedirect_SaveUpdate(BankStateView *state);
int BankdataRedirect_OriginalSaveUpdate(BankStateView *state);
int BankdataRedirect_SourceRecoveryUpdate(BankStateView *state);
int BankdataRedirect_OriginalSourceRecoveryUpdate(BankStateView *state);
int BankdataRedirect_OriginalTransactionRecoveryUpdate(BankStateView *);
int BankdataRedirect_ForceRollbackUpdate(BankStateView *);
int BankdataRedirect_OriginalForceRollbackUpdate(BankStateView *);

#endif
