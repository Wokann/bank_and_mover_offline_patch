#include "fakeServer/fake_servers.h"

static int settled(const FsTransactionRecord *r)
{
    return r->phase==FS_TRANSACTION_COMPLETED || r->phase==FS_TRANSACTION_ABORTED ||
        r->phase==FS_TRANSACTION_DETACHED_ROLLED_BACK;
}

static int equalStamp(const FsTransactionFingerprint *a,const FsTransactionFingerprint *b)
{
    if (a->present!=b->present || a->size!=b->size) return 0;
    for (uint32_t i=0;i<sizeof(a->hash);i++) if (a->hash[i]!=b->hash[i]) return 0;
    return 1;
}

static void copyStamp(FsTransactionFingerprint *to,const FsTransactionFingerprint *from)
{
    for (uint32_t i=0;i<sizeof(*to);i++) ((uint8_t *)to)[i]=((const uint8_t *)from)[i];
}

static int sameParameters(const FsTransactionParameters *a,const FsTransactionParameters *b)
{
    return a->dataId==b->dataId && a->token==b->token && a->size==b->size &&
        a->currentVersion==b->currentVersion && a->updateVersion==b->updateVersion;
}

static int identity(const FsSource *source,FsTransactionRecord *record)
{
    if (!source || source->kind!=FS_TRANSACTION_NDS_FILE || !source->path ||
        source->pathSize<4 || (source->pathSize&1) ||
        source->pathSize>sizeof(record->sourcePath) ||
        source->path[0]!='/' || source->path[source->pathSize/2-1]) return 0;
    for (uint32_t i=0;i<sizeof(*record);i++) ((uint8_t *)record)[i]=0;
    for (uint32_t i=0;i<source->pathSize/2-1;i++) if (!source->path[i]) return 0;
    record->sourceKind=source->kind; record->sourceGameCode=source->gameCode;
    record->sourcePathSize=source->pathSize;
    for (uint32_t i=0;i<source->pathSize/2;i++) record->sourcePath[i]=source->path[i];
    return 1;
}

static int sameSource(const FsSource *source,const FsTransactionRecord *record)
{
    if (!source || source->kind!=record->sourceKind) return 0;
    /* Native matching stays entirely in the original client. */
    /* 原生来源匹配完全保留在原版客户端内。 */
    if (source->kind==FS_TRANSACTION_NATIVE_SOURCE) return 1;
    if (source->kind!=FS_TRANSACTION_NDS_FILE || !source->path ||
        source->gameCode!=record->sourceGameCode || source->pathSize!=record->sourcePathSize)
        return 0;
    for (uint32_t i=0;i<source->pathSize/2;i++)
        if (source->path[i]!=record->sourcePath[i]) return 0;
    return 1;
}

int FsServer_DescribeTransaction(FsServer *server,FsTransactionParameters *out)
{
    FsTransactionRecord record;
    if (!out || !FsServer_Connected(server)) return FS_INVALID;
    if (!FsTransaction_EnsureIdentity(server->transactions,server->owner) ||
        FsTransaction_ReadRecord(server->transactions,&record)!=FS_TRANSACTION_PRESENT)
        return FS_LOCKED;
    for (uint32_t i=0;i<sizeof(*out);i++)
        ((uint8_t *)out)[i]=((const uint8_t *)&record.serverTransaction)[i];
    if (settled(&record) || record.phase==FS_TRANSACTION_PUBLISHED ||
        record.phase==FS_TRANSACTION_CLEANUP_PENDING ||
        record.phase==FS_TRANSACTION_CLEANUP_READY) {
        out->token=0;
        if (!settled(&record)) out->currentVersion=out->updateVersion;
        out->updateVersion=out->currentVersion;
    }
    return FS_OK;
}

