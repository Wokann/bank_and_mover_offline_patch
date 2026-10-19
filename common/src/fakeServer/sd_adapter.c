#include "fakeServer/sd_adapter.h"
#include "fs_system/fs_helpers.h"
#include "fs_system/console_identity.h"
#include "fs_system/console_settings.h"
#include "fs_system/utf_path.h"
#include "fs_system/diagnostic_log.h"
#include "fs_system/storage_notice.h"
#include "fakeServer/user_profiles.h"
#include "fakeServer/account_configuration.h"
#include "fakeServer/numbers.h"
#include "fake_server_context.h"
#include <stddef.h>
#include <string.h>

static const char serverRoot[]="/3ds/Bank/fakeServers/shared/";
extern const volatile uint8_t CombinePatch_ModeStorage[];
static FsUserProfile selectedUser __attribute__((section(".data.user_profile")))={0};
static const char *const transactionPaths[FS_TRANSACTION_FILE_COUNT]={
    "bankdata.bin","bankdata.tmp","bankdata.bak",
    "transaction.bin","transaction.tmp",
    "sav.bin","sav.tmp","sav.bak",
    0,0,0,
    "transaction.bak"
};

static uint32_t pathSize(const char *path)
{
    uint32_t length=0;
    while (path[length]) length++;
    return length+1;
}

static int nativePath(const FsPathView *input,FsPathView *out,u16 text[FS_USER_PATH_SIZE])
{
    *out=*input;
    if(input->type!=FS_PATH_TYPE_ASCII)return 1;
    const unsigned char *p=input->data;
    while(*p && *p<128)p++;
    if(!*p)return 1;
    uint32_t units=0;
    if(!FsUtf8_ToUtf16(input->data,text,FS_USER_PATH_SIZE,&units))return 0;
    out->type=FS_PATH_TYPE_UTF16;out->data=text;out->size=(units+1)*sizeof(*text);return 1;
}

static int makePath(const char *relative,char path[FS_USER_PATH_SIZE],FsPathView *view)
{
    if(FsSd_SelectUser()!=FS_OK)return 0;
    if(relative && (!strcmp(relative,"user.conf") || !strcmp(relative,"user.tmp") || !strcmp(relative,"user.bak"))) {
        if(!FsUser_Path(&selectedUser,0,relative,path))return 0;
        view->type=FS_PATH_TYPE_ASCII;view->data=path;view->size=pathSize(path);return 1;
    }
    if(relative && (!strcmp(relative,"client.conf") || !strcmp(relative,"client.tmp") || !strcmp(relative,"client.bak"))) {
        strcpy(path,!strcmp(relative,"client.conf")?FS_CLIENT_CONFIG:
            !strcmp(relative,"client.tmp")?FS_CLIENT_TEMP:FS_CLIENT_BACKUP);
        view->type=FS_PATH_TYPE_ASCII;view->data=path;view->size=pathSize(path);return 1;
    }
    uint32_t used=0;
    while (serverRoot[used]) { path[used]=serverRoot[used]; used++; }
    if (!relative || !*relative || *relative=='/') return 0;
    const char *part=relative;
    for (;;) {
        char byte=*relative;
        if (!byte || byte=='/') {
            uint32_t length=(uint32_t)(relative-part);
            if (!length || (length==1 && part[0]=='.') ||
                (length==2 && part[0]=='.' && part[1]=='.')) return 0;
            if (!byte) break;
            part=relative+1;
        } else if ((uint8_t)byte<32 || (uint8_t)byte>126 || byte==':' || byte=='\\') return 0;
        if (used+1>=FS_USER_PATH_SIZE) return 0;
        path[used++]=byte; relative++;
    }
    path[used++]=0;
    view->type=FS_PATH_TYPE_ASCII; view->size=used; view->data=path;
    return 1;
}

