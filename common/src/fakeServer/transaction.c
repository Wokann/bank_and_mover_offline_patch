#include "fakeServer/transaction.h"
#include "fs_system/settings_store.h"
#include "fs_system/diagnostic_log.h"
#include "fs_system/storage_notice.h"

_Static_assert(sizeof(FsTransactionFingerprint)==0x18,"CRC32 fingerprint ABI");
_Static_assert(sizeof(FsTransactionRecord)==0x370,"CRC32 transaction journal ABI");

/* Constant table avoids per-session initialization or a writable global. */
/* 常量查表，不增加会话初始化或可写全局状态。 */
const uint32_t FsHash_CrcTable[256] = {
    0x00000000u,0x77073096u,0xee0e612cu,0x990951bau,0x076dc419u,0x706af48fu,0xe963a535u,0x9e6495a3u,
    0x0edb8832u,0x79dcb8a4u,0xe0d5e91eu,0x97d2d988u,0x09b64c2bu,0x7eb17cbdu,0xe7b82d07u,0x90bf1d91u,
    0x1db71064u,0x6ab020f2u,0xf3b97148u,0x84be41deu,0x1adad47du,0x6ddde4ebu,0xf4d4b551u,0x83d385c7u,
    0x136c9856u,0x646ba8c0u,0xfd62f97au,0x8a65c9ecu,0x14015c4fu,0x63066cd9u,0xfa0f3d63u,0x8d080df5u,
    0x3b6e20c8u,0x4c69105eu,0xd56041e4u,0xa2677172u,0x3c03e4d1u,0x4b04d447u,0xd20d85fdu,0xa50ab56bu,
    0x35b5a8fau,0x42b2986cu,0xdbbbc9d6u,0xacbcf940u,0x32d86ce3u,0x45df5c75u,0xdcd60dcfu,0xabd13d59u,
    0x26d930acu,0x51de003au,0xc8d75180u,0xbfd06116u,0x21b4f4b5u,0x56b3c423u,0xcfba9599u,0xb8bda50fu,
    0x2802b89eu,0x5f058808u,0xc60cd9b2u,0xb10be924u,0x2f6f7c87u,0x58684c11u,0xc1611dabu,0xb6662d3du,
    0x76dc4190u,0x01db7106u,0x98d220bcu,0xefd5102au,0x71b18589u,0x06b6b51fu,0x9fbfe4a5u,0xe8b8d433u,
    0x7807c9a2u,0x0f00f934u,0x9609a88eu,0xe10e9818u,0x7f6a0dbbu,0x086d3d2du,0x91646c97u,0xe6635c01u,
    0x6b6b51f4u,0x1c6c6162u,0x856530d8u,0xf262004eu,0x6c0695edu,0x1b01a57bu,0x8208f4c1u,0xf50fc457u,
    0x65b0d9c6u,0x12b7e950u,0x8bbeb8eau,0xfcb9887cu,0x62dd1ddfu,0x15da2d49u,0x8cd37cf3u,0xfbd44c65u,
    0x4db26158u,0x3ab551ceu,0xa3bc0074u,0xd4bb30e2u,0x4adfa541u,0x3dd895d7u,0xa4d1c46du,0xd3d6f4fbu,
    0x4369e96au,0x346ed9fcu,0xad678846u,0xda60b8d0u,0x44042d73u,0x33031de5u,0xaa0a4c5fu,0xdd0d7cc9u,
    0x5005713cu,0x270241aau,0xbe0b1010u,0xc90c2086u,0x5768b525u,0x206f85b3u,0xb966d409u,0xce61e49fu,
    0x5edef90eu,0x29d9c998u,0xb0d09822u,0xc7d7a8b4u,0x59b33d17u,0x2eb40d81u,0xb7bd5c3bu,0xc0ba6cadu,
    0xedb88320u,0x9abfb3b6u,0x03b6e20cu,0x74b1d29au,0xead54739u,0x9dd277afu,0x04db2615u,0x73dc1683u,
    0xe3630b12u,0x94643b84u,0x0d6d6a3eu,0x7a6a5aa8u,0xe40ecf0bu,0x9309ff9du,0x0a00ae27u,0x7d079eb1u,
    0xf00f9344u,0x8708a3d2u,0x1e01f268u,0x6906c2feu,0xf762575du,0x806567cbu,0x196c3671u,0x6e6b06e7u,
    0xfed41b76u,0x89d32be0u,0x10da7a5au,0x67dd4accu,0xf9b9df6fu,0x8ebeeff9u,0x17b7be43u,0x60b08ed5u,
    0xd6d6a3e8u,0xa1d1937eu,0x38d8c2c4u,0x4fdff252u,0xd1bb67f1u,0xa6bc5767u,0x3fb506ddu,0x48b2364bu,
    0xd80d2bdau,0xaf0a1b4cu,0x36034af6u,0x41047a60u,0xdf60efc3u,0xa867df55u,0x316e8eefu,0x4669be79u,
    0xcb61b38cu,0xbc66831au,0x256fd2a0u,0x5268e236u,0xcc0c7795u,0xbb0b4703u,0x220216b9u,0x5505262fu,
    0xc5ba3bbeu,0xb2bd0b28u,0x2bb45a92u,0x5cb36a04u,0xc2d7ffa7u,0xb5d0cf31u,0x2cd99e8bu,0x5bdeae1du,
    0x9b64c2b0u,0xec63f226u,0x756aa39cu,0x026d930au,0x9c0906a9u,0xeb0e363fu,0x72076785u,0x05005713u,
    0x95bf4a82u,0xe2b87a14u,0x7bb12baeu,0x0cb61b38u,0x92d28e9bu,0xe5d5be0du,0x7cdcefb7u,0x0bdbdf21u,
    0x86d3d2d4u,0xf1d4e242u,0x68ddb3f8u,0x1fda836eu,0x81be16cdu,0xf6b9265bu,0x6fb077e1u,0x18b74777u,
    0x88085ae6u,0xff0f6a70u,0x66063bcau,0x11010b5cu,0x8f659effu,0xf862ae69u,0x616bffd3u,0x166ccf45u,
    0xa00ae278u,0xd70dd2eeu,0x4e048354u,0x3903b3c2u,0xa7672661u,0xd06016f7u,0x4969474du,0x3e6e77dbu,
    0xaed16a4au,0xd9d65adcu,0x40df0b66u,0x37d83bf0u,0xa9bcae53u,0xdebb9ec5u,0x47b2cf7fu,0x30b5ffe9u,
    0xbdbdf21cu,0xcabac28au,0x53b39330u,0x24b4a3a6u,0xbad03605u,0xcdd70693u,0x54de5729u,0x23d967bfu,
    0xb3667a2eu,0xc4614ab8u,0x5d681b02u,0x2a6f2b94u,0xb40bbe37u,0xc30c8ea1u,0x5a05df1bu,0x2d02ef8du
};

static void copyBytes(void *destination,const void *source,uint32_t size)
{
    uint8_t *out=destination;
    const uint8_t *in=source;
    while (size--) *out++=*in++;
}

static void zeroBytes(void *data,uint32_t size)
{
    uint8_t *out=data;
    while (size--) *out++=0;
}

static int equalBytes(const void *left,const void *right,uint32_t size)
{
    const uint8_t *a=left,*b=right;
    while (size--) if (*a++!=*b++) return 0;
    return 1;
}

static int emptyBytes(const void *data,uint32_t size)
{
    const uint8_t *in=data;
    while (size--) if (*in++) return 0;
    return 1;
}

void FsHash_Initialize(FsHashState *state)
{
    state->crc=0xffffffffu;
}

void FsHash_Update(FsHashState *state,const void *data,uint32_t size)
{
    const uint8_t *in=data;
    uint32_t crc=state->crc;
    while (size--) crc=FsHash_CrcTable[(crc^*in++)&255u]^(crc>>8);
    state->crc=crc;
}

void FsHash_Finish(FsHashState *state,uint8_t hash[FS_TRANSACTION_HASH_SIZE])
{
    uint32_t crc=state->crc^0xffffffffu;
    for (uint32_t i=0;i<FS_TRANSACTION_HASH_SIZE;i++)
        hash[i]=(uint8_t)(crc>>(24-8*i));
}

void FsTransaction_Hash(const void *data,uint32_t size,uint8_t hash[FS_TRANSACTION_HASH_SIZE])
{
    FsHashState state;
    FsHash_Initialize(&state);
    FsHash_Update(&state,data,size);
    FsHash_Finish(&state,hash);
}


static int absentFingerprint(const FsTransactionFingerprint *value)
{
    return !value->present && !value->reserved && !value->reserved14 && !value->size &&
        emptyBytes(value->hash,sizeof(value->hash));
}

static int validFingerprint(const FsTransactionFingerprint *value)
{
    return value->present==1?!value->reserved && !value->reserved14 && value->size<=0x4000000:
        absentFingerprint(value);
}

/* Thumb C stays compact; buffer arbitration alone needs ARM atomics. */
/* 普通 C 使用紧凑 Thumb；仅缓冲仲裁需要 ARM 原子指令。 */
#if defined(__arm__)
__attribute__((target("arm"),noinline))
#endif
static uint8_t *acquireBuffer(const FsTransactionIo *io,uint8_t *fallback,uint32_t *size,uint64_t needed)
{
    *size=FS_TRANSACTION_BUFFER_SIZE;
    if (needed>*size && io->buffer && io->bufferSize>=*size && io->bufferBusy &&
        !__sync_lock_test_and_set(io->bufferBusy,1)) {
        *size=io->bufferSize;
        return io->buffer;
    }
    return fallback;
}

#if defined(__arm__)
__attribute__((target("arm"),noinline))
#endif
static void releaseBuffer(const FsTransactionIo *io,const uint8_t *buffer)
{
    if (buffer==io->buffer) __sync_lock_release(io->bufferBusy);
}

int FsTransaction_ObserveChecked(const FsTransactionIo *io,uint32_t file,
    const FsTransactionRecord *record,FsTransactionFingerprint *value,
    FsTransactionInspect inspect,void *context)
{
    FsHashState state;
    uint8_t fallback[FS_TRANSACTION_BUFFER_SIZE];
    uint32_t handle=0;
    uint64_t offset=0,size=0;
    int result=io->openRead(file,record,&handle,&size),ok=1;
    zeroBytes(value,sizeof(*value));
    if (result!=FS_TRANSACTION_PRESENT) {
        if (result==FS_TRANSACTION_MISSING) FsTransaction_ViewPut(io,file,record,value);
        else FsTransaction_ViewClear(io);
        return result;
    }
    uint32_t capacity;
    uint8_t *buffer=acquireBuffer(io,fallback,&capacity,size);
    if (buffer==io->buffer && io->view) io->view->bodyValid=0;
    if (size>0x4000000) ok=0;
    FsHash_Initialize(&state);
    while (ok && offset<size) {
        uint64_t remaining=size-offset;
        uint32_t chunk=remaining>capacity?capacity:(uint32_t)remaining;
        if (!io->read(handle,offset,buffer,chunk)) { ok=0; break; }
        if (inspect && !inspect(context,size,offset,buffer,chunk)) { ok=0; break; }
        FsHash_Update(&state,buffer,chunk);
        offset+=chunk;
    }
    if (!io->close(handle)) ok=0;
    if (!ok) {
        releaseBuffer(io,buffer);FsTransaction_ViewClear(io);return FS_TRANSACTION_IO_ERROR;
    }
    value->present=1;
    value->size=size;
    FsHash_Finish(&state,value->hash);
    if (buffer==io->buffer && size<=capacity && io->view) {
        io->view->body=*value;io->view->bodyFile=file;io->view->bodyValid=1;
    }
    FsTransaction_ViewPut(io,file,record,value);
    releaseBuffer(io,buffer);
    return FS_TRANSACTION_PRESENT;
}

