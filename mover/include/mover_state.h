#ifndef MOVER_OFFLINE_PATCH_MOVER_STATE_H
#define MOVER_OFFLINE_PATCH_MOVER_STATE_H

#include "patch_types.h"

struct MoverBankRuntimeObjectView;

/* Prefix of the shared flow data populated by the remote transaction query.
   The explicit padding preserves the original ARM layout before the final
   64-bit transaction password. */
/* 远端事务查询所填充的共用流程数据前缀。显式填充用于保持最后一个 64 位
   事务密码之前的原版 ARM 布局。 */
typedef struct MoverTransactionParamView {
    u64 dataId;
    u32 currentVersion;
    u32 updateVersion;
    u32 size;
    u32 reserved14;
    u64 transactionPassword;
} MoverTransactionParamView;

typedef struct MoverCommonDataPrefixView {
    MoverTransactionParamView transaction;
    u32 bankStatus;
} MoverCommonDataPrefixView;

/* Confirmed fields shared by the patched Mover state-machine objects. */
/* 补丁涉及的 Mover 状态机对象中已确认的共用字段。 */
typedef struct MoverNetworkContextView {
    u8 reserved00[0x11];
    u8 connected;
} MoverNetworkContextView;

typedef struct MoverFlowView {
    u8 reserved00[0x24];
    MoverNetworkContextView *networkContext;
    u8 reserved28[0xA4];
    struct MoverBankRuntimeObjectView *bankObject;
} MoverFlowView;

typedef struct MoverStateView {
    u8 reserved00[0x04];
    MoverFlowView *networkFlow;
    MoverFlowView *bankdataFlow;
    u32 heapId;
    u32 substate;
    u8 reserved14[0x14];
    void *sharedData;
    u8 reserved2C[0x04];
    u8 phase;
    u8 reserved31[0x13];
    u8 saveDelayMarker;
} MoverStateView;

enum MoverStateUpdateResult {
    MOVER_STATE_UPDATE_WAITING = 0,
    MOVER_STATE_UPDATE_FINISHED = 1
};

enum MoverNetworkConnectionValue {
    MOVER_NETWORK_DISCONNECTED = 0,
    MOVER_NETWORK_CONNECTED = 1
};

enum MoverBankStatusValue {
    MOVER_BANK_STATUS_READY = 50
};

/* These phase values belong to different native state classes and therefore
   intentionally keep state-specific names. */
/* 这些阶段值分属不同原版状态类，因此有意保留各自的状态专用名称。 */
enum MoverNetworkStatePhase {
    MOVER_NETWORK_PHASE_COMPLETE = 3
};

enum MoverRemoteCheckStatePhase {
    MOVER_REMOTE_CHECK_PHASE_COMPLETE = 8
};

enum MoverEligibilityStatePhase {
    MOVER_ELIGIBILITY_PHASE_COMPLETE = 4,
    MOVER_ELIGIBILITY_PHASE_ERROR = 5
};

enum MoverNoTransferStatePhase {
    MOVER_NO_TRANSFER_PHASE_COMPLETE = 3
};

enum MoverEligibilitySubstate {
    MOVER_ELIGIBILITY_SUBSTATE_TRANSFER_BOX_OCCUPIED = 7
};

enum MoverSaveSubstate {
    MOVER_SAVE_SUBSTATE_LOCAL_COMPLETE = 3
};

#endif