static int transactionPath(uint32_t file,const FsTransactionRecord *record,FsPathView *view,
    uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY],char userPath[FS_USER_PATH_SIZE])
{
    if (file>=FS_TRANSACTION_FILE_COUNT) return 0;
    if (file>=FS_TRANSACTION_SOURCE && file<=FS_TRANSACTION_SOURCE_BACKUP) {
        if (!record || record->sourceKind!=FS_TRANSACTION_NDS_FILE ||
            record->sourcePathSize<4 || (record->sourcePathSize&1) ||
            record->sourcePathSize>sizeof(record->sourcePath) ||
            record->sourcePath[record->sourcePathSize/2-1]) return 0;
        if (!FsTransaction_SourcePath(record,file,sourcePath)) return 0;
        view->type=FS_PATH_TYPE_UTF16;
        view->size=record->sourcePathSize;
        view->data=sourcePath;
    } else {
        if(FsSd_SelectUser()!=FS_OK || !FsUser_Path(&selectedUser,
            file>=FS_TRANSACTION_TURTLE && file<=FS_TRANSACTION_TURTLE_BACKUP,
            transactionPaths[file],userPath))return 0;
        view->type=FS_PATH_TYPE_ASCII;
        view->data=userPath; view->size=pathSize(view->data);
    }
    return 1;
}

static s32 openPath(const FsPathView *path,u32 flags,u32 *handle)
{
    u16 unicode[FS_USER_PATH_SIZE];FsPathView native;
    if(!nativePath(path,&native,unicode))return -1;
    path=&native;
    if (path->type==FS_PATH_TYPE_ASCII)
        return openFile(path->data,path->size,flags,handle);
    const u32 empty=0;
    FsDiag_Checkpoint("file.open16.begin",0,FsDiag_Path(path->type,path->data,path->size),flags,path->size);
    s32 result=FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,&empty,1,path->type,path->data,path->size,flags,0);
    FsDiag_Record("file.open16.end",result,FsDiag_Path(path->type,path->data,path->size),
        flags,result?0:*handle);
    return result;
}

static int openReadPath(const FsPathView *path,u32 flags,uint32_t *handle,uint64_t *size)
{
    u32 nativeHandle=0;
    s32 result=openPath(path,flags,&nativeHandle);
    if (result) {
        if (resultIsNotFound(result)) return 0;
        FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);return -1;
    }
    u64 nativeSize=0;
    result=FSFILE_GetSize(&nativeHandle,&nativeSize);
    FsDiag_Record("file.size",result,nativeHandle,(u32)nativeSize,(u32)(nativeSize>>32));
    if (result) { closeFile(nativeHandle);FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);return -1; }
    *handle=nativeHandle; *size=nativeSize;
    return 1;
}

static int openWritePath(const FsPathView *path,uint32_t *handle)
{
    u32 nativeHandle=0;
    if (!FsStorageNotice_CheckIo(!openPath(path,OPEN_READ|OPEN_WRITE|OPEN_CREATE,&nativeHandle))) return 0;
    *handle=nativeHandle;
    return 1;
}

static int sdSetSize(uint32_t handle,uint64_t size)
{
    return FsStorageNotice_CheckIo(!setSize(handle,size));
}

static int sdRead(uint32_t handle,uint64_t offset,void *data,uint32_t size)
{
    u32 nativeHandle=handle,actual=0;
    FsDiag_Checkpoint("file.read.begin",0,handle,(u32)offset,size);
    s32 result=FSFILE_Read(&nativeHandle,&actual,offset,data,size);
    FsDiag_Record("file.read.end",result,handle,size,actual);
    return FsStorageNotice_CheckIo(!result && actual==size);
}

static int sdWrite(uint32_t handle,uint64_t offset,const void *data,uint32_t size)
{
    u32 nativeHandle=handle,actual=0;
    FsDiag_Checkpoint("file.write.begin",0,handle,(u32)offset,size);
    s32 result=FSFILE_Write(&nativeHandle,&actual,offset,data,size,WRITE_FLUSH);
    FsDiag_Record("file.write.end",result,handle,size,actual);
    return FsStorageNotice_CheckIo(!result && actual==size);
}

static int sdClose(uint32_t handle)
{
    return FsStorageNotice_CheckIo(!closeFile(handle));
}

