#include "local_validation.h"

/* Classify integrity and occupancy without modifying the source record.
   Decoded words are temporary values; the native continuation decrypts its
   own copy and retains every local transfer check. */
/* 只分类记录完整性与占用状态，不修改来源记录。解码字仅作为临时值；原版续接
   路径会解码自己的副本，并保留全部本地传送检查。 */
static int gen5RecordValidationResult(const MoverGen5RecordView *record)
{
    u32 index;
    u32 seed=record->checksum;
    u32 checksum=0;
    u32 order=(record->pid>>MOVER_GEN5_BLOCK_ORDER_SHIFT)&
        MOVER_GEN5_BLOCK_ORDER_MASK;
    u32 speciesWord=MoverGen5_BlockOrderTable[
        order*MOVER_GEN5_BLOCK_ORDER_ROW_SIZE]/sizeof(u16);
    u16 species=0;
    u16 control=record->control;

    if (control&MOVER_GEN5_RECORD_EXCLUDED)
        return MOVER_VALIDATION_RESULT_REJECTED;
    for (index=0;index<MOVER_GEN5_BODY_WORD_COUNT;index++) {
        u16 value=record->body[index];
        if (!(control&MOVER_GEN5_BODY_DECODED)) {
            seed=seed*MOVER_GEN5_DECRYPT_MULTIPLIER+
                MOVER_GEN5_DECRYPT_INCREMENT;
            value^=(u16)(seed>>16);
        }
        checksum+=value;
        if (index==speciesWord) species=value;
    }
    if ((u16)checksum!=record->checksum)
        return MOVER_VALIDATION_RESULT_REJECTED;
    if (!species) return MOVER_VALIDATION_RESULT_EMPTY;
    if (species>MOVER_GEN5_SPECIES_MAX)
        return MOVER_VALIDATION_RESULT_REJECTED;
    return MOVER_VALIDATION_RESULT_ACCEPTED;
}

__attribute__((used,noinline,section(".text.offline.04_validation_gen5")))
int OfflinePatch_PrepareGen5Validation(MoverStateView *state)
{
    MoverValidationSharedView *shared;
    MoverGameManagerView *manager;
    const MoverGen5RecordView *records=0;
    u32 index;

    if (!state) return 0;
    if (!state->sharedData) {
        state->substate=MOVER_GEN5_VALIDATION_READ_ERROR;
        return 0;
    }
    shared=(MoverValidationSharedView *)state->sharedData;
    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        shared->result[index]=MOVER_VALIDATION_RESULT_REJECTED;
    manager=*MOVER_RUNTIME_SINGLETON;
    if (manager && manager->gameData)
        records=(const MoverGen5RecordView *)
            MOVER_GET_GEN5_CANDIDATE_BUFFER(manager->gameData);
    if (!records) {
        state->substate=MOVER_GEN5_VALIDATION_READ_ERROR;
        return 0;
    }

    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        shared->result[index]=gen5RecordValidationResult(records+index);
    return 1;
}

/* VC conversion already skips null Pokemon before consulting the server
   result. Clear every slot so a prior original-mode session cannot leak old
   server results into the offline conversion. */
/* VC 转换会在读取服务器结果前先跳过空宝可梦。这里清零全部槽位，防止此前
   原版模式会话的旧服务器结果残留到离线转换。 */
__attribute__((used,noinline,section(".text.offline.04_validation_gen12")))
void OfflinePatch_PrepareGen12Validation(MoverStateView *state)
{
    MoverValidationSharedView *shared;
    u32 index;
    if (!state || !state->sharedData) return;
    shared=(MoverValidationSharedView *)state->sharedData;
    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        shared->result[index]=MOVER_VALIDATION_RESULT_ACCEPTED;
}
