#ifndef BANK_OFFLINE_PATCH_BANKDATA_REDIRECT_H
#define BANK_OFFLINE_PATCH_BANKDATA_REDIRECT_H

#include "patch_types.h"

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
    u8 reserved28[0xA4];
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
    u8 reserved31[0x17];
    u8 saveDelayMarker;
    u8 reserved49[0x18];
    u8 postSelectionDelayMarker;
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

typedef struct BankInitialRecordSharedView {
    u8 reserved00[0x20];
    u8 mode;
    u8 reserved21[0x27];
    u16 counter;
    u8 reserved4A[0x02];
    u8 flag0;
    u8 flag1;
    u8 flag2;
} BankInitialRecordSharedView;

/* Result of validating one local Bankdata candidate. */
/* 单个本地 Bankdata 候选文件的校验结果。 */
enum BankdataFileStatus {
    BANKDATA_FILE_INVALID = -1,
    BANKDATA_FILE_MISSING = 0,
    BANKDATA_FILE_VALID = 1
};

enum BankInitialRecordMode {
    BANK_INITIAL_RECORD_MODE_EXISTING = '4',
    BANK_INITIAL_RECORD_MODE_FIRST_USE = '5'
};

enum BankdataRecoveryConstant {
    BANKDATA_BROKEN_COPY_ATTEMPTS = 4,
    BANKDATA_COPY_BUFFER_SIZE = 0x400
};

typedef u32 (*BankFile_DeriveFlowMetadataFn)(void *);

#define BankFile_DeriveFlowMetadata ((BankFile_DeriveFlowMetadataFn)0x001D5EC0u)
#define BankFile_Vtable 0x003626FCu

int OfflinePatch_InitialRemoteRecordUpdate(BankStateView *state);
int OfflinePatch_LoadBankData(BankStateView *state);
int OfflinePatch_CreateInitial(void *remote,const void *data,u32 size);
int BankdataRedirect_CaptureDownloaded(BankRuntimeObjectView *object);
int OfflinePatch_Stage(void *remote,const void *data,u32 size,void *transaction);
int OfflinePatch_Commit(void *remote,void *transaction,u32 zero);
int OfflinePatch_Rollback(void *remote,void *transaction,u32 zero);

#endif
