#ifndef MOVER_OFFLINE_PATCH_BANKDATA_REDIRECT_H
#define MOVER_OFFLINE_PATCH_BANKDATA_REDIRECT_H

#include "mover_state.h"
#include "patch_types.h"
#include "fakeServer/types.h"
#include "fakeServer/native_response.h"

typedef void (*MoverUi_SetMessageLineFn)(void *,u32,u32,u32);
#define MoverUi_SetMessageLine ((MoverUi_SetMessageLineFn)0x0019C410u)

enum MoverTitleHint { MOVER_TITLE_MODE_PANE = 43, MOVER_TITLE_SWITCH_PANE = 44,
    MOVER_TITLE_SWITCH_MESSAGE = 73 };

void BankdataRedirect_TitleInitialize(void *view,u32 screen,u32 pane,u32 modeMessage);

typedef FsNativeTransaction MoverSourceTransactionView;

typedef void (*MoverUi_ShowMessageLineFn)(void *,u32,u32);
typedef u32 (*MessageView_GetResultFn)(void *);
typedef void *(*MoverSource_TransactionFn)(void *,MoverSourceTransactionView *);
#define MoverUi_ShowMessageLine ((MoverUi_ShowMessageLineFn)0x0019B50Cu)
#define MessageView_GetResult ((MessageView_GetResultFn)0x0019B4BCu)
#define MoverSource_GetTransaction ((MoverSource_TransactionFn)0x0019BA68u)
#define MoverSource_SetTransaction ((MoverSource_TransactionFn)0x0019A13Cu)


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
    u64 identityBoundValue;
    u16 nameBuffers[BANKDATA_NAME_BUFFER_COUNT][BANKDATA_NAME_CODE_UNIT_COUNT];
    u16 formatVersion;
    u16 boxCount;
    BankdataDateFields date;
    u32 remoteContentIds[2];
    u32 counters[2];
    u8 flags[4];
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

/* Partial runtime Bankdata object used by the Mover flow. */
/* Mover 流程使用的运行时 Bankdata 对象局部视图。 */
typedef struct MoverBankRuntimeObjectView {
    u32 vtable;
    u32 reserved04;
    BankdataRecord record;
    u32 reservedBB520;
    void *transferBox;
} MoverBankRuntimeObjectView;

#define BankFile_Vtable 0x002E6DD0u

int BankdataRedirect_EligibilityUpdate(MoverStateView *state);
int BankdataRedirect_SourceMetadata(MoverFlowView *,FsSourceMetadata *);
int BankdataRedirect_OriginalEligibilityUpdate(MoverStateView *) __attribute__((long_call));
int BankdataRedirect_BindRemote(NativeRemoteView *);
int BankdataRedirect_OriginalBindRemote(NativeRemoteView *,const void *) __attribute__((long_call));
int BankdataRedirect_TransactionProvider(NativeRemoteResult *,NativeRemoteView *,u32 commit,u32 force);
int BankdataRedirect_RemoteCheckUpdate(MoverStateView *);
int BankdataRedirect_OriginalRemoteCheckUpdate(MoverStateView *) __attribute__((long_call));
int BankdataRedirect_NoSaveUpdate(MoverStateView *);
int BankdataRedirect_OriginalNoSaveUpdate(MoverStateView *) __attribute__((long_call));
int BankdataRedirect_TransactionRecoveryUpdate(MoverStateView *);

#define MoverRemote_BuildResult ((NativeRemoteResultBuildFn)0x001DC5CCu)
#define MoverRemote_StageResponse ((NativeRemoteResponseFn)0x0023F588u)
#define MoverRemote_CommitResponse ((NativeRemoteResponseFn)0x0023F3CCu)
#define MoverRemote_RollbackResponse ((NativeRemoteResponseFn)0x0023F058u)
#define MoverRemote_MetadataResponse ((NativeRemoteResponseFn)0x0023EDB8u)
#define MoverRemote_CompleteJob NativeResponse_Complete
#define MoverRemote_Allocate ((NativeResponseAllocateFn)0x001DC71Cu)

enum MoverSavePhase {
    MOVER_SAVE_PHASE_ERROR = 2,
    MOVER_SAVE_PHASE_COMPLETE = 3
};

int BankdataRedirect_SaveUpdate(MoverStateView *state);
int BankdataRedirect_OriginalSaveUpdate(MoverStateView *state) __attribute__((long_call));

/* Recovery-state fields independently confirmed by the native update functions. */
/* 由原版更新函数独立确认的恢复状态字段。 */
typedef struct MoverSourceRecoveryView {
    u8 reserved00[0x34];
    void *callbackVtable;
    void *view;
    void *job;
    FsTransactionParameters transaction;
    u8 finished;
    u8 cancelled;
} MoverSourceRecoveryView;

typedef struct MoverRemoteCheckView {
    u8 reserved00[0x3C];
    NativeRemoteView *remote;
} MoverRemoteCheckView;
typedef struct MoverTransactionRecoveryView {
    u8 reserved00[0x40];
    NativeRemoteView *remote;
} MoverTransactionRecoveryView;

int BankdataRedirect_SourceRecoveryUpdate(MoverStateView *);
int BankdataRedirect_OriginalSourceRecoveryUpdate(MoverStateView *) __attribute__((long_call));
int BankdataRedirect_OriginalTransactionRecoveryUpdate(MoverStateView *) __attribute__((long_call));

#endif
