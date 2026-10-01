#include "bankdata_redirect.h"
#include "fs_helpers.h"
#include "patch_paths.h"

/* Local Bankdata loading and transfer-slot preservation. */
/* 本地 Bankdata 载入与传送槽保留。 */
static int validHeader(const u8 *data)
{
    data+=sizeof(u64)+BANKDATA_NAME_BUFFER_COUNT*BANKDATA_NAME_CODE_UNIT_COUNT*sizeof(u16);
    return (u16)(data[0]|((u16)data[1]<<8))==BANKDATA_FORMAT_VERSION &&
        (u16)(data[2]|((u16)data[3]<<8))==BANKDATA_BOX_COUNT;
}

static void destroyTransferObjects(TransferObjectView *objects[BANKDATA_TRANSFER_SLOT_COUNT])
{
    u32 i;
    for (i=0;i<BANKDATA_TRANSFER_SLOT_COUNT;i++) {
        TransferObjectView *object=objects[i];
        if (object) {
            object->vtable->destroy(object);
            objects[i]=0;
        }
    }
}

static int loadLocalBank(MoverStateView *state)
{
    MoverFlowView *flow=state->bankdataFlow;
    MoverBankRuntimeObjectView *object=flow?flow->bankObject:0;
    TransferObjectView *transferObjects[BANKDATA_TRANSFER_SLOT_COUNT];
    u32 index,count=0;
    s32 result;
    void *box;
    u32 heap;

    for (index=0;index<BANKDATA_TRANSFER_SLOT_COUNT;index++) transferObjects[index]=0;
    if (!object || object->vtable!=BankFile_Vtable)
        return MOVER_LOCAL_BANK_LOAD_ERROR;
    box=object->transferBox;
    if (!box) return MOVER_LOCAL_BANK_LOAD_ERROR;
    heap=state->heapId;

    /* Preserve candidates through the same semantic slot objects used by the
       stock download callback; this also keeps the patch stack small. */
    /* 使用原版下载回调相同的语义槽对象保留候选，同时降低补丁栈占用。 */
    for (index=0;index<BANKDATA_TRANSFER_SLOT_COUNT;index++) {
        if (TransferSlot_IsOccupied(box,index)) {
            void *memory=Heap_AllocateObject(TRANSFER_OBJECT_ALLOCATION_SIZE,heap);
            TransferObjectView *candidate=memory?TransferObject_Create(memory,heap,1u):0;
            if (!candidate) {
                destroyTransferObjects(transferObjects);
                return MOVER_LOCAL_BANK_LOAD_ERROR;
            }
            TransferSlot_Load(box,candidate,index);
            transferObjects[count++]=candidate;
        }
    }

    result=readCompleteFile(bankPath,sizeof(bankPath),BANK_FILE_SIZE,0,
        &object->record,BANK_FILE_SIZE);
    if (result || !validHeader((const u8 *)&object->record)) {
        destroyTransferObjects(transferObjects);
        return MOVER_LOCAL_BANK_LOAD_ERROR;
    }
    if (TransferSlot_CountOccupied(box)!=0) {
        destroyTransferObjects(transferObjects);
        return MOVER_LOCAL_BANK_TRANSFER_BOX_OCCUPIED;
    }
    for (index=0;index<BANKDATA_TRANSFER_SLOT_COUNT;index++) TransferSlot_Clear(box,index);
    for (index=0;index<count;index++) TransferSlot_Write(box,transferObjects[index],index);
    destroyTransferObjects(transferObjects);
    return MOVER_LOCAL_BANK_LOAD_COMPLETE;
}

static int writeTemporary(const void *data)
{
    return writeCompleteFile(tempPath,sizeof(tempPath),data,BANK_FILE_SIZE,BANK_FILE_SIZE);
}

static int commitTemporary(void)
{
    u64 archive=0;
    s32 result,restoreResult;
    int hasBackup=0;
    if (openArchive(&archive)) return 0;

    /* Only a missing old backup is harmless. Preserve the current bank file
       unless it was successfully moved aside before installing the temp file. */
    /* 只有旧备份不存在属于无害情况。必须先成功把当前银行文件移作备份，
       才能安装临时文件。 */
    result=pathCommand(FSUSER_CMD_DELETE_FILE,archive,backupPath,sizeof(backupPath));
    if (result && !resultIsNotFound(result)) { closeArchive(archive); return 0; }
    result=renamePath(archive,bankPath,sizeof(bankPath),backupPath,sizeof(backupPath));
    if (!result) hasBackup=1;
    else if (!resultIsNotFound(result)) { closeArchive(archive); return 0; }

    result=renamePath(archive,tempPath,sizeof(tempPath),bankPath,sizeof(bankPath));
    if (result && hasBackup) {
        restoreResult=renamePath(archive,backupPath,sizeof(backupPath),
            bankPath,sizeof(bankPath));
        (void)restoreResult;
    }
    closeArchive(archive);
    return !result;
}

__attribute__((used,noinline,section(".text.offline.05_eligibility")))
int OfflinePatch_EligibilityUpdate(MoverStateView *state)
{
    int loaded=loadLocalBank(state);
    if (loaded==MOVER_LOCAL_BANK_LOAD_ERROR) {
        state->phase=MOVER_ELIGIBILITY_PHASE_ERROR;
    } else if (loaded==MOVER_LOCAL_BANK_TRANSFER_BOX_OCCUPIED) {
        /* Transfer-Box occupancy is an internal native message path, not outer
           result 0x0F. Resume at substate 7 so message 5 is displayed and the
           stock acknowledgement path returns result 0x10. */
        /* 传送盒占用属于原版内部消息路径，并非外层结果 0x0F。转入 substate 7，
           由原版显示消息 5，并在确认后通过结果 0x10 正常返回。 */
        state->substate=MOVER_ELIGIBILITY_SUBSTATE_TRANSFER_BOX_OCCUPIED;
        return MOVER_STATE_UPDATE_WAITING;
    } else {
        state->phase=MOVER_ELIGIBILITY_PHASE_COMPLETE;
    }
    return MOVER_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline.08_stage")))
int OfflinePatch_Stage(void *remote,const void *data,u32 size,void *transaction)
{
    (void)remote;
    (void)transaction;
    return size==BANK_FILE_SIZE && writeTemporary(data);
}

__attribute__((used,noinline,section(".text.offline.09_commit")))
int OfflinePatch_Commit(void *remote,void *transaction,u32 zero)
{
    (void)remote;
    (void)transaction;
    (void)zero;
    return commitTemporary();
}

__attribute__((used,noinline,section(".text.offline.10_rollback")))
int OfflinePatch_Rollback(void *remote,void *transaction,u32 zero)
{
    u64 archive=0;
    s32 result;
    (void)remote;
    (void)transaction;
    (void)zero;
    if (openArchive(&archive)) return 0;
    result=pathCommand(FSUSER_CMD_DELETE_FILE,archive,tempPath,sizeof(tempPath));
    closeArchive(archive);
    /* Repeating rollback after the temp file is gone is idempotent. */
    /* 临时文件已不存在时，重复回滚仍视为成功。 */
    return !result || resultIsNotFound(result);
}