static int removePath(const FsPathView *path)
{
    u16 unicode[FS_USER_PATH_SIZE];FsPathView native;
    if(!nativePath(path,&native,unicode))return 0;
    path=&native;
    u64 archive=0;
    if (!FsStorageNotice_CheckIo(!openArchive(&archive))) return 0;
    s32 result;
    if (path->type==FS_PATH_TYPE_ASCII)
        result=pathCommand(FSUSER_CMD_DELETE_FILE,archive,path->data,path->size);
    else {
        volatile u32 *command=fsCommandBuffer();
        command[0]=FSUSER_CMD_DELETE_FILE; command[1]=0;
        command[2]=(u32)archive; command[3]=(u32)(archive>>32);
        command[4]=path->type; command[5]=path->size;
        command[6]=(path->size<<14)|2; command[7]=(u32)path->data;
        result=fsSync(*FSUSER_HandleSlot);
        if (!result) result=(s32)command[1];
    }
    int ok=!result || resultIsNotFound(result);
    s32 closed=closeArchive(archive);
    return FsStorageNotice_CheckIo(ok && !closed);
}

static int renamePaths(const FsPathView *from,const FsPathView *to)
{
    u16 left[FS_USER_PATH_SIZE],right[FS_USER_PATH_SIZE];FsPathView a,b;
    if(!nativePath(from,&a,left) || !nativePath(to,&b,right))return 0;
    from=&a;to=&b;
    u64 archive=0;
    if (!FsStorageNotice_CheckIo(!openArchive(&archive))) return 0;
    s32 result;
    if (from->type==FS_PATH_TYPE_ASCII && to->type==FS_PATH_TYPE_ASCII)
        result=renamePath(archive,from->data,from->size,to->data,to->size);
    else {
        volatile u32 *command=fsCommandBuffer();
        command[0]=FSUSER_CMD_RENAME_FILE; command[1]=0;
        command[2]=(u32)archive; command[3]=(u32)(archive>>32);
        command[4]=from->type; command[5]=from->size;
        command[6]=(u32)archive; command[7]=(u32)(archive>>32);
        command[8]=to->type; command[9]=to->size;
        command[10]=(from->size<<14)|0x402; command[11]=(u32)from->data;
        command[12]=(to->size<<14)|0x802; command[13]=(u32)to->data;
        result=fsSync(*FSUSER_HandleSlot);
        if (!result) result=(s32)command[1];
    }
    s32 closed=closeArchive(archive);
    return FsStorageNotice_CheckIo(!result && !closed);
}

static int storageOpenRead(const char *relative,uint32_t *handle,uint32_t *size)
{
    if(FsSd_SelectUser()!=FS_OK)return -1;
    if((FsSdStorage.capabilities&FS_STORAGE_READ_ONLY) && relative &&
        (!strcmp(relative,"user.conf") || !strcmp(relative,"user.tmp") || !strcmp(relative,"user.bak")))return 0;
    char path[FS_USER_PATH_SIZE]; FsPathView view; uint64_t actual=0;
    if (!makePath(relative,path,&view)) return -1;
    int result=openReadPath(&view,OPEN_READ,handle,&actual);
    if (result==1 && actual>0xffffffffu) { sdClose(*handle); return -1; }
    if (result==1) *size=(uint32_t)actual;
    return result;
}

static int storageOpenWrite(const char *relative,uint32_t size,uint32_t *handle)
{
    char path[FS_USER_PATH_SIZE]; FsPathView view;
    if (!makePath(relative,path,&view)) return 0;
    if(FsSdStorage.capabilities&FS_STORAGE_READ_ONLY)return 0;
    if (!openWritePath(&view,handle)) return 0;
    if (sdSetSize(*handle,size)) return 1;
    sdClose(*handle);
    return 0;
}

static int storageRead(uint32_t handle,uint32_t offset,void *data,uint32_t size)
{
    return sdRead(handle,offset,data,size);
}

static int storageWrite(uint32_t handle,uint32_t offset,const void *data,uint32_t size)
{
    return sdWrite(handle,offset,data,size);
}

static int storageRemove(const char *relative)
{
    if(FsSd_SelectUser()!=FS_OK || (FsSdStorage.capabilities&FS_STORAGE_READ_ONLY))return 0;
    char path[FS_USER_PATH_SIZE]; FsPathView view;
    return makePath(relative,path,&view) && removePath(&view);
}

