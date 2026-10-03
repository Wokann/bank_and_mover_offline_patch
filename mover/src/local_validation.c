#include "local_validation.h"

/* Reconstruct only the server result needed by the native local converter.
   A record matching the native empty-slot template receives the native no-data
   result (20); every other record continues through native checks. */
/* 只重建原版本地转换器所需的服务器结果。与原版空槽模板匹配的第五世代记录
   使用“无数据”结果 20；其他记录仍继续经过原版本地合法性检查。 */
static int gen5RecordIsEmpty(const u8 *record)
{
    u32 offset;
    for (offset=0;offset<MOVER_GEN5_RECORD_SIZE;offset++) {
        if (record[offset]!=MOVER_GEN5_EMPTY_RECORD_TEMPLATE[offset]) return 0;
    }
    return 1;
}

__attribute__((used,noinline,section(".text.offline.04_validation_gen5")))
void OfflinePatch_PrepareGen5Validation(MoverStateView *state)
{
    MoverValidationSharedView *shared;
    MoverGameManagerView *manager;
    const u8 *records=0;
    u32 index;

    if (!state || !state->sharedData) return;
    shared=(MoverValidationSharedView *)state->sharedData;
    manager=*MOVER_RUNTIME_SINGLETON;
    if (manager && manager->gameData)
        records=(const u8 *)MOVER_GET_GEN5_CANDIDATE_BUFFER(manager->gameData);

    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++) {
        shared->result[index]=(records &&
            gen5RecordIsEmpty(records+index*MOVER_GEN5_RECORD_SIZE))?
            MOVER_VALIDATION_RESULT_EMPTY:MOVER_VALIDATION_RESULT_ACCEPTED;
    }
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