int FsTransaction_Observe(const FsTransactionIo *io,uint32_t file,
    const FsTransactionRecord *record,FsTransactionFingerprint *value)
{
    if (FsTransaction_ViewGet(io,file,record,value))
        return value->present?FS_TRANSACTION_PRESENT:FS_TRANSACTION_MISSING;
    return FsTransaction_ObserveChecked(io,file,record,value,0,0);
}

int FsTransaction_Matches(const FsTransactionIo *io,uint32_t file,
    const FsTransactionRecord *record,const FsTransactionFingerprint *value)
{
    FsTransactionFingerprint actual;
    if (FsTransaction_Observe(io,file,record,&actual)==FS_TRANSACTION_IO_ERROR) return 0;
    return equalBytes(&actual,value,sizeof(actual));
}

static int fileStatus(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record)
{
    uint32_t handle=0;
    uint64_t size=0;
    int result=io->openRead(file,record,&handle,&size);
    if (result==FS_TRANSACTION_IO_ERROR ||
        (result==FS_TRANSACTION_PRESENT && !io->close(handle))) {
        FsTransaction_ViewClear(io);return FS_TRANSACTION_IO_ERROR;
    }
    return result;
}

static int missing(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record)
{
    return fileStatus(io,file,record)==FS_TRANSACTION_MISSING;
}

/* A loaded digest describes the session image, not the continued existence of
   its physical file. Publication still checks existence/length without another
   whole-body read; changed bodies are rechecked at the next load/write boundary. */
/* 已载入摘要代表本次内存镜像，不证明物理文件一直存在。发布仍检查存在性及长度，
   不再读全正文；正文变化在下一次载入／实际写入边界重新校验。 */
static int physicalFileIs(const FsTransactionIo *io,uint32_t file,
    const FsTransactionRecord *record,const FsTransactionFingerprint *value)
{
    uint32_t handle=0;uint64_t size=0;
    int status=io->openRead(file,record,&handle,&size);
    int ok=value->present?status==FS_TRANSACTION_PRESENT && size==value->size:
        status==FS_TRANSACTION_MISSING;
    if (status==FS_TRANSACTION_PRESENT && !io->close(handle)) ok=0;
    if (!ok) FsTransaction_ViewClear(io);
    return ok;
}

static int writeFile(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,
    const void *data,uint32_t size)
{
    FsDiag_Checkpoint("transaction.file.begin",0,file,size,0);
    uint32_t handle=0;
    int ok=FsTransaction_OpenWrite(io,file,record,&handle);
    if (ok) ok=io->setSize(handle,size);
    if (ok) ok=io->write(handle,0,data,size);
    if (handle && !io->close(handle)) ok=0;
    FsDiag_Record("transaction.file.end",ok?0:-1,file,size,0);
    if (!ok) FsTransaction_ViewClear(io);
    return ok;
}

int FsTransaction_Copy(const FsTransactionIo *io,uint32_t from,uint32_t to,
    const FsTransactionRecord *record,const FsTransactionFingerprint *expected)
{
    uint8_t fallback[FS_TRANSACTION_BUFFER_SIZE];
    uint32_t input=0,output=0;
    uint64_t size=0,offset=0;
    int ok;
    if (!expected->present) return missing(io,from,record);
    uint32_t capacity;
    uint8_t *buffer=acquireBuffer(io,fallback,&capacity,expected->size);
    if (expected->size<=capacity) {
        /* Read and authenticate the actual copy bytes before touching the
           destination. The complete image fits the existing workspace; no
           separate pre-read is needed. Release it before verification so
           nested readers can use the large buffer instead of 1 KiB chunks. */
        /* 整份能放入既有缓冲时，先读取并校验实际复制字节，再碰目标；省去
           独立预读。回读前释放缓冲，让检查继续整份读取而非退回 1 KiB 分块。 */
        FsTransactionFingerprint actual;
        zeroBytes(&actual,sizeof(actual));
        int loaded=buffer==io->buffer && FsTransaction_ViewBody(io,from,record,expected);
        if (loaded) { size=expected->size;ok=physicalFileIs(io,from,record,expected); }
        else {
            if (buffer==io->buffer && io->view) io->view->bodyValid=0;
            ok=io->openRead(from,record,&input,&size)==FS_TRANSACTION_PRESENT &&
                size==expected->size && (!size || io->read(input,0,buffer,(uint32_t)size));
            if (input && !io->close(input)) ok=0;
        }
        if (ok && !loaded) {
            actual.present=1; actual.size=size;
            FsTransaction_Hash(buffer,(uint32_t)size,actual.hash);
            ok=equalBytes(&actual,expected,sizeof(actual));
        }
        if (ok) ok=FsTransaction_OpenWrite(io,to,record,&output);
        if (ok) ok=io->setSize(output,size);
        if (ok && size) ok=io->write(output,0,buffer,(uint32_t)size);
        if (output && !io->close(output)) ok=0;
        if (!ok) FsTransaction_ViewClear(io);
        releaseBuffer(io,buffer);
        return ok && FsTransaction_Matches(io,to,record,expected) &&
            FsTransaction_Matches(io,from,record,expected);
    }
    releaseBuffer(io,buffer);
    /* Larger images/reentrant callers retain the checked streaming path. */
    /* 大镜像及缓冲被占用时保留原有流式检查路径。 */
    if (!FsTransaction_Matches(io,from,record,expected)) return 0;
    buffer=acquireBuffer(io,fallback,&capacity,expected->size);
    if (buffer==io->buffer && io->view) io->view->bodyValid=0;
    ok=io->openRead(from,record,&input,&size)==FS_TRANSACTION_PRESENT;
    if (ok) ok=FsTransaction_OpenWrite(io,to,record,&output);
    if (ok) ok=io->setSize(output,size);
    while (ok && offset<size) {
        uint64_t remaining=size-offset;
        uint32_t chunk=remaining>capacity?capacity:(uint32_t)remaining;
        ok=io->read(input,offset,buffer,chunk) && io->write(output,offset,buffer,chunk);
        offset+=chunk;
    }
    if (output && !io->close(output)) ok=0;
    if (input && !io->close(input)) ok=0;
    if (!ok) FsTransaction_ViewClear(io);
    releaseBuffer(io,buffer);
    return ok && FsTransaction_Matches(io,to,record,expected) &&
        FsTransaction_Matches(io,from,record,expected);
}

static int validRecord(const FsTransactionRecord *record)
{
    const FsTransactionFingerprint *values=&record->bankBefore;
    uint8_t hash[FS_TRANSACTION_HASH_SIZE];
    uint32_t i;
    if (record->magic!=FS_TRANSACTION_MAGIC ||
        (record->version!=FS_TRANSACTION_VERSION && record->version!=FS_TRANSACTION_VERSION_LEGACY) ||
        record->recordSize!=sizeof(*record) || !record->sequence || !record->transaction ||
        record->transaction>record->sequence || record->owner<FS_TRANSACTION_OWNER_BANK ||
        record->owner>FS_TRANSACTION_OWNER_MOVER || record->mode>3 ||
        record->operation<FS_TRANSACTION_SOURCE_SAVE ||
        record->operation>FS_TRANSACTION_ADOPT_BANK ||
        record->phase<FS_TRANSACTION_PREPARING ||
        record->phase>FS_TRANSACTION_DETACHED_ROLLED_BACK ||
        !record->participants || record->participants>7 || record->flags>127 ||
        record->sourceKind>FS_TRANSACTION_NDS_FILE) return 0;
    for (i=0;i<8;i++) if (!validFingerprint(&values[i])) return 0;
    if (record->sourceKind==FS_TRANSACTION_NDS_FILE) {
        uint32_t units=record->sourcePathSize/sizeof(uint16_t);
        if (!(record->participants&FS_TRANSACTION_HAS_SOURCE) ||
            record->sourcePathSize<4 || (record->sourcePathSize&1) ||
            units>FS_TRANSACTION_PATH_CAPACITY || record->sourcePath[0]!='/' ||
            record->sourcePath[units-1] || !record->sourceBefore.present) return 0;
        for (i=0;i+1<units;i++) if (!record->sourcePath[i]) return 0;
        /* Old settled descriptors remain readable; old pending global work
           files are neither resumed, migrated nor attributed to a new path. */
        /* 仅保留旧已结束描述符的读取；旧固定路径未决文件不续办、不迁移到新路径。 */
        if ((record->version==FS_TRANSACTION_VERSION_LEGACY &&
                record->phase!=FS_TRANSACTION_COMPLETED && record->phase!=FS_TRANSACTION_ABORTED &&
                record->phase!=FS_TRANSACTION_DETACHED_ROLLED_BACK) ||
            !FsTransaction_SourcePath(record,FS_TRANSACTION_SOURCE,0)) return 0;
    } else if (record->sourcePathSize || record->sourceGameCode || record->sourceTitleId ||
        !emptyBytes(record->sourcePath,sizeof(record->sourcePath))) return 0;
    if (!record->serverTransaction.dataId || record->serverTransaction.reserved14 ||
        record->serverTransaction.updateVersion<record->serverTransaction.currentVersion)
        return 0;
    if ((record->participants&FS_TRANSACTION_HAS_SOURCE)!=0) {
        if (record->sourceKind==FS_TRANSACTION_NO_SOURCE) return 0;
    } else if (record->sourceKind!=FS_TRANSACTION_NO_SOURCE) return 0;
    FsTransaction_Hash(record,__builtin_offsetof(FsTransactionRecord,recordHash),hash);
    return equalBytes(hash,record->recordHash,sizeof(hash));
}

static int readExact(const FsTransactionIo *io,uint32_t file,FsTransactionRecord *record)
{
    uint32_t handle=0;
    uint64_t size=0;
    int result=io->openRead(file,0,&handle,&size),ok;
    if (result!=FS_TRANSACTION_PRESENT) {
        if (result==FS_TRANSACTION_IO_ERROR) FsTransaction_ViewClear(io);
        return result;
    }
    ok=size==sizeof(*record) && io->read(handle,0,record,sizeof(*record));
    if (!io->close(handle)) ok=0;
    if (ok && validRecord(record)) return FS_TRANSACTION_PRESENT;
    FsTransaction_ViewClear(io);return FS_TRANSACTION_IO_ERROR;
}

static int noWorkingFiles(const FsTransactionIo *,const FsTransactionRecord *);

/* An empty backup marks bootstrap, never a published decision. No participant
   may be changed before the first complete journal is published. */
/* 空备份标记首次建立，不是正式决定；首份完整日志发布前不得修改参与文件。 */
static int initialMarker(const FsTransactionIo *io)
{
    uint32_t handle=0;
    uint64_t size=0;
    if (io->openRead(FS_TRANSACTION_JOURNAL_BACKUP,0,&handle,&size)!=FS_TRANSACTION_PRESENT)
        return 0;
    return io->close(handle) && !size;
}

int FsTransaction_ReadRecord(const FsTransactionIo *io,FsTransactionRecord *record)
{
    int result=readExact(io,FS_TRANSACTION_JOURNAL,record);
    /* Only a missing main permits the previous published record to resume.
       An unreadable/invalid main is preserved; a temporary is not authority. */
    /* 仅正式日志缺失时从上一条已发布记录续办；不可读或无效的正式日志保留，
       临时日志不能成为权威。 */
    if (result==FS_TRANSACTION_MISSING) {
        result=readExact(io,FS_TRANSACTION_JOURNAL_BACKUP,record);
        /* tmp is never authority. Missing published history remains missing;
           independent mutation guards decide whether a checked baseline can
           replace the orphan. Invalid/nonempty published records still fail. */
        /* tmp 不作权威；正式历史缺失仍返回缺失，由写入保护独立判断能否用已校验
           基线替代孤立暂存。非空损坏／不可读的正式记录仍然失败。 */
        if (result==FS_TRANSACTION_IO_ERROR && initialMarker(io))
            return FS_TRANSACTION_MISSING;
    }
    return result;
}