static int storageRename(const char *from,const char *to)
{
    if(FsSd_SelectUser()!=FS_OK || (FsSdStorage.capabilities&FS_STORAGE_READ_ONLY))return 0;
    char fromPath[FS_USER_PATH_SIZE],toPath[FS_USER_PATH_SIZE]; FsPathView fromView,toView;
    return makePath(from,fromPath,&fromView) && makePath(to,toPath,&toView) &&
        renamePaths(&fromView,&toView);
}

static int transactionOpenRead(uint32_t file,const FsTransactionRecord *record,
    uint32_t *handle,uint64_t *size)
{
    /* Without a selected NDS source, there is no sibling work file to inspect.
       Do not scan other games or keep consulting the retired global path. */
    /* 没有选定 NDS 来源时，同名工作文件不适用；不扫描其他游戏或旧固定路径。 */
    if ((file==FS_TRANSACTION_SOURCE_TEMP || file==FS_TRANSACTION_SOURCE_BACKUP) &&
        (!record || record->sourceKind!=FS_TRANSACTION_NDS_FILE)) return FS_TRANSACTION_MISSING;
    if(file<FS_TRANSACTION_SOURCE || file>FS_TRANSACTION_SOURCE_BACKUP) {
        if(FsSd_SelectUser()!=FS_OK)return -1;
        if(FsSdStorage.capabilities&FS_STORAGE_READ_ONLY)return FS_TRANSACTION_MISSING;
    }
    FsPathView path;uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY];char userPath[FS_USER_PATH_SIZE];
    if (!transactionPath(file,record,&path,sourcePath,userPath)) return -1;
    return openReadPath(&path,OPEN_READ|(file==FS_TRANSACTION_SOURCE?OPEN_WRITE:0),handle,size);
}

static int transactionOpenWrite(uint32_t file,const FsTransactionRecord *record,uint32_t *handle)
{
    if(FsSd_SelectUser()!=FS_OK || (FsSdStorage.capabilities&FS_STORAGE_READ_ONLY))return 0;
    FsPathView path;uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY];char userPath[FS_USER_PATH_SIZE];
    if (!transactionPath(file,record,&path,sourcePath,userPath)) return 0;
    return openWritePath(&path,handle);
}

static int transactionRemove(uint32_t file,const FsTransactionRecord *record)
{
    if ((file==FS_TRANSACTION_SOURCE_TEMP || file==FS_TRANSACTION_SOURCE_BACKUP) &&
        (!record || record->sourceKind!=FS_TRANSACTION_NDS_FILE)) return 1;
    if(FsSd_SelectUser()!=FS_OK)return 0;
    if(FsSdStorage.capabilities&FS_STORAGE_READ_ONLY)
        return file<FS_TRANSACTION_SOURCE || file>FS_TRANSACTION_SOURCE_BACKUP;
    FsPathView path;uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY];char userPath[FS_USER_PATH_SIZE];
    return transactionPath(file,record,&path,sourcePath,userPath) && removePath(&path);
}

static int transactionRename(uint32_t from,uint32_t to,const FsTransactionRecord *record)
{
    if(FsSd_SelectUser()!=FS_OK || (FsSdStorage.capabilities&FS_STORAGE_READ_ONLY))return 0;
    FsPathView fromPath,toPath;
    uint16_t sourceFrom[FS_TRANSACTION_PATH_CAPACITY],sourceTo[FS_TRANSACTION_PATH_CAPACITY];
    char userFrom[FS_USER_PATH_SIZE],userTo[FS_USER_PATH_SIZE];
    return transactionPath(from,record,&fromPath,sourceFrom,userFrom) && transactionPath(to,record,&toPath,sourceTo,userTo) &&
        renamePaths(&fromPath,&toPath);
}

