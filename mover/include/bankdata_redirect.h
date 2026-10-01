#ifndef MOVER_OFFLINE_PATCH_BANKDATA_REDIRECT_H
#define MOVER_OFFLINE_PATCH_BANKDATA_REDIRECT_H

#include "mover_state.h"
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

/* Transfer-object constants and stock entry points. */
/* 传送对象常量及原版入口。 */
enum MoverTransferObjectConstant {
    TRANSFER_OBJECT_ALLOCATION_SIZE = 0x14
};

enum MoverLocalBankLoadResult {
    MOVER_LOCAL_BANK_LOAD_ERROR = -1,
    MOVER_LOCAL_BANK_LOAD_COMPLETE = 0,
    MOVER_LOCAL_BANK_TRANSFER_BOX_OCCUPIED = 1
};

typedef void (*TransferObject_DestroyFn)(void *);

typedef struct TransferObjectVtableView {
    void *reserved00;
    void *reserved04;
    TransferObject_DestroyFn destroy;
} TransferObjectVtableView;

typedef struct TransferObjectView {
    TransferObjectVtableView *vtable;
} TransferObjectView;

typedef void *(*Heap_AllocateObjectFn)(u32,u32);
typedef TransferObjectView *(*TransferObject_CreateFn)(void *,u32,u32);
typedef int (*TransferSlot_IsOccupiedFn)(void *,u32);
typedef void (*TransferSlot_LoadFn)(void *,void *,u32);
typedef void (*TransferSlot_ClearFn)(void *,u32);
typedef void (*TransferSlot_WriteFn)(void *,void *,u32);
typedef int (*TransferSlot_CountOccupiedFn)(void *);

#define Heap_AllocateObject ((Heap_AllocateObjectFn)0x001E6360u)
#define TransferObject_Create ((TransferObject_CreateFn)0x001DFBA4u)
#define TransferSlot_IsOccupied ((TransferSlot_IsOccupiedFn)0x0019A858u)
#define TransferSlot_Load ((TransferSlot_LoadFn)0x0019A224u)
#define TransferSlot_Clear ((TransferSlot_ClearFn)0x0019A7E0u)
#define TransferSlot_Write ((TransferSlot_WriteFn)0x0019A6F4u)
#define TransferSlot_CountOccupied ((TransferSlot_CountOccupiedFn)0x0024D624u)
#define BankFile_Vtable 0x002E6DD0u

int OfflinePatch_EligibilityUpdate(MoverStateView *state);
int OfflinePatch_Stage(void *remote,const void *data,u32 size,void *transaction);
int OfflinePatch_Commit(void *remote,void *transaction,u32 zero);
int OfflinePatch_Rollback(void *remote,void *transaction,u32 zero);

#endif
