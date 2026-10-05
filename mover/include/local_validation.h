#ifndef MOVER_OFFLINE_PATCH_LOCAL_VALIDATION_H
#define MOVER_OFFLINE_PATCH_LOCAL_VALIDATION_H

#include "mover_state.h"
#include "patch_types.h"

enum MoverLocalValidationConstant {
    MOVER_VALIDATION_SLOT_COUNT = 30,
    MOVER_GEN5_RECORD_SIZE = 136,
    MOVER_GEN5_BODY_WORD_COUNT = 64,
    MOVER_GEN5_SPECIES_MAX = 649,
    MOVER_GEN5_BLOCK_ORDER_ROW_SIZE = 4,
    MOVER_GEN5_BLOCK_ORDER_SHIFT = 13,
    MOVER_GEN5_BLOCK_ORDER_MASK = 31,
    MOVER_GEN5_DECRYPT_MULTIPLIER = 0x41C64E6D,
    MOVER_GEN5_DECRYPT_INCREMENT = 0x6073,
    MOVER_VALIDATION_RESULT_ACCEPTED = 0,
    MOVER_VALIDATION_RESULT_REJECTED = 1,
    MOVER_VALIDATION_RESULT_EMPTY = 20
};

enum MoverGen5RecordControl {
    MOVER_GEN5_BODY_DECODED = 0x02,
    MOVER_GEN5_RECORD_EXCLUDED = 0x04
};

enum MoverGen5ValidationSubstate {
    MOVER_GEN5_VALIDATION_READ_ERROR = 13
};

/* The native BOX1 buffer contains aligned, fixed-size Gen 5 records. */
/* 原版 BOX1 缓冲区由对齐、固定长度的第五世代记录组成。 */
typedef struct MoverGen5RecordView {
    u32 pid;
    u16 control;
    u16 checksum;
    u16 body[MOVER_GEN5_BODY_WORD_COUNT];
} MoverGen5RecordView;

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
#define MoverGen5_BlockOrderTable ((const u8 *)0x002B42E4u)

int OfflinePatch_PrepareGen5Validation(MoverStateView *state);
void OfflinePatch_PrepareGen12Validation(MoverStateView *state);

#endif