static int orphanAdoptionReady(const FsTransactionIo *io,const FsTransactionRecord *record)
{
    if (record->operation!=FS_TRANSACTION_ADOPT_BANK ||
        !missing(io,FS_TRANSACTION_SOURCE_TEMP,record)) return 0;
    if (!record->bankAfter.present) {
        if (!missing(io,FS_TRANSACTION_BANK,record) ||
            !missing(io,FS_TRANSACTION_BACKUP,record)) return 0;
    } else if (record->bankBefore.present) {
        if (!physicalFileIs(io,FS_TRANSACTION_BANK,record,&record->bankAfter)) return 0;
    } else if (!missing(io,FS_TRANSACTION_BANK,record) ||
        !physicalFileIs(io,FS_TRANSACTION_BACKUP,record,&record->bankAfter)) return 0;
    if (missing(io,FS_TRANSACTION_TURTLE_TEMP,record)) return 1;
    /* An orphan settings tmp is discardable only behind the exact valid
       published settings image captured by this new adoption decision. */
    /* 孤立设置暂存只在本次决定登记的正式设置完整有效时才可放弃。 */
    uint8_t settings[512],hash[FS_TRANSACTION_HASH_SIZE];
    uint32_t handle=0;uint64_t size=0;
    int ok=record->turtleCurrent.present && record->turtleCurrent.size==sizeof(settings) &&
        io->openRead(FS_TRANSACTION_TURTLE,record,&handle,&size)==FS_TRANSACTION_PRESENT &&
        size==sizeof(settings) && io->read(handle,0,settings,sizeof(settings));
    if (handle && !io->close(handle)) ok=0;
    if (!ok) { FsTransaction_ViewClear(io);return 0; }
    FsTransaction_Hash(settings,sizeof(settings),hash);
    return equalBytes(hash,record->turtleCurrent.hash,sizeof(hash)) &&
        FsSettings_ValidPrefix(settings) && settings[0x28] && settings[0x28]<=10 &&
        settings[0x28]!=6 && !settings[0x29] && settings[0x2a]<=1 && !settings[0x2b];
}

static int persist(const FsTransactionIo *io,FsTransactionRecord *record,uint32_t phase)
{
    FsDiag_Checkpoint("transaction.persist.begin",0,record->operation,phase,record->sequence);
    FsTransactionRecord actual;
    if (record->sequence==0xffffffffu) return 0;
    if (!record->sequence) {
        if (!missing(io,FS_TRANSACTION_JOURNAL,0) ||
            (!noWorkingFiles(io,0) && !orphanAdoptionReady(io,record))) return 0;
        if (!initialMarker(io)) {
            uint32_t handle=0;
            int ok=missing(io,FS_TRANSACTION_JOURNAL_BACKUP,0) &&
                FsTransaction_OpenWrite(io,FS_TRANSACTION_JOURNAL_BACKUP,0,&handle);
            if (ok) ok=io->setSize(handle,0);
            if (handle && !io->close(handle)) ok=0;
            if (!ok || !initialMarker(io)) return 0;
        }
    }
    record->phase=phase;
    record->sequence++;
    FsTransaction_Hash(record,__builtin_offsetof(FsTransactionRecord,recordHash),record->recordHash);
    if (!FsTransaction_Remove(io,FS_TRANSACTION_JOURNAL_TEMP,record) ||
        !writeFile(io,FS_TRANSACTION_JOURNAL_TEMP,record,record,sizeof(*record)) ||
        readExact(io,FS_TRANSACTION_JOURNAL_TEMP,&actual)!=FS_TRANSACTION_PRESENT ||
        !equalBytes(record,&actual,sizeof(actual))) return 0;
    int status=readExact(io,FS_TRANSACTION_JOURNAL,&actual);
    if (status==FS_TRANSACTION_PRESENT) {
        if (actual.sequence!=record->sequence-1 ||
            !FsTransaction_Remove(io,FS_TRANSACTION_JOURNAL_BACKUP,record) ||
            !FsTransaction_Rename(io,FS_TRANSACTION_JOURNAL,FS_TRANSACTION_JOURNAL_BACKUP,record)) return 0;
    } else if (status==FS_TRANSACTION_MISSING) {
        status=readExact(io,FS_TRANSACTION_JOURNAL_BACKUP,&actual);
        if (status==FS_TRANSACTION_PRESENT) {
            if (actual.sequence!=record->sequence-1) return 0;
        } else if (record->sequence!=1 || !initialMarker(io) ||
            (!noWorkingFiles(io,0) && !orphanAdoptionReady(io,record))) return 0;
    } else return 0;
    if (!FsTransaction_Rename(io,FS_TRANSACTION_JOURNAL_TEMP,FS_TRANSACTION_JOURNAL,record)) return 0;
    return FsTransaction_ReadRecord(io,&actual)==FS_TRANSACTION_PRESENT &&
        equalBytes(record,&actual,sizeof(actual));
}

static int settled(const FsTransactionRecord *record)
{
    return record->phase==FS_TRANSACTION_COMPLETED ||
        record->phase==FS_TRANSACTION_ABORTED ||
        record->phase==FS_TRANSACTION_DETACHED_ROLLED_BACK;
}

static int noWorkingFiles(const FsTransactionIo *io,const FsTransactionRecord *record)
{
    return missing(io,FS_TRANSACTION_TEMP,record) &&
        missing(io,FS_TRANSACTION_SOURCE_TEMP,record) &&
        missing(io,FS_TRANSACTION_TURTLE_TEMP,record);
}

static int discardUnboundTemporary(const FsTransactionIo *io,uint32_t main,uint32_t backup,
    uint32_t temp,const FsTransactionRecord *source)
{
    int found=fileStatus(io,temp,source);
    if (found!=FS_TRANSACTION_PRESENT) return found==FS_TRANSACTION_MISSING;
    int primary=main==FS_TRANSACTION_FILE_COUNT?FS_TRANSACTION_MISSING:fileStatus(io,main,source);
    int saved=fileStatus(io,backup,source);
    if (primary==FS_TRANSACTION_IO_ERROR || saved==FS_TRANSACTION_IO_ERROR) return 0;
    if (primary==FS_TRANSACTION_PRESENT || saved==FS_TRANSACTION_PRESENT) return 1;
    return FsTransaction_Remove(io,temp,source) && missing(io,temp,source);
}

int FsTransaction_ReconcileOrphanParticipants(const FsTransactionIo *io)
{
    FsTransactionRecord record;
    int state=FsTransaction_ReadRecord(io,&record);
    if (state!=FS_TRANSACTION_MISSING) return state==FS_TRANSACTION_PRESENT;
    /* A published decision always wins. Source siblings are inspected only
       after a game is selected, not by scanning every save at server startup. */
    /* 正式决定优先；来源工作文件仅在选游戏后检查，启动服务器不扫描所有存档。 */
    return discardUnboundTemporary(io,FS_TRANSACTION_TURTLE,FS_TRANSACTION_TURTLE_BACKUP,
        FS_TRANSACTION_TURTLE_TEMP,0);
}

int FsTransaction_ReconcileOrphanSource(const FsTransactionIo *io,const FsTransactionRecord *source)
{
    FsTransactionRecord record;
    if (!FsTransaction_SourcePath(source,FS_TRANSACTION_SOURCE,0)) return 0;
    int state=FsTransaction_ReadRecord(io,&record);
    if (state!=FS_TRANSACTION_MISSING) return state==FS_TRANSACTION_PRESENT;
    return discardUnboundTemporary(io,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_BACKUP,
        FS_TRANSACTION_SOURCE_TEMP,source);
}

static int turtleBaseline(const FsTransactionRecord *record)
{
    /* Participation or a recorded after-image also establishes a baseline. */
    /* 设置参与或已有目标镜像同样构成基线；旧格式仍由日志版本检查拒绝。 */
    return (record->flags&FS_TRANSACTION_TURTLE_BASELINE) ||
        (record->participants&FS_TRANSACTION_HAS_TURTLE) || record->turtleAfter.present;
}

static int turtleIs(const FsTransactionIo *io,const FsTransactionRecord *record)
{
    return !turtleBaseline(record) ||
        FsTransaction_Matches(io,FS_TRANSACTION_TURTLE,record,&record->turtleCurrent) ||
        ((record->flags&FS_TRANSACTION_TURTLE_WRITE_PENDING) &&
         FsTransaction_Matches(io,FS_TRANSACTION_TURTLE,record,&record->turtleAfter));
}

int FsTransaction_TurtleGuard(const FsTransactionIo *io)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING)
        return FsTransaction_ReconcileOrphanParticipants(io) && missing(io,FS_TRANSACTION_TURTLE_TEMP,0);
    return status==FS_TRANSACTION_PRESENT && turtleIs(io,&record);
}

int FsTransaction_GuardBank(const FsTransactionIo *io,const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING) return noWorkingFiles(io,0);
    if (status!=FS_TRANSACTION_PRESENT || !settled(&record)) return 0;
    return (bank?equalBytes(bank,&record.bankAfter,sizeof(*bank)):
        FsTransaction_Matches(io,FS_TRANSACTION_BANK,&record,&record.bankAfter)) &&
        turtleIs(io,&record) && noWorkingFiles(io,&record);
}

int FsTransaction_Guard(const FsTransactionIo *io)
{
    return FsTransaction_GuardBank(io,0);
}

static int publishFile(const FsTransactionIo *io,FsTransactionRecord *record,
    uint32_t main,uint32_t temporary,uint32_t backup,const FsTransactionFingerprint *before,
    const FsTransactionFingerprint *after)
{
    FsTransactionFingerprint actual;
    if (FsTransaction_Observe(io,main,record,&actual)==FS_TRANSACTION_IO_ERROR) return 0;
    /* One loaded observation distinguishes before, after and missing. A
       mismatch with after does not require rereading the same main for before. */
    /* 一份已载入观察区分前版、后版及缺失；不因不匹配后版就再读同一主文件判前版。 */
    if (!physicalFileIs(io,main,record,&actual)) return 0;
    if (equalBytes(&actual,after,sizeof(actual))) {
        if (!missing(io,temporary,record) &&
            (!FsTransaction_Matches(io,temporary,record,after) ||
             !FsTransaction_Remove(io,temporary,record))) return 0;
        return missing(io,temporary,record);
    }
    if (actual.present && !equalBytes(&actual,before,sizeof(actual))) return 0;
    const FsTransactionFingerprint *snapshot=before;
    if (main==FS_TRANSACTION_TURTLE) snapshot=&record->turtleBefore;
    else if (main==FS_TRANSACTION_SOURCE) snapshot=&record->sourceBefore;
    else if (record->operation==FS_TRANSACTION_RESTORE_BACKUP) snapshot=&record->bankAfter;
    if (snapshot->present && (!FsTransaction_Matches(io,backup,record,snapshot) ||
        !physicalFileIs(io,backup,record,snapshot))) return 0;
    if (after->present && (!FsTransaction_Matches(io,temporary,record,after) ||
        !physicalFileIs(io,temporary,record,after))) return 0;
    if (!FsTransaction_Remove(io,main,record)) return 0;
    if (after->present && !FsTransaction_Rename(io,temporary,main,record)) return 0;
    return FsTransaction_Matches(io,main,record,after) && missing(io,temporary,record);
}

static int publishTurtle(const FsTransactionIo *io,FsTransactionRecord *record)
{
    if (!(record->flags&FS_TRANSACTION_TURTLE_WRITE_PENDING)) return 1;
    if (!publishFile(io,record,FS_TRANSACTION_TURTLE,FS_TRANSACTION_TURTLE_TEMP,
        FS_TRANSACTION_TURTLE_BACKUP,&record->turtleCurrent,&record->turtleAfter)) return 0;
    copyBytes(&record->turtleCurrent,&record->turtleAfter,sizeof(record->turtleCurrent));
    record->flags&=~FS_TRANSACTION_TURTLE_WRITE_PENDING;
    return persist(io,record,record->phase);
}