int FsServer_RequestTransaction(FsServer *server,const FsTransactionParameters *request,
    int commit,int force)
{
    if (!FsServer_Connected(server) || !request) return FS_INVALID;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    if (status==FS_TRANSACTION_IO_ERROR) return FS_IO_ERROR;
    if (status==FS_TRANSACTION_MISSING)
        return FsServer_Guard(server)==FS_OK?FS_REMOTE_NOT_FOUND:FS_LOCKED;
    const FsTransactionParameters *saved=&record.serverTransaction;
    if (force) {
        if (commit || !sameParameters(request,saved)) return FS_TRANSACTION_MISMATCH;
        if (server->owner==FS_TRANSACTION_OWNER_BANK) {
            if (!server->rollbackAuthorized || server->rollbackTransaction!=record.transaction ||
                !sameParameters(request,&server->rollbackDescriptor)) return FS_TRANSACTION_MISMATCH;
        } else {
            /* Mover's native metadata-recovery flow confirms twice, without a
               keyboard. Its explicit force request must still match our live reservation. */
            /* Mover 原元数据恢复经过两次确认，不使用键盘；其明确强制请求仍须匹配当前预留。 */
            if (server->owner!=FS_TRANSACTION_OWNER_MOVER ||
                record.operation!=FS_TRANSACTION_SOURCE_SAVE ||
                (record.phase!=FS_TRANSACTION_SOURCE_PENDING && record.phase!=FS_TRANSACTION_STAGED) ||
                !request->token)
                return FS_TRANSACTION_MISMATCH;
        }
    }
    if (settled(&record) && FsServer_Guard(server)!=FS_OK) return FS_LOCKED;
    if (!commit && !force && !request->token &&
        request->currentVersion==request->updateVersion) {
        if (!settled(&record) || request->dataId!=saved->dataId ||
            request->currentVersion!=saved->currentVersion || request->size!=saved->size)
            return FS_TRANSACTION_MISMATCH;
        /* A no-save exit without a reservation only acknowledges unchanged
           server data. It never cancels an unrelated pending transaction. */
        /* 无预留的不保存退出只确认未变更的数据，不能取消其他未决事务。 */
        return FsServer_Guard(server);
    }
    /* Only an intact service record proves a missing/expired reservation.
       Original callbacks decide whether that reply permits token cleanup or
       a mismatch prompt. Storage failure must never become that permission. */
    /* 仅完整服务记录能证明预留不存在／已失效；是否清令牌或提示不匹配由原回调决定，
       存储错误绝不能转为允许清理的业务响应。 */
    if (!force) {
        if (request->dataId!=saved->dataId) return FS_REMOTE_NOT_FOUND;
        if (!request->token || request->token!=saved->token || request->size!=saved->size)
            return FS_TRANSACTION_MISMATCH;
        if (settled(&record)) {
            uint32_t completed=record.phase==FS_TRANSACTION_COMPLETED;
            if (request->currentVersion==0xffffffffu ||
                request->updateVersion!=request->currentVersion+1 ||
                (completed?request->updateVersion:request->currentVersion)!=saved->currentVersion ||
                (commit!=0)!=(completed!=0)) return FS_TRANSACTION_MISMATCH;
        } else if (request->currentVersion!=saved->currentVersion ||
            request->updateVersion!=saved->updateVersion) return FS_TRANSACTION_MISMATCH;
    }
    if (!FsTransaction_Request(server->transactions,server->owner,request,commit,force))
        return FS_LOCKED;
    if (force) {
        server->rollbackAuthorized=0;
        server->rollbackCodes.candidateCount=0;
    }
    return FS_OK;
}

int FsServer_QueryMetadata(FsServer *server,FsTransactionParameters *descriptor,
    FsSourceMetadata *metadata)
{
    FsTransactionRecord record;
    if (!metadata || FsServer_DescribeTransaction(server,descriptor)!=FS_OK ||
        FsTransaction_ReadRecord(server->transactions,&record)!=FS_TRANSACTION_PRESENT)
        return FS_LOCKED;
    for (uint32_t i=0;i<sizeof(*metadata);i++)
        ((uint8_t *)metadata)[i]=((const uint8_t *)&record.metadata)[i];
    return FS_OK;
}

