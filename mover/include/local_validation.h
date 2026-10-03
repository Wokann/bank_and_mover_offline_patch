#ifndef MOVER_OFFLINE_PATCH_LOCAL_VALIDATION_H
#define MOVER_OFFLINE_PATCH_LOCAL_VALIDATION_H

#include "mover_state.h"
#include "patch_types.h"

enum MoverLocalValidationConstant {
    MOVER_VALIDATION_SLOT_COUNT = 30,
    MOVER_GEN5_RECORD_SIZE = 136,
    MOVER_VALIDATION_RESULT_ACCEPTED = 0,
    MOVER_VALIDATION_RESULT_EMPTY = 20
};

/* The native shared flow object stores its per-slot results at offset 0x48. */
/* 原版共用流程对象在偏移 0x48 保存逐槽结果。 */
typedef struct MoverValidationSharedView {
    u8 reserved00[0x48];
    s32 result[MOVER_VALIDATION_SLOT_COUNT];
} MoverValidationSharedView;

typedef struct MoverGameManagerView {
    u8 reserved00[0x1C];
    void *gameData;
} MoverGameManagerView;

typedef void *(*MoverGetGen5CandidateBufferFn)(void *);

#define MOVER_RUNTIME_SINGLETON ((MoverGameManagerView * volatile *)0x00329380u)
#define MOVER_GET_GEN5_CANDIDATE_BUFFER \
    ((MoverGetGen5CandidateBufferFn)0x0019A6D8u)
#define MOVER_GEN5_EMPTY_RECORD_TEMPLATE ((const u8 *)0x002B3FD4u)

void OfflinePatch_PrepareGen5Validation(MoverStateView *state);
void OfflinePatch_PrepareGen12Validation(MoverStateView *state);

#endif