static int bankIs(const FsTransactionIo *io,FsTransactionRecord *record,int after)
{
    /* Source/settings-only saves preserve Bank metadata in the shared record,
       but do not participate in or read the unrelated Bank file. */
    /* 来源／设置单独保存仅保留共用记录的银行元数据，不参与或读取银行正文。 */
    if (record->operation==FS_TRANSACTION_SOURCE_ONLY ||
        record->operation==FS_TRANSACTION_SETTINGS_ONLY) return 1;
    return FsTransaction_Matches(io,FS_TRANSACTION_BANK,record,
        after?&record->bankAfter:&record->bankBefore);
}

static int restoreFile(const FsTransactionIo *io,FsTransactionRecord *record,
    uint32_t main,uint32_t temporary,uint32_t backup,const FsTransactionFingerprint *before,
    const FsTransactionFingerprint *after)
{
    if (FsTransaction_Matches(io,main,record,before) &&
        physicalFileIs(io,main,record,before)) return 1;
    if (!FsTransaction_Matches(io,main,record,after) && !missing(io,main,record)) return 0;
    if (before->present &&
        !FsTransaction_Copy(io,backup,temporary,record,before)) return 0;
    if (!FsTransaction_Remove(io,main,record)) return 0;
    if (before->present && !FsTransaction_Rename(io,temporary,main,record)) return 0;
    return FsTransaction_Matches(io,main,record,before);
}

static int rollbackRecord(const FsTransactionIo *io,FsTransactionRecord *record,int detached)
{
    if (!detached && record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE) return 0;
    if (!detached && record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        !restoreFile(io,record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
            FS_TRANSACTION_SOURCE_BACKUP,&record->sourceBefore,
            record->sourceClean.present &&
                !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,record,&record->sourceAfter)?
                &record->sourceClean:&record->sourceAfter)) return 0;
    if ((record->participants&FS_TRANSACTION_HAS_BANK) &&
        !restoreFile(io,record,FS_TRANSACTION_BANK,FS_TRANSACTION_TEMP,
            FS_TRANSACTION_BACKUP,&record->bankBefore,&record->bankAfter)) return 0;
    if ((record->participants&FS_TRANSACTION_HAS_TURTLE) &&
        !restoreFile(io,record,FS_TRANSACTION_TURTLE,FS_TRANSACTION_TURTLE_TEMP,
            FS_TRANSACTION_TURTLE_BACKUP,&record->turtleBefore,&record->turtleCurrent)) return 0;
    if (!FsTransaction_Remove(io,FS_TRANSACTION_TEMP,record) ||
        !FsTransaction_Remove(io,FS_TRANSACTION_SOURCE_TEMP,record) ||
        !FsTransaction_Remove(io,FS_TRANSACTION_TURTLE_TEMP,record)) return 0;
    if (!detached && record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        !FsTransaction_Remove(io,FS_TRANSACTION_SOURCE_BACKUP,record)) return 0;
    copyBytes(&record->bankAfter,&record->bankBefore,sizeof(record->bankAfter));
    copyBytes(&record->turtleCurrent,&record->turtleBefore,sizeof(record->turtleCurrent));
    copyBytes(&record->turtleAfter,&record->turtleBefore,sizeof(record->turtleAfter));
    record->flags&=~FS_TRANSACTION_TURTLE_WRITE_PENDING;
    return persist(io,record,detached?FS_TRANSACTION_DETACHED_ROLLED_BACK:FS_TRANSACTION_ABORTED);
}

static int abortUnpublished(const FsTransactionIo *io,FsTransactionRecord *record)
{
    if (!bankIs(io,record,0)) return 0;
    if (record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,record,&record->sourceBefore)) return 0;
    if (record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE &&
        record->phase!=FS_TRANSACTION_PREPARING) return 0;
    if (record->participants&FS_TRANSACTION_HAS_TURTLE) {
        if (!FsTransaction_Matches(io,FS_TRANSACTION_TURTLE,record,&record->turtleBefore) &&
            !FsTransaction_Matches(io,FS_TRANSACTION_TURTLE,record,&record->turtleCurrent)) return 0;
    }
    /* Native PREPARING has not yet admitted a source write. */
    /* 原生来源处于 PREPARING 时尚未允许来源写入。 */
    if (record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE) {
        record->sourceKind=FS_TRANSACTION_NO_SOURCE;
        record->participants&=~FS_TRANSACTION_HAS_SOURCE;
        record->sourceGameCode=0;
        zeroBytes(record->sourcePath,sizeof(record->sourcePath));
        zeroBytes(&record->sourceBefore,sizeof(record->sourceBefore)*3);
    }
    if (!persist(io,record,FS_TRANSACTION_ROLLBACK_DECIDED)) return 0;
    return rollbackRecord(io,record,0);
}

static int publish(const FsTransactionIo *io,FsTransactionRecord *record)
{
    if (!publishTurtle(io,record)) return 0;
    if (record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        !publishFile(io,record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
            FS_TRANSACTION_SOURCE_BACKUP,&record->sourceBefore,&record->sourceAfter)) return 0;
    if ((record->participants&FS_TRANSACTION_HAS_BANK) &&
        !publishFile(io,record,FS_TRANSACTION_BANK,FS_TRANSACTION_TEMP,
            FS_TRANSACTION_BACKUP,&record->bankBefore,&record->bankAfter)) return 0;
    if (!bankIs(io,record,1)) return 0;
    return persist(io,record,FS_TRANSACTION_PUBLISHED);
}

int FsTransaction_Finish(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING) return noWorkingFiles(io,0);
    if (status!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        !publishTurtle(io,&record) || !bankIs(io,&record,1) ||
        ((record.participants&FS_TRANSACTION_HAS_BANK) &&
         !physicalFileIs(io,FS_TRANSACTION_BANK,&record,&record.bankAfter)) || !turtleIs(io,&record)) return 0;
    if (record.phase==FS_TRANSACTION_COMPLETED)
        return (record.sourceKind!=FS_TRANSACTION_NDS_FILE ||
            physicalFileIs(io,FS_TRANSACTION_SOURCE,&record,
                record.sourceClean.present?&record.sourceClean:&record.sourceAfter)) &&
            noWorkingFiles(io,&record);
    if (record.phase!=FS_TRANSACTION_PUBLISHED &&
        record.phase!=FS_TRANSACTION_CLEANUP_READY) return 0;
    if (record.operation==FS_TRANSACTION_SOURCE_SAVE &&
        !(record.flags&FS_TRANSACTION_CLEANUP_ACKNOWLEDGED)) return 0;
    if (record.operation==FS_TRANSACTION_SOURCE_SAVE &&
        record.sourceKind==FS_TRANSACTION_NDS_FILE) {
        if (!(record.flags&FS_TRANSACTION_CLEANUP_ACKNOWLEDGED)) return 0;
        if (!record.sourceClean.present ||
            !publishFile(io,&record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
                FS_TRANSACTION_SOURCE_BACKUP,&record.sourceAfter,&record.sourceClean)) return 0;
    }
    if (!noWorkingFiles(io,&record)) return 0;
    if (record.sourceKind==FS_TRANSACTION_NDS_FILE &&
        !physicalFileIs(io,FS_TRANSACTION_SOURCE,&record,
            record.operation==FS_TRANSACTION_SOURCE_SAVE?&record.sourceClean:&record.sourceAfter)) return 0;
    /* All NDS publication and token-cleanup acknowledgements precede retirement.
       A failed delete or journal publication resumes here with the checked sav;
       first/cleanup publication is idempotent and no longer needs the old bak.
       Bank/settings backups remain pre-write generations, never final mirrors. */
    /* NDS 的发布及清令牌保存均确认后才清理恢复文件。删除／日志发布失败可凭已核对
       sav 在此续办，原样重入发布不再依赖旧 bak；银行／设置备份仍保留写入前一代。 */
    if (record.sourceKind==FS_TRANSACTION_NDS_FILE &&
        !FsTransaction_Remove(io,FS_TRANSACTION_SOURCE_BACKUP,&record)) return 0;
    if (record.operation==FS_TRANSACTION_SOURCE_SAVE || record.operation==FS_TRANSACTION_BANK_ONLY)
        record.serverTransaction.currentVersion=record.serverTransaction.updateVersion;
    return persist(io,&record,FS_TRANSACTION_COMPLETED);
}

static void newRecord(FsTransactionRecord *record,uint32_t owner,uint32_t operation,
    uint32_t mode,uint32_t participants,uint32_t sequence)
{
    zeroBytes(record,sizeof(*record));
    record->magic=FS_TRANSACTION_MAGIC;
    record->version=FS_TRANSACTION_VERSION;
    record->recordSize=sizeof(*record);
    record->sequence=sequence;
    record->transaction=sequence+1;
    record->owner=owner;
    record->operation=operation;
    record->mode=mode;
    record->participants=participants;
}

int FsTransaction_ReconcileTurtle(const FsTransactionIo *io)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING)
        return FsTransaction_ReconcileOrphanParticipants(io) && missing(io,FS_TRANSACTION_TURTLE_TEMP,0);
    if (status!=FS_TRANSACTION_PRESENT) return 0;
    if (!turtleBaseline(&record) || turtleIs(io,&record)) return 1;
    /* A recorded settings publication must finish through its existing path,
       not restore the previous settings over the new saved generation. */
    /* 已登记的设置发布仍走原续办路径，不能恢复旧设置覆盖新保存的版本。 */
    if (record.flags&FS_TRANSACTION_TURTLE_WRITE_PENDING) return 0;
    if (!missing(io,FS_TRANSACTION_TURTLE,&record)) return 0;
    FsTransactionFingerprint backup,temporary;
    if (FsTransaction_Observe(io,FS_TRANSACTION_TURTLE_BACKUP,&record,&backup)==
            FS_TRANSACTION_IO_ERROR ||
        FsTransaction_Observe(io,FS_TRANSACTION_TURTLE_TEMP,&record,&temporary)==
            FS_TRANSACTION_IO_ERROR) return 0;
    if (settled(&record) && !backup.present && !temporary.present) {
        /* All settings were deliberately removed: let the native initializer
           recreate them. The Bank descriptor, metadata and decision stay intact. */
        /* 设置副本全删时交原初始化重新创建；银行描述符、元数据和原决定均保持。 */
        zeroBytes(&record.turtleBefore,sizeof(record.turtleBefore));
        zeroBytes(&record.turtleCurrent,sizeof(record.turtleCurrent));
        zeroBytes(&record.turtleAfter,sizeof(record.turtleAfter));
        record.flags|=FS_TRANSACTION_TURTLE_BASELINE;
        return persist(io,&record,record.phase);
    }
    /* The published journal already authorizes this exact missing image. A
       partial recovery tmp can be rewritten only from that verified backup. */
    /* 正式日志已授权这一份缺失设置；恢复中断的暂存仅能从同摘要的备份重写。 */
    if (record.turtleCurrent.present && record.turtleCurrent.size==0x200) {
        if (!equalBytes(&temporary,&record.turtleCurrent,sizeof(temporary)) &&
            (!equalBytes(&backup,&record.turtleCurrent,sizeof(backup)) ||
             !FsTransaction_Copy(io,FS_TRANSACTION_TURTLE_BACKUP,
                FS_TRANSACTION_TURTLE_TEMP,&record,&record.turtleCurrent))) return 0;
        return missing(io,FS_TRANSACTION_TURTLE,&record) &&
            FsTransaction_Rename(io,FS_TRANSACTION_TURTLE_TEMP,FS_TRANSACTION_TURTLE,&record) &&
            turtleIs(io,&record);
    }
    return 0;
}