int FsServer_QueryMetadataFor(FsServer *server,uint64_t dataId,FsSourceMetadata *metadata)
{
    FsTransactionParameters descriptor;
    if (!dataId || !metadata) return FS_INVALID;
    int status=FsServer_QueryMetadata(server,&descriptor,metadata);
    if (status!=FS_OK) return status;
    return descriptor.dataId==dataId?FS_OK:FS_REMOTE_NOT_FOUND;
}

int FsServer_UpdateMetadata(FsServer *server,uint64_t dataId,const FsSourceMetadata *metadata)
{
    FsTransactionParameters descriptor;
    if (!dataId || !metadata || metadata->formatVersion!=2) return FS_INVALID;
    int status=FsServer_DescribeTransaction(server,&descriptor);
    if (status!=FS_OK || descriptor.dataId!=dataId)
        return status==FS_OK?FS_REMOTE_NOT_FOUND:status;
    /* A successful independent update is already durable. Active reservations
       may only acknowledge the exact metadata bound to their descriptor. */
    /* 独立更新的成功应答必须已落盘；在途预留仅可确认其描述符绑定的原元数据。 */
    if (FsServer_Guard(server)==FS_OK)
        return FsTransaction_UpdateMetadata(server->transactions,dataId,metadata)?FS_OK:FS_IO_ERROR;
    FsSourceMetadata expected;
    status=FsServer_QueryMetadataFor(server,dataId,&expected);
    if (status!=FS_OK) return status;
    for (uint32_t i=0;i<sizeof(expected);i++)
        if (((const uint8_t *)&expected)[i]!=((const uint8_t *)metadata)[i]) return FS_LOCKED;
    return FS_OK;
}

int FsServer_ObserveSource(FsServer *server,const FsSource *source,
    FsTransactionFingerprint *out)
{
    FsTransactionRecord record;
    if (!out || !FsServer_Connected(server) ||
        server->owner!=FS_TRANSACTION_OWNER_MOVER || !identity(source,&record))
        return FS_INVALID;
    /* This endpoint starts a native source load, even for the same path after
       returning to selection. Guards inside that load reuse its observation. */
    /* 本接口开启一次原生来源载入；返回选择后再选同一路径也重新观察，该次载入
       内部的保护检查则复用此结果。 */
    for (uint32_t file=FS_TRANSACTION_SOURCE;file<=FS_TRANSACTION_SOURCE_BACKUP;file++)
        FsTransaction_ViewForget(server->transactions,file);
    int status=FsTransaction_Observe(server->transactions,FS_TRANSACTION_SOURCE,&record,out);
    if (status==FS_TRANSACTION_MISSING) return FS_MISSING;
    return status==FS_TRANSACTION_PRESENT?FS_OK:FS_IO_ERROR;
}

int FsServer_BeginSourceSave(FsServer *server,const FsSource *source)
{
    if (!FsServer_Connected(server) || server->owner!=FS_TRANSACTION_OWNER_MOVER ||
        !source) return FS_INVALID;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    if (status==FS_TRANSACTION_IO_ERROR) return FS_LOCKED;
    /* The recovery wrapper already matched the native descriptor and recorded
       cleanup start. Its save backend may belong to the other application. */
    /* 恢复包装器已匹配原描述符并记录清理开始；保存后端可来自另一客户端。 */
    if (status==FS_TRANSACTION_PRESENT && source->kind==FS_TRANSACTION_NATIVE_SOURCE &&
        record.sourceKind==FS_TRANSACTION_NATIVE_SOURCE &&
        record.operation==FS_TRANSACTION_SOURCE_SAVE && record.phase==FS_TRANSACTION_CLEANUP_PENDING)
        return FS_OK;
    if (status==FS_TRANSACTION_PRESENT && record.owner==server->owner &&
        record.operation==FS_TRANSACTION_SOURCE_SAVE &&
        (record.phase==FS_TRANSACTION_SOURCE_PENDING || record.phase==FS_TRANSACTION_PUBLISHED ||
         record.phase==FS_TRANSACTION_CLEANUP_PENDING)) {
        if (!sameSource(source,&record)) return FS_LOCKED;
        if (record.phase!=FS_TRANSACTION_SOURCE_PENDING)
            return FsServer_Transaction(server,FS_TRANSACTION_BEGIN_CLEANUP);
        return FS_OK;
    }
    if (source->kind==FS_TRANSACTION_NDS_FILE) {
        if (!source->expected) return FS_LOCKED;
        /* Stage performs the full guard immediately before preparation. */
        /* Stage 在暂存前执行完整保护检查，不在这里再扫描同一银行正文。 */
        return FsServer_Stage(server,0,0,source);
    }
    int result=FsServer_Guard(server);
    if (result!=FS_OK) return result;
    return source->kind==FS_TRANSACTION_NATIVE_SOURCE?FS_OK:FS_INVALID;
}