/* ARM provides the 32 x 32 -> 64 multiply without a runtime-library helper. */
/* ARM 指令可直接完成 32 位乘数的 64 位积，不依赖运行库辅助函数。 */
__attribute__((target("arm"),noinline))
static int transactionFreeBytes(uint64_t *bytes)
{
    if (!bytes) return 0;
    *bytes=0;
    FsDiag_Checkpoint("space.begin",0,0,0,0);
    volatile u32 *command=fsCommandBuffer();
    command[0]=FSUSER_CMD_GET_SDMC_ARCHIVE_RESOURCE;
    s32 result=fsSync(*FSUSER_HandleSlot);
    if (!result) result=(s32)command[1];
    u32 sectorSize=0,clusterSize=0,totalClusters=0,freeClusters=0;
    if (!result) {
        sectorSize=command[2]; clusterSize=command[3];
        totalClusters=command[4]; freeClusters=command[5];
    }
    int valid=!result && sectorSize && !(sectorSize&(sectorSize-1)) &&
        clusterSize>=sectorSize && !(clusterSize&(sectorSize-1)) &&
        totalClusters && freeClusters<=totalClusters;
    /* SDMC capacity comes from its media resource, not GetFreeBytes on a save
       archive. clusterSize is already in bytes; widen before multiplying so
       FAT32 volumes with more than 4 GiB free are not truncated. */
    /* SDMC 容量取自介质资源，不对存档归档调用 GetFreeBytes。clusterSize 已为
       字节数，乘法前提升为 64 位，避免 FAT32 大容量卷的空闲空间被截断。 */
    if (valid) *bytes=(uint64_t)clusterSize*freeClusters;
    FsDiag_Record("space.resource",result,sectorSize,clusterSize,totalClusters);
    FsDiag_Record("space.result",result,(u32)*bytes,(u32)(*bytes>>32),freeClusters);
    if (!result && !valid) FsDiag_Record("space.resource.invalid",FS_INVALID,0,0,0);
    return valid;
}

/* Retail GFL heap ABI, independently checked in both binaries. Its public
   allocator loops forever on OOM. Use the same locked raw allocation and
   ownership header, but return NULL before entering that fatal branch.
   The unchanged retail FreeMemoryAuto remains the paired release. */
/* 两端独立核对的原 GFL 堆 ABI。公开分配器在内存不足时无限等待；这里只复用
   同一带锁底层分配及原所有权头，失败返回 NULL，仍配对原版自动释放。 */
typedef struct StorageHeap StorageHeap;
typedef struct StorageHeapVtable {
    void *reserved00[7];
    int (*alignment)(StorageHeap *);
    uint32_t (*identity)(StorageHeap *);
    void *reserved24[9];
    void (*increment)(StorageHeap *);
    void *reserved4c[3];
    void *(*allocate)(StorageHeap *,int,int);
    void *reserved5c[2];
    void (*userHeader)(StorageHeap *,void *);
} StorageHeapVtable;
struct StorageHeap { const StorageHeapVtable *vtable; };
_Static_assert(offsetof(StorageHeapVtable,allocate)==0x58 &&
    offsetof(StorageHeapVtable,userHeader)==0x64,"retail GFL heap vtable ABI");

static void *storageAllocate(uint32_t size)
{
    if (!size) return 0;
    StorageHeap *heap=FsHeap_Get(0);
    const StorageHeapVtable *ops=heap->vtable;
    int alignment=ops->alignment(heap);
    if (!alignment || alignment==(-2147483647-1)) return 0;
    uint32_t offset=alignment<0?(uint32_t)-alignment:(uint32_t)alignment;
    if (offset>0xffffu || (offset&(offset-1))) return 0;
    if (offset<32) offset=32;
    if (size>0x7fffffffu-offset) return 0;
    uint8_t *raw=ops->allocate(heap,(int)(size+offset),alignment);
    if (!raw) return 0;
    memset(raw,0,size+offset);
    uint8_t *result=raw+offset,*header=result-32;
    uint32_t id=ops->identity(heap);
    memcpy(header,&id,sizeof(id));
    uint16_t distance=(uint16_t)offset;
    memcpy(header+4,&distance,sizeof(distance));
    ops->userHeader(heap,header+6);
    if (offset>32) memcpy(raw,header,32);
    ops->increment(heap);
    return result;
}
static void storageRelease(void *data) { if (data) FsHeap_Release(data); }

FsStorageIo FsSdStorage={
    storageOpenRead,storageOpenWrite,storageRead,storageWrite,sdClose,storageRemove,storageRename,
    storageAllocate,storageRelease,FS_STORAGE_ACCOUNT_LAYOUT
};