static int finishAdoption(const FsTransactionIo *io,FsTransactionRecord *record)
{
    /* Never retire the old backup until the adopted main/absence is checked.
       COMMIT_DECIDED makes this cleanup resumable after any publication gap. */
    /* 新主文件／明确缺失核对成功后才删除旧备份；已发布决定保证清理中断可续办。 */
    if (record->operation!=FS_TRANSACTION_ADOPT_BANK ||
        record->phase!=FS_TRANSACTION_COMMIT_DECIDED || !turtleIs(io,record)) return 0;
    if (!bankIs(io,record,1)) {
        /* Missing-history backup import: the published decision now permits
           rebuilding its temporary. Never overwrite a different primary. */
        /* 无历史备份接纳：正式决定发布后才可重建暂存，不覆盖其他正式正文。 */
        if (record->bankBefore.present || !record->bankAfter.present ||
            !missing(io,FS_TRANSACTION_BANK,record) ||
            (!FsTransaction_Matches(io,FS_TRANSACTION_TEMP,record,&record->bankAfter) &&
             !FsTransaction_Copy(io,FS_TRANSACTION_BACKUP,FS_TRANSACTION_TEMP,record,&record->bankAfter)) ||
            !publishFile(io,record,FS_TRANSACTION_BANK,FS_TRANSACTION_TEMP,
                FS_TRANSACTION_BACKUP,&record->bankBefore,&record->bankAfter)) return 0;
    }
    return physicalFileIs(io,FS_TRANSACTION_BANK,record,&record->bankAfter) &&
        FsTransaction_Remove(io,FS_TRANSACTION_TEMP,record) &&
        FsTransaction_Remove(io,FS_TRANSACTION_SOURCE_TEMP,record) &&
        FsTransaction_Remove(io,FS_TRANSACTION_TURTLE_TEMP,record) && noWorkingFiles(io,record) &&
        FsTransaction_Remove(io,FS_TRANSACTION_BACKUP,record) && missing(io,FS_TRANSACTION_BACKUP,record) &&
        persist(io,record,FS_TRANSACTION_COMPLETED);
}

static int adoptRecord(const FsTransactionIo *io,FsTransactionRecord *record,uint32_t owner,
    const FsTransactionFingerprint *bank,uint32_t bankFile)
{
    FsTransactionFingerprint settings;
    uint32_t sequence=record->sequence;
    uint64_t dataId=0;
    int settingsStatus=FsTransaction_Observe(io,FS_TRANSACTION_TURTLE,record,&settings);
    if (settingsStatus==FS_TRANSACTION_IO_ERROR || (settings.present && settings.size!=0x200))
        return 0;
    if (sequence==0xffffffffu || !io->randomBytes ||
        !io->randomBytes(&dataId,sizeof(dataId)) || !dataId ||
        dataId==record->serverTransaction.dataId) return 0;
    newRecord(record,owner,FS_TRANSACTION_ADOPT_BANK,0,FS_TRANSACTION_HAS_BANK,sequence);
    record->serverTransaction.dataId=dataId;
    if (bank) {
        if (bankFile==FS_TRANSACTION_BANK) copyBytes(&record->bankBefore,bank,sizeof(*bank));
        copyBytes(&record->bankAfter,bank,sizeof(*bank));
        record->serverTransaction.size=(uint32_t)bank->size;
    }
    record->flags|=FS_TRANSACTION_TURTLE_BASELINE;
    copyBytes(&record->turtleBefore,&settings,sizeof(settings));
    copyBytes(&record->turtleCurrent,&settings,sizeof(settings));
    copyBytes(&record->turtleAfter,&settings,sizeof(settings));
    return persist(io,record,FS_TRANSACTION_COMMIT_DECIDED) && finishAdoption(io,record);
}

static int restoreSettledBank(const FsTransactionIo *io,FsTransactionRecord *record)
{
    FsTransactionFingerprint actual,backup;
    FsTransactionFingerprint settings;
    int knownSettings=turtleBaseline(record);
    uint32_t owner=record->owner,sequence=record->sequence;
    copyBytes(&settings,&record->turtleCurrent,sizeof(settings));
    if (!turtleIs(io,record) || !noWorkingFiles(io,record) || sequence==0xffffffffu ||
        FsTransaction_Observe(io,FS_TRANSACTION_BANK,record,&actual)==FS_TRANSACTION_IO_ERROR ||
        equalBytes(&actual,&record->bankAfter,sizeof(actual)))
        return 0;
    int found=FsTransaction_Observe(io,FS_TRANSACTION_BACKUP,record,&backup);
    if (found==FS_TRANSACTION_MISSING && !actual.present) {
        /* A settled history is not a pending reservation. When both Bank
           copies and all participant temporaries are absent, publish an empty
           baseline instead of requiring the deleted committed image. A new
           identity keeps old source tokens distinct from the next Bank.
           Settings, game saves, gift history and configuration stay intact. */
        /* 已结束日志不是未决预留；两份银行及所有参与暂存均缺失时，发布空基线，
           不再要求已删除正文。新编号隔离旧来源令牌；设置、游戏存档、礼物记录
           和配置均不改写。该分支仅由已结束事务检查调用。 */
        return adoptRecord(io,record,owner,0,FS_TRANSACTION_BANK);
    }
    if (found!=FS_TRANSACTION_PRESENT) return 0;
    /* Only the journal's committed version is an automatic recovery source.
       An old or merely readable .bak never authorizes a new baseline. */
    /* 自动恢复仅接受日志确认的提交版本；旧版或仅可读的 .bak 不授权新基线。 */
    if (!equalBytes(&backup,&record->bankAfter,sizeof(backup))) return 0;
    FsTransactionParameters descriptor;
    FsSourceMetadata metadata;
    copyBytes(&descriptor,&record->serverTransaction,sizeof(descriptor));
    copyBytes(&metadata,&record->metadata,sizeof(metadata));
    newRecord(record,owner,FS_TRANSACTION_RESTORE_BACKUP,0,FS_TRANSACTION_HAS_BANK,sequence);
    copyBytes(&record->serverTransaction,&descriptor,sizeof(descriptor));
    /* Repairing the same Bank must retain its native last-source response. */
    /* 同一银行的正文恢复不能清空原生上次游戏／训练家响应信息。 */
    copyBytes(&record->metadata,&metadata,sizeof(metadata));
    if (knownSettings) {
        record->flags|=FS_TRANSACTION_TURTLE_BASELINE;
        copyBytes(&record->turtleBefore,&settings,sizeof(settings));
        copyBytes(&record->turtleCurrent,&settings,sizeof(settings));
        copyBytes(&record->turtleAfter,&settings,sizeof(settings));
    }
    copyBytes(&record->bankBefore,&actual,sizeof(actual));
    copyBytes(&record->bankAfter,&backup,sizeof(backup));
    /* Record the repair decision before touching the damaged main. An
       interrupted repair is resumed from the same verified backup, not aborted
       into treating the damaged bytes as a new trusted baseline. */
    /* 接触损坏主文件前记录恢复决定；中断后从同一已核对备份续办，不能撤销恢复后
       把损坏内容当作新可信基线。 */
    return persist(io,record,FS_TRANSACTION_COMMIT_DECIDED) && FsTransaction_Check(io);
}

static int checkBank(const FsTransactionIo *io,const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING) return noWorkingFiles(io,0);
    /* A download snapshot cannot authorize recovery that replaces its source. */
    /* 下载快照不能授权会替换其来源文件的恢复操作。 */
    if (bank && (status!=FS_TRANSACTION_PRESENT || !settled(&record))) return 0;
    if (status!=FS_TRANSACTION_PRESENT || !publishTurtle(io,&record) || !turtleIs(io,&record)) return 0;
    switch (record.phase) {
    case FS_TRANSACTION_PREPARING:
        return abortUnpublished(io,&record);
    case FS_TRANSACTION_SOURCE_PENDING:
    case FS_TRANSACTION_STAGED:
        /* Await the original client's matching/state decision. */
        /* 等待原版客户端完成匹配及状态分支判断。 */
        if (record.operation==FS_TRANSACTION_SOURCE_SAVE) {
            /* A validated SD save must be visible to the native loader before
               it decides whether to commit or roll back the Bank reservation. */
            /* 已验证的 SD 保存先向原版 loader 发布；银行提交／回滚仍由客户端决定。 */
            if (record.phase==FS_TRANSACTION_STAGED &&
                record.sourceKind==FS_TRANSACTION_NDS_FILE &&
                !publishFile(io,&record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
                    FS_TRANSACTION_SOURCE_BACKUP,&record.sourceBefore,&record.sourceAfter))
                return 0;
            return 0;
        }
        /* A source-only write is the SD backend of a native save, not a Bank
           reservation. Once validated, resume its publication without asking
           the client to commit an unrelated remote transaction. */
        /* 单独来源写入是原生保存的 SD 后端，不是银行预留；验证后自动续办发布，
           不要求客户端为它另行提交远端事务。 */
        if (record.operation==FS_TRANSACTION_SOURCE_ONLY &&
            record.phase==FS_TRANSACTION_STAGED &&
            (record.flags&FS_TRANSACTION_SOURCE_ACKNOWLEDGED))
            return FsTransaction_SourceSaved(io,record.owner) && FsTransaction_Check(io);
        return abortUnpublished(io,&record);
    case FS_TRANSACTION_COMMIT_DECIDED:
        if (record.operation==FS_TRANSACTION_ADOPT_BANK) return finishAdoption(io,&record);
        if (record.operation==FS_TRANSACTION_RESTORE_BACKUP &&
            !bankIs(io,&record,1) &&
            !FsTransaction_Matches(io,FS_TRANSACTION_TEMP,&record,&record.bankAfter) &&
            !FsTransaction_Copy(io,FS_TRANSACTION_BACKUP,FS_TRANSACTION_TEMP,
                &record,&record.bankAfter)) return 0;
        if (!publish(io,&record)) return 0;
        /* fall through */
    case FS_TRANSACTION_PUBLISHED:
        if (record.operation==FS_TRANSACTION_SOURCE_SAVE &&
            !(record.flags&FS_TRANSACTION_CLEANUP_ACKNOWLEDGED)) return 0;
        return FsTransaction_Finish(io,record.owner);
    case FS_TRANSACTION_CLEANUP_PENDING:
        return 0;
    case FS_TRANSACTION_CLEANUP_READY:
        return FsTransaction_Finish(io,record.owner);
    case FS_TRANSACTION_ROLLBACK_DECIDED:
        return rollbackRecord(io,&record,0);
    case FS_TRANSACTION_DETACHED_ROLLBACK_DECIDED:
        return rollbackRecord(io,&record,1);
    case FS_TRANSACTION_COMPLETED:
    case FS_TRANSACTION_ABORTED:
    case FS_TRANSACTION_DETACHED_ROLLED_BACK:
        return FsTransaction_GuardBank(io,bank) || (!bank && restoreSettledBank(io,&record));
    default:
        return 0;
    }
}

int FsTransaction_Check(const FsTransactionIo *io)
{
    return checkBank(io,0);
}

