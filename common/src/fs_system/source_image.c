#include "fs_system/source_image.h"
#include <string.h>

_Static_assert(sizeof(FsLocalSource)<=0x1000,"local source control page");

static int same(const void *a,const void *b,uint32_t size)
{
    for (uint32_t i=0;i<size;i++) if (((const uint8_t *)a)[i]!=((const uint8_t *)b)[i]) return 0;
    return 1;
}

static int ended(const FsTransactionRecord *r)
{
    return r->phase==FS_TRANSACTION_COMPLETED || r->phase==FS_TRANSACTION_ABORTED ||
        r->phase==FS_TRANSACTION_DETACHED_ROLLED_BACK;
}

static int sourceIdentity(const FsSource *source,FsTransactionRecord *record)
{
    if (!source || source->kind!=FS_TRANSACTION_NDS_FILE || !source->path ||
        source->pathSize<4 || (source->pathSize&1) || source->pathSize>sizeof(record->sourcePath) ||
        source->path[0]!='/' || source->path[source->pathSize/2-1]) return 0;
    for (uint32_t i=0;i<source->pathSize/2-1;i++) if (!source->path[i]) return 0;
    memset(record,0,sizeof(*record));
    record->sourceKind=source->kind;record->sourceGameCode=source->gameCode;
    record->sourcePathSize=source->pathSize;
    memcpy(record->sourcePath,source->path,source->pathSize);
    return FsTransaction_SourcePath(record,FS_TRANSACTION_SOURCE,0);
}

int FsLocalSource_Contains(const FsLocalSource *local,const uint16_t *path,
    uint32_t size,uint32_t code)
{
    return local && local->magic==FS_LOCAL_SOURCE_MAGIC && local->loaded && path &&
        size==local->identity.sourcePathSize && code==local->identity.sourceGameCode &&
        same(path,local->identity.sourcePath,size);
}

int FsLocalSource_Initialize(FsLocalSource *local,const FsTransactionIo *io,
    void *image,uint32_t size)
{
    if (!local || !io || !image || size!=FS_LOCAL_SOURCE_SIZE ||
        !io->openRead || !io->openWrite || !io->setSize || !io->read || !io->write ||
        !io->close || !io->remove || !io->rename || !io->freeBytes) return FS_INVALID;
    if (local->magic==FS_LOCAL_SOURCE_MAGIC)
        return local->image==image?FS_OK:FS_INVALID;
    memset(local,0,sizeof(*local));
    local->magic=FS_LOCAL_SOURCE_MAGIC;local->image=image;local->io=*io;
    local->io.buffer=image;local->io.bufferSize=size;local->io.bufferBusy=&local->busy;
    local->io.view=&local->view;local->io.deferSourceImage=1;
    return FS_OK;
}

static int fail(FsLocalSource *local,int result)
{
    local->loaded=0;local->working=0;local->error=1;
    local->validated=(FsTransactionFingerprint){0};
    FsTransaction_ViewClear(&local->io);
    if (local->coordinator) FsTransaction_ViewClear(local->coordinator);
    return result;
}

void FsLocalSource_Discard(FsLocalSource *local)
{
    if (local && local->magic==FS_LOCAL_SOURCE_MAGIC) (void)fail(local,FS_IO_ERROR);
}

