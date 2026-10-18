#ifndef MOVER_SERVER_VALIDATION_H
#define MOVER_SERVER_VALIDATION_H

#include "mover_state.h"
#include "patch_types.h"
#include "fakeServer/native_response.h"

enum MoverValidationConstant {
    MOVER_VALIDATION_SLOT_COUNT = 30,
    MOVER_GEN5_RECORD_SIZE = 136,
    MOVER_GEN5_BODY_WORD_COUNT = 64,
    MOVER_GEN5_SPECIES_MAX = 649,
    MOVER_GEN5_BLOCK_ORDER_ROW_SIZE = 4,
    MOVER_GEN5_BLOCK_ORDER_SHIFT = 13,
    MOVER_GEN5_BLOCK_ORDER_MASK = 31,
    MOVER_GEN5_DECRYPT_MULTIPLIER = 0x41C64E6D,
    MOVER_GEN5_DECRYPT_INCREMENT = 0x6073,
    MOVER_VALIDATION_RECORD_STRIDE = 0xE8,
    MOVER_VALIDATION_GEN5 = 0x600,
    MOVER_VALIDATION_VC = 0x800
};

enum MoverGen5RecordControl {
    MOVER_GEN5_BODY_DECODED = 0x02,
    MOVER_GEN5_RECORD_EXCLUDED = 0x04
};

/* The native BOX1 buffer contains aligned, fixed-size Gen 5 records. */
/* 原版 BOX1 缓冲区由对齐、固定长度的第五世代记录组成。 */
typedef struct MoverGen5RecordView {
    u32 pid;
    u16 control;
    u16 checksum;
    u16 body[MOVER_GEN5_BODY_WORD_COUNT];
} MoverGen5RecordView;

typedef struct MoverVcCandidateView {
    u8 reserved00[0x0C];
    void *record;
} MoverVcCandidateView;
typedef struct MoverVcValidationStateView {
    u8 reserved00[0x3C];
    struct MoverValidationRequestView *request;
    u8 reserved40[0x0C];
    MoverVcCandidateView *candidates[MOVER_VALIDATION_SLOT_COUNT];
} MoverVcValidationStateView;

typedef struct MoverValidationVectorView {
    const u8 *begin;
    const u8 *end;
    const u8 *capacity;
} MoverValidationVectorView;
typedef struct MoverValidationPayloadView {
    void *vtable;
    void *replyVtable;
    MoverValidationVectorView *body;
    u32 httpStatus;
} MoverValidationPayloadView;
typedef struct MoverValidationClientView {
    void *vtable;
    u8 initialized,busy;
    u8 reserved06[2];
    NativeRemoteResult *job;
    void *transport;
    void *listeners[4];
    MoverValidationPayloadView *payload;
} MoverValidationClientView;
typedef struct MoverValidationRequestView {
    void *vtable;
    u8 initialized,operation;
    u8 reserved06[0x104];
    u16 format;
    u8 reserved10C[4];
    u8 records[MOVER_VALIDATION_SLOT_COUNT][MOVER_VALIDATION_RECORD_STRIDE];
    u8 reserved1C40[0x12CC];
    u32 count;
    void *callback;
    u32 allocator;
    MoverValidationClientView *client;
} MoverValidationRequestView;
typedef u32 (*Pokemon_CalculateLevelFn)(u32 species,u32 form,u32 experience,u32 format);
#define Pokemon_CalculateLevel ((Pokemon_CalculateLevelFn)0x001D32E8u)
typedef void (*Pokemon_AccessFn)(void *);
typedef const u8 *(*Pokemon_GetBlockFn)(void *,u32 writing);
#define Pokemon_BeginAccess ((Pokemon_AccessFn)0x001D352Cu)
#define Pokemon_EndAccess ((Pokemon_AccessFn)0x001D34BCu)
#define Pokemon_GetGrowthBlock ((Pokemon_GetBlockFn)0x001CAEF0u)
#define Pokemon_GetMoveBlock ((Pokemon_GetBlockFn)0x001CB1E4u)
#define Pokemon_GetTrainerBlock ((Pokemon_GetBlockFn)0x001C9060u)
typedef struct MoverPokemonAccessView {
    u8 reserved00[8];
    const u8 *raw;
} MoverPokemonAccessView;

#define MoverGen5_BlockOrderTable ((const u8 *)0x002B42E4u)
typedef int (*MoverValidationAppendFn)(MoverValidationPayloadView *,const u8 *,u32);
#define MoverValidation_Append ((MoverValidationAppendFn)0x00240698u)
#define MoverValidation_Complete NativeResponse_Complete
#define MoverValidation_Result ((NativeRemoteResultBuildFn)0x001DC5CCu)
#define MoverValidation_Allocate ((NativeResponseAllocateFn)0x001DC71Cu)
#define MoverValidation_Free ((NativeResponseReleaseFn)0x001D3244u)

void ServerValidation_Start(u32 *accepted,void *client,NativeRemoteResult *job,
    const void *resource,void *payload,u32 flags);
void ServerValidation_Poll(MoverStateView *state);
void ServerValidation_UnbindClient(MoverValidationClientView *);
void ServerValidation_OriginalUnbindClient(MoverValidationClientView *) __attribute__((long_call));

#endif