static int initialize(const FsTransactionIo *io,FsTransactionRecord *record,
    uint32_t owner,uint32_t operation,uint32_t mode,uint32_t participants,
    const FsTransactionFingerprint *bank)
{
    FsTransactionRecord previous;
    int status;
    uint32_t sequence=0;
    int local=operation==FS_TRANSACTION_SOURCE_ONLY || operation==FS_TRANSACTION_SETTINGS_ONLY;
    if (owner<FS_TRANSACTION_OWNER_BANK || owner>FS_TRANSACTION_OWNER_MOVER || mode>3) return 0;
    if (!local && !checkBank(io,bank)) return 0;
    status=FsTransaction_ReadRecord(io,&previous);
    if (status==FS_TRANSACTION_PRESENT) {
        if (bank && (!settled(&previous) ||
            !equalBytes(bank,&previous.bankAfter,sizeof(*bank)))) return 0;
        sequence=previous.sequence;
    }
    else if (status!=FS_TRANSACTION_MISSING) return 0;
    /* Local saves may not discard an active server reservation. Checking its
       record and local working files does not require reading Bank/config. */
    /* 本地保存不能丢弃在途服务预留；核对记录与本地工作文件不读取银行／配置。 */
    if (local && ((status==FS_TRANSACTION_PRESENT && !settled(&previous)) ||
        !noWorkingFiles(io,status==FS_TRANSACTION_PRESENT?&previous:0) ||
        !FsTransaction_TurtleGuard(io))) return 0;
    if (sequence==0xffffffffu) return 0;
    newRecord(record,owner,operation,mode,participants,sequence);
    if (status==FS_TRANSACTION_PRESENT) {
        copyBytes(&record->serverTransaction,&previous.serverTransaction,
            sizeof(record->serverTransaction));
        /* A settings/source-only operation does not change the Bank's last
           game display metadata; staging may replace it with its own input. */
        /* 设置／单独来源保存不清掉银行的游戏显示元数据；暂存可用本次输入覆盖。 */
        copyBytes(&record->metadata,&previous.metadata,sizeof(record->metadata));
        if (turtleBaseline(&previous)) {
            record->flags|=FS_TRANSACTION_TURTLE_BASELINE;
            copyBytes(&record->turtleBefore,&previous.turtleCurrent,sizeof(record->turtleBefore));
            copyBytes(&record->turtleCurrent,&previous.turtleCurrent,sizeof(record->turtleCurrent));
            copyBytes(&record->turtleAfter,&previous.turtleCurrent,sizeof(record->turtleAfter));
        }
    }
    if (!record->serverTransaction.dataId &&
        (!io->randomBytes || !io->randomBytes(&record->serverTransaction.dataId,sizeof(uint64_t)) ||
         !record->serverTransaction.dataId)) return 0;
    if (local && status==FS_TRANSACTION_PRESENT)
        copyBytes(&record->bankBefore,&previous.bankAfter,sizeof(record->bankBefore));
    else if (local) zeroBytes(&record->bankBefore,sizeof(record->bankBefore));
    else if (bank) copyBytes(&record->bankBefore,bank,sizeof(*bank));
    else if (FsTransaction_Observe(io,FS_TRANSACTION_BANK,record,&record->bankBefore)==
        FS_TRANSACTION_IO_ERROR) return 0;
    copyBytes(&record->bankAfter,&record->bankBefore,sizeof(record->bankAfter));
    return 1;
}

static int ensureIdentity(const FsTransactionIo *io,uint32_t owner,
    const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_PRESENT) {
        /* A settings-only bootstrap can precede the first Bank query. */
        /* 共享设置的首次保存可能先于首次银行查询。 */
        if (settled(&record) && record.bankAfter.present &&
            !record.serverTransaction.size) {
            if (record.bankAfter.size>0xffffffffu || !FsTransaction_GuardBank(io,bank)) return 0;
            record.serverTransaction.size=(uint32_t)record.bankAfter.size;
            return persist(io,&record,record.phase);
        }
        return 1;
    }
    if (status!=FS_TRANSACTION_MISSING ||
        !initialize(io,&record,owner,FS_TRANSACTION_BANK_ONLY,0,FS_TRANSACTION_HAS_BANK,bank) ||
        !record.bankBefore.present) return 0;
    record.serverTransaction.size=(uint32_t)record.bankBefore.size;
    return persist(io,&record,FS_TRANSACTION_COMPLETED);
}

int FsTransaction_EnsureIdentity(const FsTransactionIo *io,uint32_t owner)
{
    return ensureIdentity(io,owner,0);
}

int FsTransaction_EnsureIdentitySnapshot(const FsTransactionIo *io,uint32_t owner,
    const FsTransactionFingerprint *bank)
{
    return bank && bank->present && validFingerprint(bank) && ensureIdentity(io,owner,bank);
}

int FsTransaction_AdoptBankSnapshot(const FsTransactionIo *io,uint32_t owner,
    const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    if ((bank && (!bank->present || !validFingerprint(bank) || bank->size>0xffffffffu)) ||
        owner<FS_TRANSACTION_OWNER_BANK || owner>FS_TRANSACTION_OWNER_MOVER) return 0;
    int status=FsTransaction_ReadRecord(io,&record);
    if (status==FS_TRANSACTION_MISSING) {
        zeroBytes(&record,sizeof(record));
    } else if (status!=FS_TRANSACTION_PRESENT) return 0;
    if (!bank) {
        if (!missing(io,FS_TRANSACTION_BANK,&record) ||
            (status==FS_TRANSACTION_PRESENT && !missing(io,FS_TRANSACTION_TEMP,&record)) ||
            !missing(io,FS_TRANSACTION_BACKUP,&record)) return 0;
    } else if (status==FS_TRANSACTION_PRESENT && !settled(&record) &&
        (equalBytes(bank,&record.bankBefore,sizeof(*bank)) ||
         equalBytes(bank,&record.bankAfter,sizeof(*bank)))) {
        /* A normal pending publication can expose either recorded image.
           This is recovery, not evidence of an imported Bank. */
        /* 正常未决发布允许出现日志中的前／后正文；这是续办，不是导入证据。 */
        return 0;
    }
    return adoptRecord(io,&record,owner,bank,FS_TRANSACTION_BANK);
}

int FsTransaction_AdoptBankBackupSnapshot(const FsTransactionIo *io,uint32_t owner,
    const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    if (!bank || !bank->present || !validFingerprint(bank) || bank->size>0xffffffffu ||
        owner<FS_TRANSACTION_OWNER_BANK || owner>FS_TRANSACTION_OWNER_MOVER ||
        FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_MISSING ||
        !missing(io,FS_TRANSACTION_BANK,0)) return 0;
    zeroBytes(&record,sizeof(record));
    return adoptRecord(io,&record,owner,bank,FS_TRANSACTION_BACKUP);
}

int FsTransaction_ResetOrphanBank(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_MISSING ||
        !missing(io,FS_TRANSACTION_BANK,0) || !missing(io,FS_TRANSACTION_BACKUP,0)) return 0;
    if (missing(io,FS_TRANSACTION_TEMP,0) && missing(io,FS_TRANSACTION_JOURNAL_TEMP,0)) return 1;
    return FsTransaction_AdoptBankSnapshot(io,owner,0);
}

int FsTransaction_UpdateMetadata(const FsTransactionIo *io,uint64_t dataId,
    const FsSourceMetadata *metadata)
{
    FsTransactionRecord record;
    if (!dataId || !metadata || metadata->formatVersion!=2 || !FsTransaction_Guard(io) ||
        FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT ||
        record.serverTransaction.dataId!=dataId || !settled(&record)) return 0;
    if (equalBytes(&record.metadata,metadata,sizeof(*metadata))) return 1;
    copyBytes(&record.metadata,metadata,sizeof(*metadata));
    return persist(io,&record,record.phase);
}

static int beginDescriptor(const FsTransactionIo *io,FsTransactionRecord *record,uint32_t size)
{
    FsTransactionParameters *p=&record->serverTransaction;
    uint64_t previous=p->token;
    if (p->currentVersion==0xffffffffu || !io->randomBytes) return 0;
    for (uint32_t attempt=0;attempt<4;attempt++) {
        if (!io->randomBytes(&p->token,sizeof(p->token))) return 0;
        if (p->token && p->token!=previous) {
            p->updateVersion=p->currentVersion+1;
            p->size=size;
            return 1;
        }
    }
    return 0;
}

static int availableRoom(const FsTransactionIo *io,uint64_t required)
{
    uint64_t available=0;
    if (!io->freeBytes || !io->freeBytes(&available)) {
        FsStorageNotice_Mark(FS_STORAGE_ISSUE_CAPACITY);return 0;
    }
    if (available<required) {
        FsStorageNotice_Mark(FS_STORAGE_ISSUE_FULL);return 0;
    }
    return 1;
}

static int room(const FsTransactionIo *io,const FsTransactionRecord *record,uint64_t extra)
{
    uint64_t available=0;
    if (record->participants&FS_TRANSACTION_HAS_BANK) {
        extra+=record->bankBefore.size+record->bankAfter.size;
    }
    if (record->sourceKind==FS_TRANSACTION_NDS_FILE) extra+=record->sourceBefore.size*2;
    if (record->participants&FS_TRANSACTION_HAS_TURTLE) extra+=record->turtleBefore.size;
    extra+=sizeof(*record);
    int queried=io->freeBytes && io->freeBytes(&available);
    FsDiag_Record("space.required",0,(uint32_t)extra,(uint32_t)(extra>>32),record->operation);
    FsDiag_Record("space.available",queried?0:-1,(uint32_t)available,(uint32_t)(available>>32),
        available>=extra);
    if (!queried) FsStorageNotice_Mark(FS_STORAGE_ISSUE_CAPACITY);
    else if (available<extra) FsStorageNotice_Mark(FS_STORAGE_ISSUE_FULL);
    return queried && available>=extra;
}

static int prepare(const FsTransactionIo *io,FsTransactionRecord *record,const void *bank,uint32_t size,
    const FsTransactionFingerprint *snapshot)
{
    int reference=snapshot && record->owner==FS_TRANSACTION_OWNER_BANK &&
        record->operation==FS_TRANSACTION_SOURCE_SAVE &&
        record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE && record->bankBefore.present &&
        equalBytes(&record->bankBefore,&record->bankAfter,sizeof(record->bankBefore));
    if (!room(io,record,0) || !persist(io,record,FS_TRANSACTION_PREPARING)) return 0;
    if (record->participants&FS_TRANSACTION_HAS_BANK) {
        if (reference) {
            /* No write is admitted yet. Keep the previous generation untouched;
               actual upload creates the rollback snapshot before staging. */
            /* 尚未允许写入，保留原备份；实际上传才在暂存新正文前建立回滚快照。 */
            if (!FsTransaction_Matches(io,FS_TRANSACTION_BANK,record,&record->bankBefore)) return 0;
        } else if (!FsTransaction_Copy(io,FS_TRANSACTION_BANK,FS_TRANSACTION_BACKUP,
            record,&record->bankBefore)) return 0;
    }
    /* A reference reservation has no new Bank body to stage. The actual save
       still writes and verifies its complete tmp before the native game save. */
    /* 引用预留没有新的银行正文，实际保存仍在原游戏保存前写齐并校验完整 tmp。 */
    if (bank && !reference && (!writeFile(io,FS_TRANSACTION_TEMP,record,bank,size) ||
        !FsTransaction_Matches(io,FS_TRANSACTION_TEMP,record,&record->bankAfter))) return 0;
    if (record->sourceKind==FS_TRANSACTION_NDS_FILE) {
        FsTransactionFingerprint backup;
        int status=FsTransaction_Observe(io,FS_TRANSACTION_SOURCE_BACKUP,record,&backup);
        if (status==FS_TRANSACTION_IO_ERROR ||
            (!equalBytes(&backup,&record->sourceBefore,sizeof(backup)) &&
             !FsTransaction_Copy(io,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_BACKUP,
                 record,&record->sourceBefore)) ||
            (!io->deferSourceImage &&
             !FsTransaction_Copy(io,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
                record,&record->sourceBefore))) return 0;
    }
    return persist(io,record,(record->participants&FS_TRANSACTION_HAS_SOURCE)?
        FS_TRANSACTION_SOURCE_PENDING:FS_TRANSACTION_STAGED);
}