int FsLocalSource_Load(FsLocalSource *local,const FsSource *source,FsTransactionFingerprint *out)
{
    if (!local || local->magic!=FS_LOCAL_SOURCE_MAGIC || !out) return FS_INVALID;
    local->loaded=0;local->working=0;local->error=0;local->written=0;
    local->coordinator=0;
    local->validated=(FsTransactionFingerprint){0};
    FsTransaction_ViewClear(&local->io);
    if (!sourceIdentity(source,&local->identity)) return FS_INVALID;
    FsTransactionRecord pending;
    int history=FsTransaction_ReadRecord(&local->io,&pending);
    if (history==FS_TRANSACTION_MISSING &&
        (!FsTransaction_ReconcileOrphanParticipants(&local->io) ||
         !FsTransaction_ReconcileOrphanSource(&local->io,&local->identity)))
        return fail(local,FS_LOCKED);
    if (history==FS_TRANSACTION_PRESENT && pending.operation==FS_TRANSACTION_SOURCE_ONLY && !ended(&pending)) {
        /* Before the native loader sees a source, finish only its existing
           local durable decision. Sibling paths isolate different games;
           the one shared coordinator still forbids replacing a pending save. */
        /* 原 loader 读取前仅续办本来源已有的本地决定；同名路径隔离不同游戏，
           共用协调日志仍不允许覆盖另一个未决保存。不接触银行或配置。 */
        if (pending.sourceKind!=FS_TRANSACTION_NDS_FILE || pending.sourceGameCode!=source->gameCode ||
            pending.sourcePathSize!=source->pathSize || !same(pending.sourcePath,source->path,source->pathSize) ||
            !FsTransaction_Check(&local->io)) return fail(local,FS_LOCKED);
    }
    int found=FsTransaction_ObserveChecked(&local->io,FS_TRANSACTION_SOURCE,
        &local->identity,&local->selected,0,0);
    if (found!=FS_TRANSACTION_PRESENT || local->selected.size!=FS_LOCAL_SOURCE_SIZE)
        return fail(local,found==FS_TRANSACTION_MISSING?FS_MISSING:FS_IO_ERROR);
    local->loaded=1;*out=local->selected;
    return FS_OK;
}

/* Only fingerprints cross the coordinator boundary. Never copy a Gen5 body
   into the Bank workspace, or construct/open a server for a local save. */
/* 协调边界只交接摘要，不把 Gen5 正文复制进银行缓冲，也不为本地保存构造服务器。 */
static void bankEvidence(FsLocalSource *local,const FsTransactionIo *coordinator)
{
    FsTransactionFingerprint bank;
    if (coordinator && FsTransaction_ViewGet(coordinator,FS_TRANSACTION_BANK,0,&bank))
        FsTransaction_ViewPut(&local->io,FS_TRANSACTION_BANK,0,&bank);
}

static void sourceEvidence(FsLocalSource *local,const FsTransactionIo *coordinator)
{
    if (!coordinator || coordinator==&local->io) return;
    for (uint32_t file=FS_TRANSACTION_SOURCE;file<=FS_TRANSACTION_SOURCE_BACKUP;file++) {
        FsTransactionFingerprint value;
        int known=FsTransaction_ViewGet(&local->io,file,&local->identity,&value);
        FsTransaction_ViewForget(coordinator,file);
        if (known) FsTransaction_ViewPut(coordinator,file,&local->identity,&value);
    }
}

static int sameReservation(const FsTransactionRecord *a,const FsTransactionRecord *b)
{
    return a->version==b->version && a->owner==b->owner && a->transaction==b->transaction && a->operation==b->operation &&
        a->sourceKind==b->sourceKind && a->sourceGameCode==b->sourceGameCode &&
        a->sourcePathSize==b->sourcePathSize && same(a->sourcePath,b->sourcePath,a->sourcePathSize) &&
        same(&a->serverTransaction,&b->serverTransaction,sizeof(a->serverTransaction));
}