static int transactionRandomBytes(void *data,uint32_t size)
{
    u32 handle=0;
    if (!data || !size || size>0xffffu) return 0;
    FsDiag_Checkpoint("random.open.begin",0,size,0,0);
    s32 opened=SRV_GetServiceHandle(&handle,"ps:ps",5,1);
    FsDiag_Record("random.open.end",opened,size,handle,0);
    if(opened)return 0;
    FsDiag_Checkpoint("random.generate.begin",0,size,0,0);
    volatile u32 *command=fsCommandBuffer();
    command[0]=0x000D0042u;
    command[1]=size;
    command[2]=(size<<4)|0x0Cu;
    command[3]=(u32)data;
    s32 result=fsSync(handle);
    if (!result) result=(s32)command[1];
    FsDiag_Record("random.result",result,size,0,0);
    s32 closed=fsCloseHandle(handle);
    return !result && !closed;
}

const FsTransactionIo FsSdTransactions={
    transactionOpenRead,transactionOpenWrite,sdSetSize,sdRead,sdWrite,sdClose,
    transactionRemove,transactionRename,transactionFreeBytes,transactionRandomBytes,
    0,0,0,0,1
};

/* Profile discovery uses an unscoped backend to avoid resolving itself. */
static int rawOpenRead(const char *path,uint32_t *handle,uint32_t *size)
{
    FsPathView view={FS_PATH_TYPE_ASCII,pathSize(path),path};uint64_t actual=0;
    int result=openReadPath(&view,OPEN_READ,handle,&actual);
    if(result==1 && actual>UINT32_MAX){sdClose(*handle);return -1;}
    if(result==1)*size=(uint32_t)actual;
    return result;
}
static int rawOpenWrite(const char *path,uint32_t size,uint32_t *handle)
{
    FsPathView view={FS_PATH_TYPE_ASCII,pathSize(path),path};
    if(!openWritePath(&view,handle))return 0;
    if(sdSetSize(*handle,size))return 1;
    sdClose(*handle);return 0;
}
static int rawRemove(const char *path)
{ FsPathView view={FS_PATH_TYPE_ASCII,pathSize(path),path};return removePath(&view); }
static int rawRename(const char *from,const char *to)
{
    FsPathView a={FS_PATH_TYPE_ASCII,pathSize(from),from},b={FS_PATH_TYPE_ASCII,pathSize(to),to};
    return renamePaths(&a,&b);
}
static const FsStorageIo profileStorage={rawOpenRead,rawOpenWrite,storageRead,storageWrite,
    sdClose,rawRemove,rawRename,storageAllocate,storageRelease,0};

int FsSd_ReadLogSetting(uint32_t *enabled)
{
    return FsAccountConfiguration_ReadLogSetting(&profileStorage,
        "/3ds/Bank/fakeServers/shared/server.conf","/3ds/Bank/fakeServers/shared/server.tmp",enabled);
}

