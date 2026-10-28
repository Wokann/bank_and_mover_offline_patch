#ifndef MOVER_OFFLINE_PATCH_NDS_SOURCES_H
#define MOVER_OFFLINE_PATCH_NDS_SOURCES_H

#include <stddef.h>
#include "fs_system/fs_utf16.h"
#include "fakeServer/fake_server_client.h"
#include "mover_state.h"
#include "patch_types.h"

enum NdsSourceConstant {
    NDS_SOURCE_LIST_CAPACITY = 40,
    NDS_GAME_COUNT = 4,
    NDS_SAVE_SIZE = 0x80000,
    NDS_DISPLAY_RECORD_SIZE = 0x24,
    NDS_PATH_CAPACITY = 0x120,
    NDS_ROM_HEADER_READ_SIZE = 0x10,
    NDS_GAME_TITLE_MESSAGE_BASE = 54,
    NDS_BACKEND_CARD = 0,
    NDS_BACKEND_SD = 1
};

enum NdsScanPhase {
    NDS_SCAN_INACTIVE = 0,
    NDS_SCAN_PHYSICAL,
    NDS_SCAN_DIRECTORY,
    NDS_SCAN_VALIDATION,
    NDS_SCAN_PASSTHROUGH
};

enum NdsWinnerKind {
    NDS_WINNER_NONE = 0,
    NDS_WINNER_CARD,
    NDS_WINNER_SD
};

typedef struct NdsSourceWinner {
    u8 valid;
    u8 kind;
    u8 rank;
    u8 sourceId;
    u32 gameCode;
    u8 display[NDS_DISPLAY_RECORD_SIZE];
    u16 savePath[NDS_PATH_CAPACITY];
    u32 savePathSize;
} NdsSourceWinner;

typedef struct NdsScannerContext {
    volatile u32 backend;
    u32 phase;
    u32 activeGameCode;
    u32 activePathSize;
    u32 candidateIndex;
    u32 candidateRank;
    u64 directoryArchive;
    u32 directoryHandle;
    NdsSourceWinner winners[NDS_GAME_COUNT];
    FsDirectoryEntry entry;
    u16 path[NDS_PATH_CAPACITY];
    u8 header[NDS_ROM_HEADER_READ_SIZE];
    FsTransactionFingerprint selectedSave;
    u32 selectedSaveValid;
    volatile u32 workActive;
    volatile u32 workError;
    volatile u32 workBytesWritten;
    u8 plannedBoxHash[FS_TRANSACTION_HASH_SIZE];
    u8 plannedTransactionHash[FS_TRANSACTION_HASH_SIZE];
    u32 saveResult;
    u32 saveBlocked;
    u32 publicationError;
    MoverStateView *saveState;
    void *sourceContext;
    u32 saveStep;
    u32 pendingSaveKind;
    u32 nativeSaveResult;
    u32 settlementReady;
    FsTransactionFingerprint validatedWork;
    u32 selectionSourceId;
    u32 selectionReady;
    volatile u32 presencePending;
    volatile u32 presenceFound;
} NdsScannerContext;

/* The save job owns the complete image; selected-face accessors are not a
   substitute for the complete-image validator. */
/* 保存作业持有完整镜像；选定存档面的访问器不能替代完整镜像校验。 */
typedef struct MoverNdsImageView {
    u8 reserved00[0xD8];
    u8 *saveImage;
} MoverNdsImageView;

#define NDS_SCANNER_CONTEXT_SLOT ((NdsScannerContext **)0x00363FF0u)

/* Fields used by the stock NDS/VC source-list state. */
/* 原版 NDS／VC 来源列表状态中本功能实际使用的字段。 */
typedef struct MoverSourceListStateView {
    u8 reserved00[0x08];
    void *sourceContext;
    u8 reserved0C[0x04];
    u32 state;
    u8 reserved14[0x18];
    void *flow;
    u8 reserved30[0x08];
    u8 *listUi;
    u8 sourceIds[NDS_SOURCE_LIST_CAPACITY];
    u16 sourceCount;
    u16 cursor;
    void *workerArena;
    u8 reserved6C[0x0C];
    void *vcEnumerator;
} MoverSourceListStateView;

/* The availability state has its own layout; only its state word is shared. */
/* 前置来源检查使用独立布局，只访问它与列表状态同偏移的状态字。 */
typedef struct MoverSourceAvailabilityStateView {
    u8 reserved00[0x10];
    u32 state;
} MoverSourceAvailabilityStateView;

_Static_assert(offsetof(MoverSourceAvailabilityStateView,state)==0x10,
    "native availability state word");
_Static_assert(offsetof(MoverSourceListStateView,sourceIds)==0x3C &&
    offsetof(MoverSourceListStateView,sourceCount)==0x64 &&
    offsetof(MoverSourceListStateView,cursor)==0x66 &&
    offsetof(MoverSourceListStateView,workerArena)==0x68,
    "native source-list layout");