int FsLocalSource_Prepare(FsLocalSource *local,const FsSource *source,
    const FsTransactionIo *coordinator)
{
    if (!local || local->magic!=FS_LOCAL_SOURCE_MAGIC || !source || source->mode>3 ||
        !source->expected || !source->expected->present ||
        !FsLocalSource_Contains(local,source->path,source->pathSize,source->gameCode) ||
        !same(source->expected,&local->selected,sizeof(local->selected))) return FS_LOCKED;
    /* A real local write boundary re-observes the primary once. UI reads and
       each segmented native write do not re-open or re-hash it. */
    /* 实际本地写入边界只重新观察主存档一次；界面读取及逐段写入不重开或重算。 */
    local->coordinator=coordinator;
    FsTransaction_ViewClear(&local->io);bankEvidence(local,coordinator);
    FsTransactionFingerprint before;
    if (FsTransaction_ObserveChecked(&local->io,FS_TRANSACTION_SOURCE,&local->identity,
        &before,0,0)!=FS_TRANSACTION_PRESENT || !same(&before,source->expected,sizeof(before)))
        return fail(local,FS_LOCKED);
    FsTransactionRecord record;
    int found=FsTransaction_ReadRecord(&local->io,&record);
    if (found==FS_TRANSACTION_IO_ERROR) return fail(local,FS_LOCKED);
    if (found==FS_TRANSACTION_PRESENT && !ended(&record) &&
        record.operation==FS_TRANSACTION_SOURCE_ONLY) {
        /* Resume/abort a local save only through its durable decision. */
        /* 单独本地保存仅按已有持久化决定续办或撤销，不重建未决记录。 */
        if (!FsTransaction_Check(&local->io) ||
            FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT)
            return fail(local,FS_LOCKED);
    }
    if (found==FS_TRANSACTION_PRESENT && !ended(&record)) {
        if (record.owner!=FS_TRANSACTION_OWNER_MOVER || record.mode!=source->mode ||
            record.operation!=FS_TRANSACTION_SOURCE_SAVE ||
            record.sourceKind!=FS_TRANSACTION_NDS_FILE ||
            record.sourceGameCode!=source->gameCode || record.sourcePathSize!=source->pathSize ||
            !same(record.sourcePath,source->path,source->pathSize)) return fail(local,FS_LOCKED);
        if (record.phase==FS_TRANSACTION_PUBLISHED) {
            if (!FsTransaction_BeginCleanup(&local->io,record.owner) ||
                FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT)
                return fail(local,FS_LOCKED);
        }
        const FsTransactionFingerprint *expected=record.phase==FS_TRANSACTION_CLEANUP_PENDING?
            &record.sourceAfter:&record.sourceBefore;
        if (!FsTransaction_SourceIsWorking(&record) || !same(&before,expected,sizeof(before)))
            return fail(local,FS_LOCKED);
    } else if (!FsTransaction_StageSource(&local->io,FS_TRANSACTION_OWNER_MOVER,source->mode,
        source->path,source->pathSize,source->gameCode,0,0,&before,0) ||
        FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT)
        return fail(local,FS_LOCKED);
    /* Backup inspection may have occupied the image. Restore only when its
       verified bytes are not the current source; no repeated Bank read. */
    /* 备份检查可能占用镜像；仅当缓冲不再是当前来源时补读，不重复读取银行。 */
    if (!FsTransaction_ViewBody(&local->io,FS_TRANSACTION_SOURCE,&local->identity,&before)) {
        FsTransactionFingerprint actual;
        if (FsTransaction_ObserveChecked(&local->io,FS_TRANSACTION_SOURCE,&local->identity,
            &actual,0,0)!=FS_TRANSACTION_PRESENT || !same(&actual,&before,sizeof(before)))
            return fail(local,FS_LOCKED);
    }
    local->reservation=record;local->selected=before;
    local->working=1;local->error=0;local->written=0;
    local->validated=(FsTransactionFingerprint){0};
    sourceEvidence(local,coordinator);
    return FS_OK;
}

int FsLocalSource_Read(FsLocalSource *local,uint32_t offset,void *data,uint32_t size)
{
    if (!local || !local->loaded || local->error || (!data && size) ||
        offset>FS_LOCAL_SOURCE_SIZE || size>FS_LOCAL_SOURCE_SIZE-offset) return FS_INVALID;
    uint8_t *out=data;
    for (uint32_t i=0;i<size;i++) out[i]=local->image[offset+i];
    return FS_OK;
}