static int openDirectory(const char *path,u32 *handle)
{
    FsDiag_Checkpoint("directory.open.begin",0,
        FsDiag_Path(FS_PATH_TYPE_ASCII,path,pathSize(path)),0,0);
    u16 unicode[FS_USER_PATH_SIZE];FsPathView input={FS_PATH_TYPE_ASCII,pathSize(path),path},native;
    if(!nativePath(&input,&native,unicode))return -1;
    u64 archive=0;if(!FsStorageNotice_CheckIo(!openArchive(&archive)))return -1;
    u32 size=native.size;volatile u32 *c=fsCommandBuffer();
    c[0]=FSUSER_CMD_OPEN_DIRECTORY;c[1]=(u32)archive;c[2]=(u32)(archive>>32);
    c[3]=native.type;c[4]=size;c[5]=(size<<14)|2;c[6]=(u32)native.data;
    s32 result=fsSync(*FSUSER_HandleSlot);if(!result)result=(s32)c[1];
    if(!result)*handle=c[3];
    FsDiag_Record("directory.open.end",result,
        FsDiag_Path(native.type,native.data,native.size),result?0:*handle,0);
    s32 closed=closeArchive(archive);
    if(closed && !result){fsCloseHandle(*handle);FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);return -1;}
    if(!result)return 1;
    if(resultIsNotFound(result))return 0;
    FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);return -1;
}
static int closeDirectory(u32 handle)
{
    FsDiag_Checkpoint("directory.close.begin",0,handle,0,0);
    volatile u32 *c=fsCommandBuffer();c[0]=FSDIR_CMD_CLOSE;
    s32 result=fsSync(handle);if(!result)result=(s32)c[1];
    FsDiag_Record("directory.close",result,handle,0,0);
    s32 closed=fsCloseHandle(handle);return FsStorageNotice_CheckIo(!result && !closed);
}
static int profileDirectories(const char *path,FsUserVisitor visit,void *context)
{
    u32 handle=0;int present=openDirectory(path,&handle);if(present!=1)return present;
    int ok=1;
    for(;;) {
        FsDirectoryEntry entry;volatile u32 *c=fsCommandBuffer();
        c[0]=FSDIR_CMD_READ;c[1]=1;c[2]=(sizeof(entry)<<4)|0xCu;c[3]=(u32)&entry;
        s32 result=fsSync(handle);if(!result)result=(s32)c[1];
        if(result){FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);ok=0;break;}
        if(!c[2])break;
        if(c[2]!=1){FsStorageNotice_Mark(FS_STORAGE_ISSUE_IO);ok=0;break;}
        if(!(entry.attributes&FS_ATTRIBUTE_DIRECTORY))continue;
        char name[FS_USER_ID_SIZE];u32 n=0;
        while(n<sizeof(entry.name)/sizeof(*entry.name) && entry.name[n])n++;
        if(n==sizeof(entry.name)/sizeof(*entry.name) || !FsUtf16_ToUtf8(entry.name,n,name,sizeof(name)))name[0]=0;
        if(!strcmp(name,".") || !strcmp(name,".."))continue;
        if(!visit(context,name)){ok=0;break;}
    }
    if(!closeDirectory(handle))ok=0;
    return ok?1:-1;
}
static int profileMkdir(const char *path)
{
    FsDiag_Checkpoint("directory.ensure.begin",0,
        FsDiag_Path(FS_PATH_TYPE_ASCII,path,pathSize(path)),0,0);
    u16 unicode[FS_USER_PATH_SIZE];FsPathView input={FS_PATH_TYPE_ASCII,pathSize(path),path},native;
    if(!nativePath(&input,&native,unicode))return 0;
    u64 archive=0;if(!FsStorageNotice_CheckIo(!openArchive(&archive)))return 0;
    u32 size=native.size;volatile u32 *c=fsCommandBuffer();
    c[0]=FSUSER_CMD_CREATE_DIRECTORY;c[1]=0;c[2]=(u32)archive;c[3]=(u32)(archive>>32);
    c[4]=native.type;c[5]=size;c[6]=0;c[7]=(size<<14)|2;c[8]=(u32)native.data;
    s32 result=fsSync(*FSUSER_HandleSlot);if(!result)result=(s32)c[1];
    FsDiag_Record("directory.create.result",result,
        FsDiag_Path(native.type,native.data,native.size),native.type,native.size);
    if(!FsStorageNotice_CheckIo(!closeArchive(archive)))return 0;
    if(!result)return 1;
    u32 directory=0;int ok=openDirectory(path,&directory)==1 && closeDirectory(directory);
    FsDiag_Record("directory.ensure.end",ok?0:-1,
        FsDiag_Path(FS_PATH_TYPE_ASCII,path,pathSize(path)),0,0);
    return FsStorageNotice_CheckIo(ok);
}
typedef struct ConsoleStartupUser {
    struct ConsoleStartupUser *next;
    uint32_t applied;
    char id[FS_USER_ID_SIZE];
} ConsoleStartupUser;
/* Process-lifetime cache, like the title catalogue: one small node per used
   account, no SD writes and no fixed user-count ceiling. */
static struct {uint32_t captured;ConsoleStartupUser *users;} consoleStartup
    __attribute__((section(".data.console_startup_users")))={0};