typedef u32 (*MoverSourceList_UpdateFn)(MoverSourceListStateView *,u32,u32,s32);
typedef u32 (*MoverSourceAvailability_UpdateFn)(MoverSourceAvailabilityStateView *,u32,u32,s32);
typedef void (*MoverNds_StartLoadFn)(void *,void *);
typedef int (*MoverNds_PollLoadFn)(void *,s32 *);
typedef int (*MoverNds_ValidateSaveFn)(int,const void *);
typedef void (*MoverNds_SelectSaveFaceFn)(void *,u32);
typedef void *(*MoverNds_GetPointerFn)(void *);
typedef u32 (*MoverNds_GetValueFn)(void *);
typedef void *(*MoverHeap_AllocateFn)(u32);
typedef void *(*MoverMemory_ClearFn)(void *,u32);
typedef void *(*MoverMemory_CopyFn)(void *,const void *,u32);

#define MoverSourceList_Update ((MoverSourceList_UpdateFn)0x00244D2Cu)
#define MoverSourceAvailability_Update ((MoverSourceAvailability_UpdateFn)0x00246F04u)
#define MoverNds_StartLoad ((MoverNds_StartLoadFn)0x0019ADECu)
#define MoverVc_StartLoad ((MoverNds_StartLoadFn)0x00153BD8u)
typedef int (*MoverVc_LoadFn)(void *,void *);
#define MoverVc_Load ((MoverVc_LoadFn)0x00153B48u)
#define MoverNds_PollLoad ((MoverNds_PollLoadFn)0x0019AD60u)
#define MoverNds_ValidateSave ((MoverNds_ValidateSaveFn)0x0019AA30u)
#define MoverNds_SelectSaveFace ((MoverNds_SelectSaveFaceFn)0x0019A9FCu)
#define MoverNds_GetSelectedSave ((MoverNds_GetPointerFn)0x0019C154u)
#define MoverNds_GetBox1 ((MoverNds_GetPointerFn)0x0019A6D8u)
#define MoverNds_GetDisplayName ((MoverNds_GetPointerFn)0x0019A994u)
#define MoverNds_GetDisplayValue ((MoverNds_GetValueFn)0x0019A970u)
#define MoverNds_GetDisplayFlag ((MoverNds_GetValueFn)0x0019A94Cu)
#define MoverHeap_Allocate ((MoverHeap_AllocateFn)0x001D1D0Cu)
#define MoverMemory_Clear ((MoverMemory_ClearFn)0x001E6D84u)
#define MoverMemory_Copy ((MoverMemory_CopyFn)0x001E6F8Cu)

/* Assembly trampolines replay the overwritten stock prologues. */
/* 汇编跳板会重放被覆盖的原版函数序言。 */
#if defined(__thumb__)
#define NDS_ARM_TRAMPOLINE __attribute__((long_call))
#else
#define NDS_ARM_TRAMPOLINE
#endif
s32 NdsSources_OriginalReadSave(u32 device,u32 offset,void *data,u32 size)
    NDS_ARM_TRAMPOLINE;
s32 NdsSources_OriginalReadGameCode(u32 *gameCode) NDS_ARM_TRAMPOLINE;
s32 NdsSources_OriginalWriteSave(u32 device,u32 offset,const void *data,u32 size)
    NDS_ARM_TRAMPOLINE;
void NdsSources_OriginalSaveBegin(MoverStateView *state,u32 kind) NDS_ARM_TRAMPOLINE;
u32 NdsSources_OriginalSavePoll(MoverStateView *state) NDS_ARM_TRAMPOLINE;
u32 NdsSources_OriginalSelectionUpdate(MoverSourceListStateView *,u32,u32,s32)
    NDS_ARM_TRAMPOLINE;
#undef NDS_ARM_TRAMPOLINE

u32 NdsSources_ListUpdate(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3);
u32 NdsSources_AvailabilityUpdate(MoverSourceAvailabilityStateView *,u32,u32,s32);
u32 NdsSources_SelectListId(u32 sourceId);
u32 NdsSources_SelectionUpdate(MoverSourceListStateView *,u32,u32,s32);
u32 NdsSources_SelectionReady(MoverSourceListStateView *);
u32 NdsSources_TitleMessage(const MoverSourceListStateView *state,u32 originalMessage);
s32 NdsSources_ReadSave(u32 device,u32 offset,void *data,u32 size);
s32 NdsSources_ReadGameCode(u32 *gameCode);
s32 NdsSources_WriteSave(u32 device,u32 offset,const void *data,u32 size);
int NdsSources_GetSelectedSource(FsSource *);
void NdsSources_SaveBegin(MoverStateView *state,u32 kind);
u32 NdsSources_SavePoll(MoverStateView *state);
int NdsSources_PublicationFailed(void);
int NdsSources_SaveCompleted(void);

#endif