int FsServer_SourceSaveResult(FsServer *server,const FsSource *source,uint32_t result,
    const FsTransactionFingerprint *validated,FsTransactionFingerprint *out)
{
    if (!FsServer_Connected(server) || !source || !result || !out) return FS_INVALID;
    for (uint32_t i=0;i<sizeof(*out);i++) ((uint8_t *)out)[i]=0;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    if (status==FS_TRANSACTION_IO_ERROR) return FS_LOCKED;
    if (status!=FS_TRANSACTION_PRESENT || record.owner!=server->owner ||
        (record.operation!=FS_TRANSACTION_SOURCE_SAVE &&
         record.operation!=FS_TRANSACTION_SOURCE_ONLY) ||
        (record.phase!=FS_TRANSACTION_SOURCE_PENDING &&
         record.phase!=FS_TRANSACTION_CLEANUP_PENDING))
        return source->kind==FS_TRANSACTION_NATIVE_SOURCE?FS_OK:FS_LOCKED;
    if (!sameSource(source,&record)) return FS_LOCKED;
    /* A failed native save is not proof of an unchanged source. Leave the
       decision to the original state machine and its later recovery. */
    /* 原生保存报错不证明来源未改动；决定留给原版状态机及下次恢复。 */
    if (result!=1) return FS_IO_ERROR;
    if (source->kind==FS_TRANSACTION_NDS_FILE &&
        (!validated || !validated->present ||
         !FsTransaction_Matches(server->transactions,FS_TRANSACTION_SOURCE_TEMP,
             &record,validated))) return FS_LOCKED;
    int cleanup=record.phase!=FS_TRANSACTION_SOURCE_PENDING;
    if (FsServer_Transaction(server,cleanup?FS_TRANSACTION_CLEANUP_SAVED:
        FS_TRANSACTION_SOURCE_SAVED)!=FS_OK ||
        FsTransaction_ReadRecord(server->transactions,&record)!=FS_TRANSACTION_PRESENT)
        return FS_LOCKED;
    const FsTransactionFingerprint *after=record.phase==FS_TRANSACTION_CLEANUP_READY?
        &record.sourceClean:&record.sourceAfter;
    if (validated && !equalStamp(validated,after)) return FS_LOCKED;
    if (record.operation==FS_TRANSACTION_SOURCE_ONLY) {
        /* SourceSaved already persisted the decision and published this
           source-only write. Finish verifies publication and refreshes its
           backup; an extra Commit only repeats the unchanged Bank check. */
        /* SourceSaved 已持久化决定并发布单独来源写入；Finish 核对发布并更新
           备份。额外 Commit 只重复扫描未改动的银行，不是新的保存确认。 */
        if (record.phase!=FS_TRANSACTION_PUBLISHED ||
            FsServer_Transaction(server,FS_TRANSACTION_FINISH)!=FS_OK) return FS_LOCKED;
        copyStamp(out,after);
    }
    return FS_OK;
}

