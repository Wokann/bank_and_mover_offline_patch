#include "bankdata_redirect.h"
#include "fs_helpers.h"
#include "patch_paths.h"

/* Bankdata redirection and local-file transaction implementation. */
/* Bankdata 重定向与本地文件事务实现。 */

static int validHeader(u16 formatVersion,u16 boxCount)
{
    return formatVersion==BANKDATA_FORMAT_VERSION && boxCount==BANKDATA_BOX_COUNT;
}

static int writeTemporary(const void *data)
{
    return writeCompleteFile(tempPath,sizeof(tempPath),data,BANK_FILE_SIZE,BANK_FILE_SIZE);
}

static int commitTemporary(void)
{
    u64 a=0; s32 r,restoreResult; int hasBackup=0;
    if (openArchive(&a)) return 0;

    /* Only a missing old backup is harmless. Preserve the current bank file
       unless it was successfully moved aside before installing the temp file. */
    /* 只有旧备份不存在属于无害情况。必须先成功把当前银行文件移作备份，
       才能安装临时文件。 */
    r=pathCommand(FSUSER_CMD_DELETE_FILE,a,backupPath,sizeof(backupPath));
    if (r && !resultIsNotFound(r)) { closeArchive(a); return 0; }
    r=renamePath(a,bankPath,sizeof(bankPath),backupPath,sizeof(backupPath));
    if (!r) hasBackup=1;
    else if (!resultIsNotFound(r)) { closeArchive(a); return 0; }

    r=renamePath(a,tempPath,sizeof(tempPath),bankPath,sizeof(bankPath));
    if (r && hasBackup) {
        restoreResult=renamePath(a,backupPath,sizeof(backupPath),bankPath,sizeof(bankPath));
        (void)restoreResult;
    }
    closeArchive(a); return !r;
}

static int writeInitialBank(const void *data)
{
    int complete;

    /* First use writes only the final bankdata.bin. If a size, write, short-write,
       flush, or close check fails, remove the incomplete new file. */
    /* 首次使用只写最终的 bankdata.bin。若长度设置、写入、短写、刷新或关闭检查
       失败，则删除不完整的新文件。 */
    complete=writeCompleteFile(bankPath,sizeof(bankPath),data,BANK_FILE_SIZE,BANK_FILE_SIZE);
    if (!complete) (void)deleteFile(bankPath,sizeof(bankPath));
    return complete;
}

static int inspectBankFile(const char *path,u32 pathSize)
{
    u16 header[2];
    s32 r=readCompleteFile(path,pathSize,BANK_FILE_SIZE,
        __builtin_offsetof(BankdataRecord,header.formatVersion),header,sizeof(header));
    if (r) return resultIsNotFound(r)?BANKDATA_FILE_MISSING:BANKDATA_FILE_INVALID;
    return validHeader(header[0],header[1])?BANKDATA_FILE_VALID:BANKDATA_FILE_INVALID;
}

static int preserveBrokenFile(const char *source,u32 sourceSize,
    const char *destination,u32 destinationSize)
{
    u8 buffer[BANKDATA_COPY_BUFFER_SIZE];
    u32 sourceHandle=0,destinationHandle=0,readCount=0,writeCount=0;
    u32 size32=0,offset=0,chunk;
    u64 size=0;
    s32 r,sourceClose=0,destinationClose=0;

    r=openFile(source,sourceSize,OPEN_READ,&sourceHandle);
    if (!r) r=FSFILE_GetSize(&sourceHandle,&size);
    if (!r && (size>>32)) r=-1;
    if (!r) size32=(u32)size;
    if (!r) r=openFile(destination,destinationSize,
        OPEN_READ|OPEN_WRITE|OPEN_CREATE,&destinationHandle);
    if (!r) r=setSize(destinationHandle,size);
    while (!r && offset<size32) {
        u32 remaining=size32-offset;
        chunk=(remaining>sizeof(buffer))?sizeof(buffer):(u32)remaining;
        readCount=0;
        writeCount=0;
        r=FSFILE_Read(&sourceHandle,&readCount,offset,buffer,chunk);
        if (!r && readCount!=chunk) r=-1;
        if (!r) r=FSFILE_Write(&destinationHandle,&writeCount,offset,buffer,chunk,WRITE_FLUSH);
        if (!r && writeCount!=chunk) r=-1;
        offset+=chunk;
    }
    if (destinationHandle) destinationClose=FSFILE_Close(&destinationHandle);
    if (sourceHandle) sourceClose=FSFILE_Close(&sourceHandle);
    if (!r && !sourceClose && !destinationClose && offset==size32) return 1;

    /* Never leave a partial .break file that could be mistaken for a complete copy. */
    /* 不保留可能被误认为完整副本的残缺 .break 文件。 */
    (void)deleteFile(destination,destinationSize);
    return 0;
}

static int restoreBankFromBackup(BankStateView *state)
{
    BankFlowView *flow=state->bankdataFlow;
    BankRuntimeObjectView *object=flow?flow->bankObject:0;
    s32 r;

    if (!object || object->vtable!=BankFile_Vtable)
        return 0;
    r=readCompleteFile(backupPath,sizeof(backupPath),BANK_FILE_SIZE,0,
        &object->record,BANK_FILE_SIZE);
    if (r || !validHeader(object->record.header.formatVersion,
        object->record.header.boxCount)) return 0;

    /* Preserve bankdata.bak and create a checked byte-for-byte bankdata.bin copy. */
    /* 保留 bankdata.bak，并创建经过完整检查、逐字节一致的 bankdata.bin 副本。 */
    return writeInitialBank(&object->record);
}

