#include "fakeServer/transaction.h"

int FsTransaction_SourcePath(const FsTransactionRecord *record,uint32_t file,uint16_t *out)
{
    if (!record || record->sourceKind!=FS_TRANSACTION_NDS_FILE ||
        file<FS_TRANSACTION_SOURCE || file>FS_TRANSACTION_SOURCE_BACKUP ||
        record->sourcePathSize<14 || (record->sourcePathSize&1) ||
        record->sourcePathSize>sizeof(record->sourcePath)) return 0;
    const uint16_t *path=record->sourcePath;
    uint32_t length=record->sourcePathSize/2-1,part=1;
    if (path[0]!='/' || path[length]) return 0;
    for (uint32_t i=1;i<=length;i++) {
        uint16_t c=path[i];
        if (i==length || c=='/') {
            uint32_t size=i-part;
            if (!size || (size==1 && path[part]=='.') ||
                (size==2 && path[part]=='.' && path[part+1]=='.')) return 0;
            part=i+1;
        } else if (c<32 || c=='\\' || c==':') return 0;
    }
    if (path[length-4]!='.' || (path[length-3]|32)!='s' ||
        (path[length-2]|32)!='a' || (path[length-1]|32)!='v') return 0;
    if (!out) return 1;
    for (uint32_t i=0;i<=length;i++) out[i]=path[i];
    if (file!=FS_TRANSACTION_SOURCE) {
        const char *extension=file==FS_TRANSACTION_SOURCE_TEMP?"tmp":"bak";
        for (uint32_t i=0;i<3;i++) out[length-3+i]=(uint16_t)extension[i];
    }
    return 1;
}

static int same(const void *a,const void *b,uint32_t size)
{
    for (uint32_t i=0;i<size;i++) if (((const uint8_t *)a)[i]!=((const uint8_t *)b)[i]) return 0;
    return 1;
}

static int cached(uint32_t file)
{
    return file<=FS_TRANSACTION_BACKUP ||
        (file>=FS_TRANSACTION_SOURCE && file<=FS_TRANSACTION_SOURCE_BACKUP);
}

void FsTransaction_ViewClear(const FsTransactionIo *io)
{
    if (io && io->view) {
        FsTransactionView *view=io->view;
        for (uint32_t i=0;i<sizeof(*view);i++) ((uint8_t *)view)[i]=0;
    }
}

void FsTransaction_ViewForget(const FsTransactionIo *io,uint32_t file)
{
    if (!io || !io->view || file>=FS_TRANSACTION_FILE_COUNT) return;
    io->view->known&=~(1u<<file);
    if (file==FS_TRANSACTION_BANK) io->view->bankStructured=0;
    if (io->view->bodyValid && io->view->bodyFile==file) io->view->bodyValid=0;
}

static int sourceKey(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record)
{
    if (file<FS_TRANSACTION_SOURCE || file>FS_TRANSACTION_SOURCE_BACKUP) return 1;
    if (!record || record->sourceKind!=FS_TRANSACTION_NDS_FILE ||
        !record->sourcePathSize || record->sourcePathSize>sizeof(io->view->sourcePath)) return 0;
    FsTransactionView *view=io->view;
    if (view->sourceGameCode==record->sourceGameCode && view->sourcePathSize==record->sourcePathSize &&
        same(view->sourcePath,record->sourcePath,record->sourcePathSize)) return 1;
    for (uint32_t i=FS_TRANSACTION_SOURCE;i<=FS_TRANSACTION_SOURCE_BACKUP;i++)
        FsTransaction_ViewForget(io,i);
    view->sourceGameCode=record->sourceGameCode;view->sourcePathSize=record->sourcePathSize;
    for (uint32_t i=0;i<record->sourcePathSize/2;i++) view->sourcePath[i]=record->sourcePath[i];
    return 1;
}

int FsTransaction_ViewGet(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,
    FsTransactionFingerprint *out)
{
    if (!io || !io->view || !cached(file) || !sourceKey(io,file,record) ||
        !(io->view->known&(1u<<file))) return 0;
    *out=io->view->files[file];return 1;
}

void FsTransaction_ViewPut(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,
    const FsTransactionFingerprint *value)
{
    if (!io || !io->view || !cached(file) || !sourceKey(io,file,record)) return;
    io->view->files[file]=*value;io->view->known|=1u<<file;
}

int FsTransaction_ViewBody(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,
    const FsTransactionFingerprint *expected)
{
    FsTransactionFingerprint actual;
    return expected->present && io->view && io->view->bodyValid &&
        FsTransaction_ViewGet(io,file,record,&actual) && same(&actual,expected,sizeof(actual)) &&
        same(&io->view->body,expected,sizeof(actual));
}

void FsTransaction_ViewRememberBody(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,
    const FsTransactionFingerprint *value,const void *data,uint32_t size)
{
    if (!io || !io->view || !cached(file) || !sourceKey(io,file,record) || !value->present ||
        size!=value->size || !io->buffer || size>io->bufferSize || !io->bufferBusy ||
        __sync_lock_test_and_set(io->bufferBusy,1)) return;
    if (data!=io->buffer) for (uint32_t i=0;i<size;i++) io->buffer[i]=((const uint8_t *)data)[i];
    io->view->body=*value;io->view->bodyFile=file;io->view->bodyValid=1;
    __sync_lock_release(io->bufferBusy);
    FsTransaction_ViewPut(io,file,record,value);
}

int FsTransaction_OpenWrite(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record,uint32_t *handle)
{
    FsTransaction_ViewForget(io,file);
    int ok=io->openWrite(file,record,handle);
    if (!ok) FsTransaction_ViewClear(io);
    return ok;
}

int FsTransaction_Remove(const FsTransactionIo *io,uint32_t file,const FsTransactionRecord *record)
{
    FsTransaction_ViewForget(io,file);
    int ok=io->remove(file,record);
    if (!ok) FsTransaction_ViewClear(io);
    return ok;
}

int FsTransaction_Rename(const FsTransactionIo *io,uint32_t from,uint32_t to,const FsTransactionRecord *record)
{
    FsTransactionFingerprint value;
    int known=FsTransaction_ViewGet(io,from,record,&value) && value.present;
    int body=known && io->view && io->view->bodyValid && io->view->bodyFile==from;
    FsTransaction_ViewForget(io,from);FsTransaction_ViewForget(io,to);
    int ok=io->rename(from,to,record);
    if (!ok) FsTransaction_ViewClear(io);
    else if (known) {
        FsTransaction_ViewPut(io,to,record,&value);
        /* A successful rename preserves the verified bytes. A failed rename
           (including failure after effect) never transfers this evidence. */
        /* 成功改名保留已验证字节；失败（包括生效后报错）不迁移该依据。 */
        if (body && io->view) {
            io->view->body=value;io->view->bodyFile=to;io->view->bodyValid=1;
        }
    }
    return ok;
}