static int stageBank(const FsTransactionIo *io,uint32_t owner,uint32_t operation,
    const void *data,uint32_t size,const FsSourceMetadata *metadata,
    const FsTransactionFingerprint *bank)
{
    FsTransactionRecord record;
    uint32_t participants=FS_TRANSACTION_HAS_BANK;
    if (!data || !size || operation<FS_TRANSACTION_SOURCE_SAVE ||
        operation>FS_TRANSACTION_RESTORE_BACKUP) return 0;
    if (operation==FS_TRANSACTION_SOURCE_SAVE) participants|=FS_TRANSACTION_HAS_SOURCE;
    if (!initialize(io,&record,owner,operation,0,participants,bank)) return 0;
    if (metadata) copyBytes(&record.metadata,metadata,sizeof(record.metadata));
    if (participants&FS_TRANSACTION_HAS_SOURCE) record.sourceKind=FS_TRANSACTION_NATIVE_SOURCE;
    if (operation!=FS_TRANSACTION_RESTORE_BACKUP &&
        !beginDescriptor(io,&record,size)) return 0;
    record.bankAfter.present=1;
    record.bankAfter.size=size;
    if (bank) copyBytes(record.bankAfter.hash,bank->hash,sizeof(bank->hash));
    else FsTransaction_Hash(data,size,record.bankAfter.hash);
    /* RESTORE must not overwrite the only valid .bak with an invalid primary. */
    /* 从备份恢复时不能用无效主文件覆盖唯一有效的 .bak。 */
    if (operation==FS_TRANSACTION_RESTORE_BACKUP) {
        if (!FsTransaction_Matches(io,FS_TRANSACTION_BACKUP,&record,&record.bankAfter) ||
            !room(io,&record,0) || !persist(io,&record,FS_TRANSACTION_PREPARING) ||
            !writeFile(io,FS_TRANSACTION_TEMP,&record,data,size) ||
            !FsTransaction_Matches(io,FS_TRANSACTION_TEMP,&record,&record.bankAfter)) return 0;
        return persist(io,&record,FS_TRANSACTION_STAGED);
    }
    return prepare(io,&record,data,size,bank);
}

int FsTransaction_Stage(const FsTransactionIo *io,uint32_t owner,uint32_t operation,
    const void *data,uint32_t size,const FsSourceMetadata *metadata)
{
    return stageBank(io,owner,operation,data,size,metadata,0);
}

int FsTransaction_StageSnapshot(const FsTransactionIo *io,uint32_t owner,const void *data,
    uint32_t size,const FsSourceMetadata *metadata,const FsTransactionFingerprint *bank)
{
    if (!bank || !bank->present || !validFingerprint(bank) || bank->size!=size ||
        owner!=FS_TRANSACTION_OWNER_BANK) return 0;
    return stageBank(io,owner,FS_TRANSACTION_SOURCE_SAVE,data,size,metadata,bank);
}

static int stagedBankIs(const FsTransactionIo *io,const FsTransactionRecord *record)
{
    FsTransactionFingerprint actual;
    int status=FsTransaction_Observe(io,FS_TRANSACTION_TEMP,record,&actual);
    if (status==FS_TRANSACTION_PRESENT)
        return equalBytes(&actual,&record->bankAfter,sizeof(actual));
    /* Only an unchanged native Bank reservation can use the verified primary
       instead of a missing tmp. Callers have already checked bankBefore in
       this invocation. An existing damaged/unreadable tmp is never ignored. */
    /* 仅未改正文的 Bank 原生预留可在 tmp 缺失时引用本次已核对的主文件；
       调用方先检查 bankBefore。已有但损坏或不可读的 tmp 绝不忽略。 */
    return status==FS_TRANSACTION_MISSING && record->owner==FS_TRANSACTION_OWNER_BANK &&
        record->operation==FS_TRANSACTION_SOURCE_SAVE &&
        record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE && record->bankBefore.present &&
        equalBytes(&record->bankBefore,&record->bankAfter,sizeof(record->bankBefore));
}

int FsTransaction_UpdateBankReservation(const FsTransactionIo *io,uint32_t owner,uint64_t token,
    const void *data,uint32_t size,const FsSourceMetadata *metadata)
{
    FsTransactionRecord record;
    if (!data || !size || !token || owner!=FS_TRANSACTION_OWNER_BANK ||
        FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        record.operation!=FS_TRANSACTION_SOURCE_SAVE || record.sourceKind!=FS_TRANSACTION_NATIVE_SOURCE ||
        record.phase!=FS_TRANSACTION_SOURCE_PENDING || record.serverTransaction.token!=token ||
        record.serverTransaction.size!=size ||
        !turtleIs(io,&record) ||
        (record.flags&(FS_TRANSACTION_SOURCE_ACKNOWLEDGED|FS_TRANSACTION_TURTLE_WRITE_PENDING)) ||
        !bankIs(io,&record,0) || !stagedBankIs(io,&record)) return 0;
    /* Download's state-1 reservation stays valid until the stock save changes
       the source to state 2. A restart has no live-session permission to restage. */
    /* 下载的状态 1 预留持续到原版保存把来源改为状态 2；重启不保留会话内再次暂存权限。 */
    record.bankAfter.present=1;
    record.bankAfter.size=size;
    FsTransaction_Hash(data,size,record.bankAfter.hash);
    if (!equalBytes(&record.bankBefore,&record.bankAfter,sizeof(record.bankBefore)) &&
        (!FsTransaction_Matches(io,FS_TRANSACTION_BACKUP,&record,&record.bankBefore) &&
         !FsTransaction_Copy(io,FS_TRANSACTION_BANK,FS_TRANSACTION_BACKUP,
             &record,&record.bankBefore))) return 0;
    if (metadata) copyBytes(&record.metadata,metadata,sizeof(record.metadata));
    return writeFile(io,FS_TRANSACTION_TEMP,&record,data,size) &&
        FsTransaction_Matches(io,FS_TRANSACTION_TEMP,&record,&record.bankAfter) &&
        persist(io,&record,FS_TRANSACTION_SOURCE_PENDING);
}

int FsTransaction_StageSource(const FsTransactionIo *io,uint32_t owner,uint32_t mode,
    const uint16_t *path,uint32_t pathSize,uint32_t gameCode,const void *bank,uint32_t size,
    const FsTransactionFingerprint *expected,const FsSourceMetadata *metadata)
{
    FsTransactionRecord record;
    uint32_t participants=FS_TRANSACTION_HAS_SOURCE;
    if (!path || pathSize<4 || (pathSize&1) || pathSize>sizeof(record.sourcePath) ||
        path[0]!='/' || path[pathSize/2-1] || (!bank && size) || (bank && !size)) return 0;
    if (bank) participants|=FS_TRANSACTION_HAS_BANK;
    if (!initialize(io,&record,owner,bank?FS_TRANSACTION_SOURCE_SAVE:
        FS_TRANSACTION_SOURCE_ONLY,mode,participants,0)) return 0;
    record.sourceKind=FS_TRANSACTION_NDS_FILE;
    if (metadata) copyBytes(&record.metadata,metadata,sizeof(record.metadata));
    record.sourceGameCode=gameCode;
    record.sourcePathSize=pathSize;
    copyBytes(record.sourcePath,path,pathSize);
    if (!FsTransaction_SourcePath(&record,FS_TRANSACTION_SOURCE,0)) return 0;
    if (FsTransaction_Observe(io,FS_TRANSACTION_SOURCE,&record,&record.sourceBefore)!=
        FS_TRANSACTION_PRESENT || record.sourceBefore.size!=0x80000) return 0;
    /* Compare the captured before-image before persisting any preparation. */
    /* 暂存前核对实际捕获的旧版本，不在另一次观察后才决定版本。 */
    if (expected && (record.sourceBefore.present!=expected->present ||
        record.sourceBefore.size!=expected->size ||
        !equalBytes(record.sourceBefore.hash,expected->hash,sizeof(expected->hash)))) return 0;
    if (bank) {
        if (!beginDescriptor(io,&record,size)) return 0;
        record.bankAfter.present=1;
        record.bankAfter.size=size;
        FsTransaction_Hash(bank,size,record.bankAfter.hash);
    }
    return prepare(io,&record,bank,size,0);
}

int FsTransaction_SourceSaved(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT ||
        record.owner!=owner ||
        (record.phase!=FS_TRANSACTION_SOURCE_PENDING && record.phase!=FS_TRANSACTION_STAGED))
        return 0;
    if (record.phase==FS_TRANSACTION_SOURCE_PENDING) {
        if (record.sourceKind==FS_TRANSACTION_NDS_FILE &&
            (FsTransaction_Observe(io,FS_TRANSACTION_SOURCE_TEMP,&record,&record.sourceAfter)!=
                FS_TRANSACTION_PRESENT || record.sourceAfter.size!=record.sourceBefore.size ||
             !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceBefore))) return 0;
        record.flags|=FS_TRANSACTION_SOURCE_ACKNOWLEDGED;
        if (!persist(io,&record,FS_TRANSACTION_STAGED)) return 0;
    }
    if (record.operation==FS_TRANSACTION_SOURCE_ONLY) {
        if (record.sourceKind!=FS_TRANSACTION_NDS_FILE ||
            !(record.flags&FS_TRANSACTION_SOURCE_ACKNOWLEDGED) ||
            !record.sourceAfter.present || !bankIs(io,&record,0) ||
            (!FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceBefore) &&
             !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceAfter)) ||
            (!FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceAfter) &&
             !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE_TEMP,&record,&record.sourceAfter)) ||
            !persist(io,&record,FS_TRANSACTION_COMMIT_DECIDED)) return 0;
        /* Persist the single-file decision before publishing the source.
           A restart then resumes publication, even if rename already worked. */
        /* 单文件决定先于来源发布持久化；即使改名已生效，重启仍从该决定续办。 */
        return publish(io,&record);
    }
    /* Native SavePoll success acknowledges the source backend's save. Mirror
       that order for SD, without publishing Bank or claiming power-loss safety. */
    /* 原版 SavePoll 成功是来源后端的保存确认；SD 来源也先发布游戏，不发布银行，
       不以此声称突然断电持久性。 */
    return record.sourceKind!=FS_TRANSACTION_NDS_FILE ||
        publishFile(io,&record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
            FS_TRANSACTION_SOURCE_BACKUP,&record.sourceBefore,&record.sourceAfter);
}

int FsTransaction_Commit(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        !publishTurtle(io,&record) || !turtleIs(io,&record)) return 0;
    if (record.phase==FS_TRANSACTION_PUBLISHED) {
        if (!bankIs(io,&record,1)) return 0;
        return 1;
    }
    if (record.phase==FS_TRANSACTION_STAGED) {
        if ((record.participants&FS_TRANSACTION_HAS_SOURCE) &&
            !(record.flags&FS_TRANSACTION_SOURCE_ACKNOWLEDGED)) return 0;
        if (!bankIs(io,&record,0) ||
            ((record.participants&FS_TRANSACTION_HAS_BANK) &&
             !stagedBankIs(io,&record)) ||
            (record.sourceKind==FS_TRANSACTION_NDS_FILE &&
             (!record.sourceAfter.present ||
              !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceAfter))) ||
            !persist(io,&record,FS_TRANSACTION_COMMIT_DECIDED)) return 0;
    }
    if (record.phase!=FS_TRANSACTION_COMMIT_DECIDED || !publish(io,&record)) return 0;
    /* Publication does not prove that the client's final token-cleanup save succeeded. */
    /* 发布银行不证明客户端最后一次清令牌保存成功；两种来源都等待实际确认。 */
    return 1;
}

int FsTransaction_SourceRestored(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        record.operation!=FS_TRANSACTION_SOURCE_SAVE ||
        record.sourceKind!=FS_TRANSACTION_NATIVE_SOURCE ||
        (record.phase!=FS_TRANSACTION_SOURCE_PENDING && record.phase!=FS_TRANSACTION_STAGED)) return 0;
    if (record.flags&FS_TRANSACTION_SOURCE_RESTORED) return 1;
    record.flags|=FS_TRANSACTION_SOURCE_RESTORED;
    return persist(io,&record,record.phase);
}