static int finishInitialRecordState(BankStateView *state,
    BankInitialRecordSharedView *shared,u8 mode)
{
    shared->mode=mode;
    shared->counter=0;
    shared->flag0=0;
    shared->flag1=0;
    shared->flag2=0;
    state->phase=BANK_STATE_PHASE_COMPLETE;
    return BANK_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_InitialRemoteRecordUpdate(BankStateView *state)
{
    BankInitialRecordSharedView *shared=(BankInitialRecordSharedView *)state->sharedData;
    int bankStatus,backupStatus;

    if (!shared) {
        state->phase=BANK_STATE_PHASE_ERROR;
        return BANK_STATE_UPDATE_FINISHED;
    }
    bankStatus=inspectBankFile(bankPath,sizeof(bankPath));
    if (bankStatus==BANKDATA_FILE_VALID) {
        /* ASCII '4' is the stock existing-record mode; '5' is first use. */
        /* ASCII“4”是原版既有记录模式；“5”表示首次使用。 */
        return finishInitialRecordState(state,shared,BANK_INITIAL_RECORD_MODE_EXISTING);
    }

    /* A missing or invalid primary always checks the backup first. A valid
       backup must be restored; a missing or invalid backup selects stock
       first-use creation instead. */
    /* 主文件缺失或无效时一律优先检查备份。有效备份必须恢复；备份缺失或
       无效时则进入原版首次创建。 */
    backupStatus=inspectBankFile(backupPath,sizeof(backupPath));
    if (backupStatus==BANKDATA_FILE_VALID) {
        if (restoreBankFromBackup(state)) {
            return finishInitialRecordState(state,shared,BANK_INITIAL_RECORD_MODE_EXISTING);
        } else {
            state->phase=BANK_STATE_PHASE_ERROR;
        }
    } else {
        int attempt;
        /* Preserve only files that exist but cannot be recognized. After the
           initial failure, retry each copy up to three times, but do not block
           stock first use if all four attempts fail. The original invalid
           files are never removed. */
        /* 只保全实际存在但无法识别的文件。初次失败后，每个副本最多重试三次；即使四次尝试全部失败，
           也不阻止原版首次创建。原始无效文件始终不会被删除。 */
        if (bankStatus==BANKDATA_FILE_INVALID) {
            for (attempt=0;attempt<BANKDATA_BROKEN_COPY_ATTEMPTS;attempt++) {
                if (preserveBrokenFile(bankPath,sizeof(bankPath),
                    brokenBankPath,sizeof(brokenBankPath))) break;
            }
        }
        if (backupStatus==BANKDATA_FILE_INVALID) {
            for (attempt=0;attempt<BANKDATA_BROKEN_COPY_ATTEMPTS;attempt++) {
                if (preserveBrokenFile(backupPath,sizeof(backupPath),
                    brokenBackupPath,sizeof(brokenBackupPath))) break;
            }
        }
        return finishInitialRecordState(state,shared,BANK_INITIAL_RECORD_MODE_FIRST_USE);
    }
    return BANK_STATE_UPDATE_FINISHED;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_LoadBankData(BankStateView *state)
{
    BankFlowView *flow=state->bankdataFlow;
    BankRuntimeObjectView *object=flow?flow->bankObject:0;
    s32 r;

    if (!object || object->vtable!=BankFile_Vtable)
        return 0;
    r=readCompleteFile(bankPath,sizeof(bankPath),BANK_FILE_SIZE,0,
        &object->record,BANK_FILE_SIZE);

    if (!r && validHeader(object->record.header.formatVersion,
        object->record.header.boxCount)) {
        /* Rebuild the flow metadata value set by the stock download callback. */
        /* 重建原版完整数据下载回调写入流程对象的元数据值。 */
        flow->metadataValue=BankFile_DeriveFlowMetadata(object->metadata);
        return 1;
    }
    return 0;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_CreateInitial(void *remote,const void *data,u32 size)
{
    (void)remote;
    return size==BANK_FILE_SIZE && writeInitialBank(data);
}

__attribute__((used,noinline,section(".text.offline")))
int BankdataRedirect_CaptureDownloaded(BankRuntimeObjectView *object)
{
    /* Native loading has already expanded any legacy body to the current
       record. Empty banks are valid; no Pokemon-count test belongs here. */
    /* 原版装载已将旧格式扩展为当前记录。空银行同样有效，此处不按宝可梦数量过滤。 */
    if (!object || object->vtable!=BankFile_Vtable ||
        !validHeader(object->record.header.formatVersion,object->record.header.boxCount))
        return 0;
    return writeTemporary(&object->record) && commitTemporary();
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_Stage(void *remote,const void *data,u32 size,void *transaction)
{
    (void)remote; (void)transaction; return size==BANK_FILE_SIZE && writeTemporary(data);
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_Commit(void *remote,void *transaction,u32 zero)
{
    (void)remote; (void)transaction; (void)zero; return commitTemporary();
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_Rollback(void *remote,void *transaction,u32 zero)
{
    s32 r; (void)remote; (void)transaction; (void)zero;
    r=deleteFile(tempPath,sizeof(tempPath));
    /* Repeating rollback after the temp file is gone is idempotent. */
    /* 临时文件已不存在时，重复回滚仍视为成功。 */
    return !r || resultIsNotFound(r);
}