int FsLocalSource_Write(FsLocalSource *local,uint32_t offset,const void *data,uint32_t size)
{
    if (!local || !local->loaded || !local->working || local->error || !data || !size ||
        offset>FS_LOCAL_SOURCE_SIZE || size>FS_LOCAL_SOURCE_SIZE-offset) {
        if (local) local->error=1;
        return FS_INVALID;
    }
    local->view.bodyValid=0;local->validated=(FsTransactionFingerprint){0};
    const uint8_t *in=data;
    for (uint32_t i=0;i<size;i++) local->image[offset+i]=in[i];
    local->written+=size;
    return FS_OK;
}

int FsLocalSource_Flush(FsLocalSource *local,void *body,uint32_t size,
    FsTransactionFingerprint *out)
{
    if (!local || !local->loaded || !local->working || local->error || !local->written ||
        !body || size!=FS_LOCAL_SOURCE_SIZE || !out) return FS_INVALID;
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT ||
        !sameReservation(&record,&local->reservation) || !FsTransaction_SourceIsWorking(&record))
        return fail(local,FS_LOCKED);
    FsTransactionFingerprint expected={.present=1,.size=FS_LOCAL_SOURCE_SIZE};
    FsTransaction_Hash(local->image,size,expected.hash);
    uint32_t handle=0;uint64_t length=0;
    int ok=FsTransaction_OpenWrite(&local->io,FS_TRANSACTION_SOURCE_TEMP,&record,&handle);
    if (ok) ok=local->io.setSize(handle,size);
    if (ok) ok=local->io.write(handle,0,local->image,size);
    if (handle && !local->io.close(handle)) ok=0;
    if (!ok) return fail(local,FS_IO_ERROR);
    handle=0;
    ok=local->io.openRead(FS_TRANSACTION_SOURCE_TEMP,&record,&handle,&length)==FS_TRANSACTION_PRESENT;
    if (ok) ok=length==size && local->io.read(handle,0,body,size);
    if (handle && !local->io.close(handle)) ok=0;
    if (!ok) return fail(local,FS_IO_ERROR);
    FsTransactionFingerprint actual={.present=1,.size=FS_LOCAL_SOURCE_SIZE};
    FsTransaction_Hash(body,size,actual.hash);
    if (!same(&expected,&actual,sizeof(actual))) return fail(local,FS_INVALID);
    FsTransaction_ViewPut(&local->io,FS_TRANSACTION_SOURCE_TEMP,&record,&actual);
    FsTransaction_ViewRememberBody(&local->io,FS_TRANSACTION_SOURCE_TEMP,&record,&actual,body,size);
    local->validated=actual;*out=actual;
    return FS_OK;
}

int FsLocalSource_Saved(FsLocalSource *local,const FsSource *source,uint32_t nativeResult,
    const FsTransactionIo *coordinator,FsTransactionFingerprint *out)
{
    if (!local || !source || !out || !local->loaded || !local->working || local->error ||
        nativeResult!=1 || !source->expected || !local->validated.present ||
        !same(source->expected,&local->validated,sizeof(local->validated)) ||
        !FsLocalSource_Contains(local,source->path,source->pathSize,source->gameCode)) return FS_INVALID;
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT ||
        !sameReservation(&record,&local->reservation) || !FsTransaction_SourceIsWorking(&record))
        return fail(local,FS_LOCKED);
    bankEvidence(local,coordinator);
    int cleanup=record.phase==FS_TRANSACTION_CLEANUP_PENDING;
    int ok=cleanup?FsTransaction_CleanupSaved(&local->io,record.owner):
        FsTransaction_SourceSaved(&local->io,record.owner);
    if (!ok || FsTransaction_ReadRecord(&local->io,&record)!=FS_TRANSACTION_PRESENT)
        return fail(local,FS_LOCKED);
    FsTransactionFingerprint after=cleanup?record.sourceClean:record.sourceAfter;
    if (!same(&after,&local->validated,sizeof(after))) return fail(local,FS_LOCKED);
    if (record.operation==FS_TRANSACTION_SOURCE_ONLY &&
        !FsTransaction_Finish(&local->io,record.owner)) return fail(local,FS_LOCKED);
    sourceEvidence(local,coordinator);
    local->selected=after;local->working=0;*out=after;
    return FS_OK;
}