int FsSd_SelectUser(void)
{
    FsDiag_Record("user.resolve.begin",0,FAKE_SERVER_CLIENT_OWNER,CombinePatch_ModeStorage[0],
        selectedUser.ready);
    if(!consoleStartup.captured) {
        FsConsoleProfile host;ConsoleSettings_GetProfile(&host);FsConsole_SetHost(&host);
        consoleStartup.captured=1;
    }
    const FsProfileIo io={&profileStorage,profileDirectories,profileMkdir,
        ConsoleIdentity_Read,transactionRandomBytes};
    /* Only offline Mover defers profile provisioning/migration to Bank. Online
       source-save protection and Bank initialization retain their own paths. */
    /* 仅离线 Mover 将用户创建／迁移留给 Bank；联网来源保护与 Bank 初始化不改。 */
    int defer=FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_MOVER && !CombinePatch_ModeStorage[0];
    if(!defer && selectedUser.ready==FS_USER_UNINITIALIZED)selectedUser.ready=0;
    int result=defer?FsUser_SelectExisting(&io,&selectedUser):FsUser_Select(&io,&selectedUser);
    FsDiag_Record("user.resolve.end",result,selectedUser.number,selectedUser.ready,(u32)defer);
    if(result==FS_OK) {
        ConsoleStartupUser *entry=consoleStartup.users;
        while(entry && strcmp(entry->id,selectedUser.id))entry=entry->next;
        if(!entry) {
            entry=profileStorage.allocate(sizeof(*entry));if(!entry)return FS_NO_MEMORY;
            entry->next=consoleStartup.users;entry->applied=0;strcpy(entry->id,selectedUser.id);
            consoleStartup.users=entry;
        }
        FsConsole_SelectStartup(&entry->applied);
        FsSdStorage.capabilities=FS_STORAGE_ACCOUNT_LAYOUT |
            (selectedUser.ready==FS_USER_UNINITIALIZED?FS_STORAGE_READ_ONLY:0u);
    }
    return result;
}
void FsSd_ResetUser(void)
{
    FsTransaction_ViewClear(&FsSdTransactions);
    memset(&selectedUser,0,sizeof(selectedUser));
    FsConsole_SelectStartup(0);
    FsSdStorage.capabilities=FS_STORAGE_ACCOUNT_LAYOUT;
}

int FsSd_UserMenu(uint32_t action,uint32_t activeUser,void *body,uint32_t size,
    FsUserMenu *menu,uint32_t *outputSize)
{
    const FsProfileIo io={&profileStorage,profileDirectories,profileMkdir,
        ConsoleIdentity_Read,transactionRandomBytes};
    *outputSize=0;
    uint32_t capacity=FsDivideU32(size,sizeof(FsUserEntry),0);
    if(action==1)return FsUser_CommitMenu(&io,body,capacity,activeUser,menu);
    FsClientConfig config;
    int result=FsUser_Catalogue(&io,&config,menu);
    if(result!=FS_OK)return result;
    FsConfigError error;
    result=FsAccountConfiguration_RepairPublished(io.files,
        "/3ds/Bank/fakeServers/shared/server.conf","/3ds/Bank/fakeServers/shared/server.tmp",
        FS_ACCOUNT_SHARED,0,&error);
    if(result==FS_MISSING)result=FS_OK;
    FsConsoleProfile factory;ConsoleSettings_GetProfile(&factory);
    /* Upgrade only published account configuration. Do not resolve/provision
       a user, touch Bank/settings/journals or consume automatic boot refresh. */
    /* 仅升级既有正式用户配置；不解析／创建活动用户，不碰银行、存档和事务，不消耗启动刷新。 */
    for(uint32_t i=0;result==FS_OK && i<config.count;i++) {
        FsUserProfile user={0};char path[FS_USER_PATH_SIZE],temp[FS_USER_PATH_SIZE];
        strcpy(user.id,config.users[i].id);
        if(!FsUser_Path(&user,0,"user.conf",path) || !FsUser_Path(&user,0,"user.tmp",temp)) {
            result=FS_INVALID;break;
        }
        result=FsAccountConfiguration_RepairPublished(io.files,path,temp,FS_ACCOUNT_USER,&factory,&error);
        if(result==FS_MISSING)result=FS_OK;
    }
    if(result!=FS_OK){FsClient_Release(io.files,&config);return result;}
    if(config.count>capacity)result=FS_NO_MEMORY;
    else {
        *outputSize=config.count*sizeof(FsUserEntry);
        if(*outputSize)memcpy(body,config.users,*outputSize);
    }
    FsClient_Release(io.files,&config);return result;
}