int FsServer_SettledSource(FsServer *server,const FsSource *source,
    FsTransactionFingerprint *out)
{
    if (!out || !FsServer_Connected(server)) return FS_INVALID;
    for (uint32_t i=0;i<sizeof(*out);i++) ((uint8_t *)out)[i]=0;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    if (status==FS_TRANSACTION_IO_ERROR) return FS_IO_ERROR;
    if (status!=FS_TRANSACTION_PRESENT ||
        record.owner!=server->owner || record.phase!=FS_TRANSACTION_COMPLETED ||
        !sameSource(source,&record)) return FS_MISSING;
    const FsTransactionFingerprint *completed=
        record.sourceClean.present?&record.sourceClean:&record.sourceAfter;
    /* The same path may now contain a later normal save or an imported image.
       An ended receipt may confirm the observed image, never replace its
       fresh identity. Pending records still follow native recovery. */
    /* 相同路径可能已被正常游戏保存或导入更新；已结束记录只能确认本次观察，
       不能以旧摘要替换新身份。未决记录仍走原生恢复，不在这里重建或解锁。 */
    if (source->kind==FS_TRANSACTION_NDS_FILE &&
        (!source->expected || source->expected->present!=1 ||
         source->expected->size!=0x80000u || !equalStamp(source->expected,completed)))
        return FS_MISSING;
    copyStamp(out,completed);
    return FS_OK;
}

int FsServer_NativeSaveEvent(FsServer *server,const FsTransactionParameters *request,
    enum FsNativeSaveEvent event)
{
    if (!FsServer_Connected(server) || !request || !request->token ||
        (unsigned)event>FS_NATIVE_CLEANUP_SAVED) return FS_INVALID;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    int cleanup=event==FS_NATIVE_CLEANUP_STARTING || event==FS_NATIVE_CLEANUP_SAVED;
    if (status==FS_TRANSACTION_MISSING)
        return cleanup?FsServer_Guard(server):FS_LOCKED;
    if (status!=FS_TRANSACTION_PRESENT) return FS_LOCKED;
    const FsTransactionParameters *saved=&record.serverTransaction;
    if (settled(&record)) {
        /* The native recovery callback may accept an expired/missing old
           reservation and save only its cleared source token. A cleanup
           acknowledgement does not commit/roll back this settled Bank, so it
           must not re-require the obsolete descriptor. Keep the file guard,
           and retain exact matching for source-write acknowledgements. */
        /* 原恢复回调可接受旧预留失效／不存在，再仅清来源令牌并保存。清理确认
           不提交或回滚已结束的银行，不能重新要求旧描述符匹配当前银行；仍检查
           文件状态，首次／恢复保存的确认继续要求描述符精确匹配。 */
        if (cleanup) return FsServer_Guard(server);
        int completed=record.phase==FS_TRANSACTION_COMPLETED;
        if (request->dataId!=saved->dataId || request->token!=saved->token || request->size!=saved->size ||
            request->currentVersion==0xffffffffu || request->updateVersion!=request->currentVersion+1 ||
            (completed?request->updateVersion:request->currentVersion)!=saved->currentVersion)
            return FS_TRANSACTION_MISMATCH;
        return FsServer_Guard(server);
    }
    if (!sameParameters(request,saved)) return FS_TRANSACTION_MISMATCH;
    /* SD file writes have their own validated SavePoll/publication path. */
    /* SD 文件保存继续使用其原有的校验、SavePoll 和发布路径。 */
    if (record.sourceKind==FS_TRANSACTION_NDS_FILE) return FS_OK;
    if (record.sourceKind!=FS_TRANSACTION_NATIVE_SOURCE ||
        record.operation!=FS_TRANSACTION_SOURCE_SAVE) return FS_LOCKED;
    const FsTransactionIo *io=server->transactions;
    int ok=0;
    switch (event) {
    case FS_NATIVE_SOURCE_SAVED:
        ok=FsTransaction_SourceSaved(io,record.owner); break;
    case FS_NATIVE_SOURCE_RESTORED:
        ok=FsTransaction_SourceRestored(io,record.owner); break;
    case FS_NATIVE_CLEANUP_STARTING:
        ok=record.phase==FS_TRANSACTION_CLEANUP_READY ||
            FsTransaction_BeginCleanup(io,record.owner); break;
    case FS_NATIVE_CLEANUP_SAVED:
        ok=(record.phase==FS_TRANSACTION_CLEANUP_READY ||
            FsTransaction_CleanupSaved(io,record.owner)) && FsTransaction_Finish(io,record.owner); break;
    }
    if (!ok) return FS_LOCKED;
    return FS_OK;
}