int FsTransaction_BeginCleanup(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        record.operation!=FS_TRANSACTION_SOURCE_SAVE ||
        (record.phase!=FS_TRANSACTION_PUBLISHED && record.phase!=FS_TRANSACTION_CLEANUP_PENDING) ||
        !bankIs(io,&record,1)) return 0;
    if (record.sourceKind==FS_TRANSACTION_NDS_FILE) {
        if (!FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceAfter) ||
            !availableRoom(io,record.sourceAfter.size+sizeof(record)) ||
            !persist(io,&record,FS_TRANSACTION_CLEANUP_PENDING) ||
            (!io->deferSourceImage &&
             !FsTransaction_Copy(io,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_TEMP,
                &record,&record.sourceAfter))) return 0;
    } else if (!persist(io,&record,FS_TRANSACTION_CLEANUP_PENDING)) return 0;
    return 1;
}

int FsTransaction_CleanupSaved(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        record.phase!=FS_TRANSACTION_CLEANUP_PENDING || !bankIs(io,&record,1)) return 0;
    if (record.sourceKind==FS_TRANSACTION_NDS_FILE &&
        (FsTransaction_Observe(io,FS_TRANSACTION_SOURCE_TEMP,&record,&record.sourceClean)!=
            FS_TRANSACTION_PRESENT || record.sourceClean.size!=record.sourceBefore.size ||
         !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,&record,&record.sourceAfter))) return 0;
    record.flags|=FS_TRANSACTION_CLEANUP_ACKNOWLEDGED;
    return persist(io,&record,FS_TRANSACTION_CLEANUP_READY);
}

int FsTransaction_Rollback(const FsTransactionIo *io,uint32_t owner)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner)
        return 0;
    if (record.phase==FS_TRANSACTION_ABORTED) return FsTransaction_Guard(io);
    if (
        (record.phase!=FS_TRANSACTION_PREPARING && record.phase!=FS_TRANSACTION_SOURCE_PENDING &&
         record.phase!=FS_TRANSACTION_STAGED && record.phase!=FS_TRANSACTION_ROLLBACK_DECIDED)) return 0;
    /* The stock client selected rollback; the server does not rewrite a native save. */
    /* 原版客户端已选择回滚；服务端不重写原生游戏存档。 */
    if (record.sourceKind==FS_TRANSACTION_NATIVE_SOURCE) {
        record.sourceKind=FS_TRANSACTION_NO_SOURCE;
        record.participants&=~FS_TRANSACTION_HAS_SOURCE;
    }
    record.serverTransaction.updateVersion=record.serverTransaction.currentVersion;
    return persist(io,&record,FS_TRANSACTION_ROLLBACK_DECIDED) && rollbackRecord(io,&record,0);
}

int FsTransaction_Request(const FsTransactionIo *io,uint32_t owner,
    const FsTransactionParameters *request,int commit,int force)
{
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(io,&record);
    if (!request || status==FS_TRANSACTION_IO_ERROR) return 0;
    if (status==FS_TRANSACTION_MISSING) return 0;
    const FsTransactionParameters *saved=&record.serverTransaction;
    if (settled(&record)) {
        /* A retry may acknowledge our last completed reservation, not a token
           from a different server or a missing transaction history. */
        /* 可确认本服务器最后一笔已结束预留的重试，不能确认其他服务器或未知历史的令牌。 */
        if (request->dataId!=saved->dataId || !request->token || request->token!=saved->token ||
            request->size!=saved->size || request->currentVersion==0xffffffffu ||
            request->updateVersion!=request->currentVersion+1) return 0;
        uint32_t version=record.phase==FS_TRANSACTION_COMPLETED?
            request->updateVersion:request->currentVersion;
        return version==saved->currentVersion && FsTransaction_Guard(io);
    }
    if ((!force && record.sourceKind!=FS_TRANSACTION_NATIVE_SOURCE && record.owner!=owner) || (!force &&
        (request->dataId!=saved->dataId || request->currentVersion!=saved->currentVersion ||
         request->updateVersion!=saved->updateVersion || request->size!=saved->size ||
         !request->token || request->token!=saved->token))) return 0;
    /* The producing application is a file-backend tag, not an extra native
       source identity. Native authorization uses the original descriptor. */
    /* 产生事务的软件仅为文件后端标记，不增加原生来源身份条件；原生请求按原描述符授权。 */
    owner=record.owner;
    if (!commit) return force?FsTransaction_ForceRollback(io,owner,1):
        FsTransaction_Rollback(io,owner);
    if (record.phase==FS_TRANSACTION_SOURCE_PENDING && !FsTransaction_SourceSaved(io,owner)) return 0;
    if (record.phase==FS_TRANSACTION_CLEANUP_PENDING || record.phase==FS_TRANSACTION_CLEANUP_READY)
        return bankIs(io,&record,1);
    return FsTransaction_Commit(io,owner);
}

static int canRestore(const FsTransactionIo *io,const FsTransactionRecord *record,
    uint32_t main,uint32_t backup,const FsTransactionFingerprint *before,
    const FsTransactionFingerprint *after)
{
    if (FsTransaction_Matches(io,main,record,before)) return 1;
    return (FsTransaction_Matches(io,main,record,after) || missing(io,main,record)) &&
        (!before->present || FsTransaction_Matches(io,backup,record,before));
}

int FsTransaction_CanRollback(const FsTransactionIo *io,const FsTransactionRecord *record,
    int withoutSource)
{
    if (settled(record)) return 0;
    if (!withoutSource && record->sourceKind==FS_TRANSACTION_NATIVE_SOURCE) return 0;
    if (!withoutSource && record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        !canRestore(io,record,FS_TRANSACTION_SOURCE,FS_TRANSACTION_SOURCE_BACKUP,
             &record->sourceBefore,record->sourceClean.present &&
                 !FsTransaction_Matches(io,FS_TRANSACTION_SOURCE,record,&record->sourceAfter)?
                 &record->sourceClean:&record->sourceAfter)) return 0;
    if ((record->participants&FS_TRANSACTION_HAS_BANK) && record->bankAfter.present &&
        !FsTransaction_Matches(io,FS_TRANSACTION_BANK,record,&record->bankAfter) &&
        !FsTransaction_Matches(io,FS_TRANSACTION_TEMP,record,&record->bankAfter)) return 0;
    if (!canRestore(io,record,FS_TRANSACTION_BANK,FS_TRANSACTION_BACKUP,
            &record->bankBefore,&record->bankAfter)) return 0;
    if ((record->participants&FS_TRANSACTION_HAS_TURTLE) &&
        !canRestore(io,record,FS_TRANSACTION_TURTLE,FS_TRANSACTION_TURTLE_BACKUP,
            &record->turtleBefore,&record->turtleCurrent)) return 0;
    return 1;
}

int FsTransaction_ForceRollback(const FsTransactionIo *io,uint32_t owner,int withoutSource)
{
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT || record.owner!=owner ||
        !FsTransaction_CanRollback(io,&record,withoutSource)) return 0;
    record.flags|=FS_TRANSACTION_MANUAL_ROLLBACK;
    if (!persist(io,&record,withoutSource?FS_TRANSACTION_DETACHED_ROLLBACK_DECIDED:
        FS_TRANSACTION_ROLLBACK_DECIDED)) return 0;
    return rollbackRecord(io,&record,withoutSource);
}

int FsTransaction_Write(const FsTransactionIo *io,uint32_t owner,uint32_t operation,
    const void *data,uint32_t size)
{
    if (operation==FS_TRANSACTION_SOURCE_SAVE) return 0;
    return FsTransaction_Stage(io,owner,operation,data,size,0) &&
        FsTransaction_Commit(io,owner) && FsTransaction_Finish(io,owner);
}

int FsTransaction_WriteTurtle(const FsTransactionIo *io,uint32_t owner,
    const void *data,uint32_t size,int activeSave)
{
    FsTransactionRecord record;
    FsTransactionFingerprint next;
    int status=FsTransaction_ReadRecord(io,&record),standalone;
    if ((size && (!data || size!=0x200)) || owner<1 || owner>2) return 0;
    if (status==FS_TRANSACTION_IO_ERROR || !FsTransaction_TurtleGuard(io)) return 0;
    standalone=status==FS_TRANSACTION_MISSING || settled(&record);
    if (standalone) {
        if (!initialize(io,&record,owner,FS_TRANSACTION_SETTINGS_ONLY,0,FS_TRANSACTION_HAS_TURTLE,0) &&
            (status!=FS_TRANSACTION_MISSING || !FsTransaction_ResetOrphanBank(io,owner) ||
             !initialize(io,&record,owner,FS_TRANSACTION_SETTINGS_ONLY,0,FS_TRANSACTION_HAS_TURTLE,0))) return 0;
        if (FsTransaction_Observe(io,FS_TRANSACTION_TURTLE,&record,&next)==FS_TRANSACTION_IO_ERROR ||
            ((record.flags&FS_TRANSACTION_TURTLE_BASELINE) &&
             !equalBytes(&next,&record.turtleCurrent,sizeof(next)))) return 0;
        copyBytes(&record.turtleBefore,&next,sizeof(next));
        copyBytes(&record.turtleCurrent,&record.turtleBefore,sizeof(record.turtleCurrent));
        copyBytes(&record.turtleAfter,&record.turtleBefore,sizeof(record.turtleAfter));
        record.flags|=FS_TRANSACTION_TURTLE_BASELINE;
        if (!room(io,&record,size) || !persist(io,&record,FS_TRANSACTION_PREPARING)) return 0;
    } else if (!activeSave || record.owner!=owner ||
        record.operation!=FS_TRANSACTION_SOURCE_SAVE || !publishTurtle(io,&record)) return 0;
    if (!(record.participants&FS_TRANSACTION_HAS_TURTLE)) {
        if (FsTransaction_Observe(io,FS_TRANSACTION_TURTLE,&record,&next)==FS_TRANSACTION_IO_ERROR ||
            ((record.flags&FS_TRANSACTION_TURTLE_BASELINE) &&
             !equalBytes(&next,&record.turtleCurrent,sizeof(next)))) return 0;
        copyBytes(&record.turtleBefore,&next,sizeof(next));
        copyBytes(&record.turtleCurrent,&record.turtleBefore,sizeof(record.turtleCurrent));
        copyBytes(&record.turtleAfter,&record.turtleBefore,sizeof(record.turtleAfter));
        record.participants|=FS_TRANSACTION_HAS_TURTLE;
        record.flags|=FS_TRANSACTION_TURTLE_BASELINE;
        if (!room(io,&record,size) || !persist(io,&record,record.phase)) return 0;
    }
    if (!FsTransaction_Matches(io,FS_TRANSACTION_TURTLE,&record,&record.turtleCurrent) ||
        (!FsTransaction_Matches(io,FS_TRANSACTION_TURTLE_BACKUP,&record,&record.turtleBefore) &&
         !FsTransaction_Copy(io,FS_TRANSACTION_TURTLE,FS_TRANSACTION_TURTLE_BACKUP,
            &record,&record.turtleBefore)) || !missing(io,FS_TRANSACTION_TURTLE_TEMP,&record)) return 0;
    zeroBytes(&next,sizeof(next));
    if (size) {
        next.present=1;
        next.size=size;
        FsTransaction_Hash(data,size,next.hash);
        if (!writeFile(io,FS_TRANSACTION_TURTLE_TEMP,&record,data,size) ||
            !FsTransaction_Matches(io,FS_TRANSACTION_TURTLE_TEMP,&record,&next)) return 0;
    }
    copyBytes(&record.turtleAfter,&next,sizeof(record.turtleAfter));
    record.flags|=FS_TRANSACTION_TURTLE_WRITE_PENDING;
    if (!persist(io,&record,record.phase) || !publishTurtle(io,&record)) return 0;
    if (!standalone) return 1;
    return persist(io,&record,FS_TRANSACTION_PUBLISHED) && FsTransaction_Finish(io,owner);
}

int FsTransaction_SourceIsWorking(const FsTransactionRecord *record)
{
    return record->sourceKind==FS_TRANSACTION_NDS_FILE &&
        (record->phase==FS_TRANSACTION_SOURCE_PENDING ||
         record->phase==FS_TRANSACTION_CLEANUP_PENDING);
}
