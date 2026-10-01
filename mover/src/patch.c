typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

enum {
    BANK_FILE_SIZE=0xBB518,
    TRANSFER_BOX_POINTER_OFFSET=0xBB524,
    TRANSFER_BOX_SLOT_COUNT=30,
    FS_ARCHIVE_ID_SDMC=9, FS_PATH_TYPE_EMPTY=1, FS_PATH_TYPE_ASCII=3,
    OPEN_READ=1, OPEN_WRITE=2, OPEN_CREATE=4, WRITE_FLUSH=1,
    RESULT_SUMMARY_SHIFT=21, RESULT_SUMMARY_MASK=0x3F,
    RESULT_SUMMARY_NOT_FOUND=4
};

#define FSUSER_HandleSlot ((volatile u32 *)0x00311F80u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x001DF448u)
#define FSFILE_Read ((FSFILE_ReadFn)0x0015930Cu)
#define FSFILE_Close ((FSFILE_CloseFn)0x00159364u)
#define FSFILE_Write ((FSFILE_WriteFn)0x00159390u)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001593F0u)
#define Heap_AllocateObject ((Heap_AllocateObjectFn)0x001E6360u)
#define TransferObject_Create ((TransferObject_CreateFn)0x001DFBA4u)
#define TransferSlot_IsOccupied ((TransferSlot_IsOccupiedFn)0x0019A858u)
#define TransferSlot_Load ((TransferSlot_LoadFn)0x0019A224u)
#define TransferSlot_Clear ((TransferSlot_ClearFn)0x0019A7E0u)
#define TransferSlot_Write ((TransferSlot_WriteFn)0x0019A6F4u)
#define TransferSlot_CountOccupied ((TransferSlot_CountOccupiedFn)0x0024D624u)
#define StateTimer_HasElapsed ((StateTimer_HasElapsedFn)0x0019B758u)
#define StateTimer_Reset ((StateTimer_ResetFn)0x00233844u)
#define BankFile_Vtable 0x002E6DD0u
#define FSUSER_CMD_OPEN_ARCHIVE 0x080C00C2u
#define FSUSER_CMD_CREATE_DIRECTORY 0x08090182u
#define FSUSER_CMD_DELETE_FILE 0x08040142u
#define FSUSER_CMD_RENAME_FILE 0x08050244u
#define FSUSER_CMD_CLOSE_ARCHIVE 0x080E0080u
#define FSFILE_CMD_SET_SIZE 0x08050080u

typedef s32 (*FSUSER_OpenFileDirectlyFn)(volatile u32 *,u32 *,u32,u32,u32,const void *,u32,u32,const void *,u32,u32,u32);
typedef s32 (*FSFILE_ReadFn)(u32 *,u32 *,u64,void *,u32);
typedef s32 (*FSFILE_WriteFn)(u32 *,u32 *,u64,const void *,u32,u32);
typedef s32 (*FSFILE_CloseFn)(u32 *);
typedef s32 (*FSFILE_GetSizeFn)(u32 *,u64 *);
typedef void *(*Heap_AllocateObjectFn)(u32,u32);
typedef void *(*TransferObject_CreateFn)(void *,u32,u32);
typedef int (*TransferSlot_IsOccupiedFn)(void *,u32);
typedef void (*TransferSlot_LoadFn)(void *,void *,u32);
typedef void (*TransferSlot_ClearFn)(void *,u32);
typedef void (*TransferSlot_WriteFn)(void *,void *,u32);
typedef int (*TransferSlot_CountOccupiedFn)(void *);
typedef int (*StateTimer_HasElapsedFn)(void *,u32);
typedef void (*StateTimer_ResetFn)(void *);

static const char emptyPath[1]={0};
static const char directory3ds[]="/3ds";
static const char directoryBank[]="/3ds/Bank";
static const char bankPath[]="/3ds/Bank/bankdata.bin";
static const char tempPath[]="/3ds/Bank/bankdata.tmp";
static const char backupPath[]="/3ds/Bank/bankdata.bak";

static volatile u32 *commandBuffer(void)
{
    u32 tls; __asm__ volatile("mrc p15, 0, %0, c13, c0, 3":"=r"(tls));
    return (volatile u32 *)(tls+0x80u);
}

static s32 sync(u32 handle)
{
    register u32 r0 __asm__("r0")=handle;
    __asm__ volatile("svc 0x32":"+r"(r0)::"r1","r2","r3","r12","memory","cc");
    return (s32)r0;
}

static s32 openArchive(u64 *archive)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=FSUSER_CMD_OPEN_ARCHIVE; c[1]=FS_ARCHIVE_ID_SDMC; c[2]=FS_PATH_TYPE_EMPTY; c[3]=1;
    c[4]=(1u<<14)|2u; c[5]=(u32)emptyPath;
    r=sync(*FSUSER_HandleSlot); if (r) return r; r=(s32)c[1];
    if (!r) *archive=(u64)c[2]|((u64)c[3]<<32);
    return r;
}

static void closeArchive(u64 archive)
{
    volatile u32 *c=commandBuffer();
    c[0]=FSUSER_CMD_CLOSE_ARCHIVE; c[1]=(u32)archive; c[2]=(u32)(archive>>32); (void)sync(*FSUSER_HandleSlot);
}

static void createDirectory(u64 archive,const char *path,u32 size)
{
    volatile u32 *c=commandBuffer();
    c[0]=FSUSER_CMD_CREATE_DIRECTORY; c[1]=0; c[2]=(u32)archive; c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=size; c[6]=0; c[7]=(size<<14)|2u; c[8]=(u32)path;
    (void)sync(*FSUSER_HandleSlot);
}

static s32 pathCommand(u32 command,u64 archive,const char *path,u32 size)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=command; c[1]=0; c[2]=(u32)archive; c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=size; c[6]=(size<<14)|2u; c[7]=(u32)path;
    r=sync(*FSUSER_HandleSlot); return r?r:(s32)c[1];
}

static s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=FSUSER_CMD_RENAME_FILE; c[1]=0; c[2]=(u32)archive; c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=fromSize; c[6]=(u32)archive; c[7]=(u32)(archive>>32);
    c[8]=FS_PATH_TYPE_ASCII; c[9]=toSize; c[10]=(fromSize<<14)|0x402u; c[11]=(u32)from;
    c[12]=(toSize<<14)|0x802u; c[13]=(u32)to;
    r=sync(*FSUSER_HandleSlot); return r?r:(s32)c[1];
}

static s32 setSize(u32 handle,u64 size)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=FSFILE_CMD_SET_SIZE; c[1]=(u32)size; c[2]=(u32)(size>>32);
    r=sync(handle); return r?r:(s32)c[1];
}

static s32 openFile(const char *path,u32 size,u32 flags,u32 *handle)
{
    return FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,emptyPath,1,FS_PATH_TYPE_ASCII,path,size,flags,0);
}

static void ensureDirectories(void)
{
    u64 archive=0; if (openArchive(&archive)) return;
    createDirectory(archive,directory3ds,sizeof(directory3ds));
    createDirectory(archive,directoryBank,sizeof(directoryBank)); closeArchive(archive);
}

static int validHeader(const u8 *data)
{
    data+=0x15C; return (u16)(data[0]|((u16)data[1]<<8))==2 &&
        (u16)(data[2]|((u16)data[3]<<8))==100;
}

static int resultIsNotFound(s32 result)
{
    return (((u32)result>>RESULT_SUMMARY_SHIFT)&RESULT_SUMMARY_MASK)==
        RESULT_SUMMARY_NOT_FOUND;
}

static void destroyTransferObjects(void *objects[TRANSFER_BOX_SLOT_COUNT])
{
    u32 i;
    for (i=0;i<TRANSFER_BOX_SLOT_COUNT;i++) {
        void *object=objects[i];
        if (object) {
            void (**vtable)(void *)=*(void (***)(void *))object;
            vtable[2](object);
            objects[i]=0;
        }
    }
}

static int loadLocalBank(void *state)
{
    u8 *flow=*(u8 **)((u8 *)state+8); u8 *object=flow?*(u8 **)(flow+0xCC):0;
    void *transferObjects[TRANSFER_BOX_SLOT_COUNT];
    u32 h=0,n=0,i,count=0; u64 size=0; s32 r=-1,closeResult=0; void *box; u32 heap;
    for (i=0;i<TRANSFER_BOX_SLOT_COUNT;i++) transferObjects[i]=0;
    if (!object || *(u32 *)object!=BankFile_Vtable) return -1;
    box=*(void **)(object+TRANSFER_BOX_POINTER_OFFSET);
    if (!box) return -1;
    heap=*(u32 *)((u8 *)state+0x0C);
    /* Preserve candidates through the same semantic slot objects used by the
       stock download callback; this also keeps the patch stack small. */
    /* 使用原版下载回调相同的语义槽对象保留候选，同时降低补丁栈占用。 */
    for (i=0;i<TRANSFER_BOX_SLOT_COUNT;i++) {
        if (TransferSlot_IsOccupied(box,i)) {
            void *memory=Heap_AllocateObject(0x14u,heap);
            void *candidate=memory?TransferObject_Create(memory,heap,1u):0;
            if (!candidate) { destroyTransferObjects(transferObjects); return -1; }
            TransferSlot_Load(box,candidate,i);
            transferObjects[count++]=candidate;
        }
    }
    r=openFile(bankPath,sizeof(bankPath),OPEN_READ,&h);
    if (!r) r=FSFILE_GetSize(&h,&size);
    if (!r && size==BANK_FILE_SIZE) r=FSFILE_Read(&h,&n,0,object+8,BANK_FILE_SIZE); else if (!r) r=-1;
    if (h) closeResult=FSFILE_Close(&h);
    if (r || closeResult || n!=BANK_FILE_SIZE || !validHeader(object+8)) {
        destroyTransferObjects(transferObjects); return -1;
    }
    if (TransferSlot_CountOccupied(box)!=0) { destroyTransferObjects(transferObjects); return 1; }
    for (i=0;i<TRANSFER_BOX_SLOT_COUNT;i++) TransferSlot_Clear(box,i);
    for (i=0;i<count;i++) TransferSlot_Write(box,transferObjects[i],i);
    destroyTransferObjects(transferObjects);
    return 0;
}

static int writeTemporary(const void *data)
{
    u32 h=0,n=0; s32 r,closeResult=0; ensureDirectories();
    r=openFile(tempPath,sizeof(tempPath),OPEN_READ|OPEN_WRITE|OPEN_CREATE,&h);
    if (!r) r=setSize(h,BANK_FILE_SIZE);
    if (!r) r=FSFILE_Write(&h,&n,0,data,BANK_FILE_SIZE,WRITE_FLUSH);
    if (h) closeResult=FSFILE_Close(&h);
    return !r && !closeResult && n==BANK_FILE_SIZE;
}

static int commitTemporary(void)
{
    u64 archive=0; s32 r,restoreResult; int hasBackup=0;
    if (openArchive(&archive)) return 0;

    /* Only a missing old backup is harmless. Preserve the current bank file
       unless it was successfully moved aside before installing the temp file. */
    /* 只有旧备份不存在属于无害情况。必须先成功把当前银行文件移作备份，
       才能安装临时文件。 */
    r=pathCommand(FSUSER_CMD_DELETE_FILE,archive,backupPath,sizeof(backupPath));
    if (r && !resultIsNotFound(r)) { closeArchive(archive); return 0; }
    r=renamePath(archive,bankPath,sizeof(bankPath),backupPath,sizeof(backupPath));
    if (!r) hasBackup=1;
    else if (!resultIsNotFound(r)) { closeArchive(archive); return 0; }

    r=renamePath(archive,tempPath,sizeof(tempPath),bankPath,sizeof(bankPath));
    if (r && hasBackup) {
        restoreResult=renamePath(archive,backupPath,sizeof(backupPath),bankPath,sizeof(bankPath));
        (void)restoreResult;
    }
    closeArchive(archive); return !r;
}

typedef struct { u64 valueMs,valueTick; s64 systemClockHz,driftMs; } SystemTimeReference;

static u64 divideU64(u64 numerator,u64 denominator,u64 *remainder)
{
    u64 quotient=0,rest=0; u32 bit;
    if (!denominator) { if (remainder) *remainder=0; return 0; }
    for (bit=0;bit<64;bit++) {
        rest=(rest<<1)|(numerator>>63); numerator<<=1; quotient<<=1;
        if (rest>=denominator) { rest-=denominator; quotient|=1; }
    }
    if (remainder) *remainder=rest;
    return quotient;
}

static u64 systemTick(void)
{
    register u32 low __asm__("r0"); register u32 high __asm__("r1");
    __asm__ volatile("svc 0x28":"=r"(low),"=r"(high)::"r2","r3","r12","memory","cc");
    return (u64)low|((u64)high<<32);
}

static u64 currentSystemTimeMs(void)
{
    volatile u32 *counter=(volatile u32 *)0x1FF81000u;
    volatile SystemTimeReference *refs=(volatile SystemTimeReference *)0x1FF81020u;
    SystemTimeReference ref; u32 before,after,zero=0; u64 elapsed,remainder,seconds,adjustment;
    const u64 hourMs=3600000u;
    do {
        before=*counter; ref=refs[before&1u];
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 5"::"r"(zero):"memory"); after=*counter;
    } while (before!=after);
    elapsed=systemTick()-ref.valueTick;
    if (ref.systemClockHz>0) {
        seconds=divideU64(elapsed,(u64)ref.systemClockHz,&remainder);
        elapsed=seconds*1000u+divideU64(remainder*1000u,(u64)ref.systemClockHz,0);
    } else elapsed=0;
    /* Apply PTM's measured drift gradually over one hour, matching the system
       time model used by libctru. */
    /* 在一小时内逐步应用 PTM 测得的漂移量，与 libctru 的系统时间模型一致。 */
    if (ref.driftMs!=0 && elapsed<hourMs) {
        u64 magnitude=(ref.driftMs<0)?(u64)(-ref.driftMs):(u64)ref.driftMs;
        adjustment=divideU64(magnitude*(hourMs-elapsed),hourMs,0);
        if (ref.driftMs>0) elapsed+=adjustment;
        else elapsed=(adjustment<elapsed)?elapsed-adjustment:0;
    }
    return ref.valueMs+elapsed;
}

static int leapYear(u32 year)
{
    u64 r100,r400; if (year&3u) return 0;
    (void)divideU64(year,100u,&r100); (void)divideU64(year,400u,&r400);
    return r100!=0u || r400==0u;
}

static int packFutureTicketDate(u32 packed[2])
{
    static const u8 monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    u64 remainder,cycles,days,future=currentSystemTimeMs()+999ull*86400000ull;
    u32 year=1900,month=1,day,hour,minute,second,span;
    days=divideU64(future,86400000ull,&remainder);
    /* Skip whole Gregorian 400-year cycles so malformed timestamps can never
       turn the year conversion into an unbounded loop. */
    /* 先跳过完整的公历 400 年周期，避免异常时间戳造成无界的逐年循环。 */
    cycles=divideU64(days,146097u,&days);
    if (cycles>20u) return 0;
    year+=(u32)cycles*400u;
    while (days>=(u32)(365+leapYear(year))) { days-=365u+leapYear(year); year++; }
    while (month<=12u) {
        span=monthDays[month-1u]+((month==2u)?leapYear(year):0);
        if (days<span) break;
        days-=span; month++;
    }
    day=(u32)days+1u; hour=(u32)divideU64(remainder,3600000u,&remainder);
    minute=(u32)divideU64(remainder,60000u,&remainder); second=(u32)divideU64(remainder,1000u,0);
    packed[0]=(second&63u)|((minute&63u)<<6)|((hour&31u)<<12)|((day&31u)<<17)|
        ((month&15u)<<22)|(year<<26); packed[1]=(year>>6)&0xFFu; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_NetworkUpdate(u8 *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,1500u)) return 0;
    u8 *flow=*(u8 **)(state+4); if (flow) { u8 *network=*(u8 **)(flow+0x24); if (network) network[0x11]=1; }
    state[0x30]=3; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_DisconnectUpdate(u8 *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,1500u)) return 0;
    u8 *flow=*(u8 **)(state+4); if (flow) { u8 *network=*(u8 **)(flow+0x24); if (network) network[0x11]=0; }
    state[0x30]=3; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_TicketUpdate(u8 *state)
{
    u8 *shared=*(u8 **)(state+0x28); u32 *p=(u32 *)shared;
    if (!shared) { state[0x30]=2; return 1; }
    /* Preserve account data and refresh only the offline ticket fields. */
    /* 保留账户数据，仅刷新离线票据字段。 */
    if (!packFutureTicketDate(&p[0x28/4])) { state[0x30]=2; return 1; }
    p[0x3C/4]=999; p[0x40/4]=999u*24u;
    shared[0x44]=1; shared[0x335]=0; state[0x30]=3; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_RemoteCheckUpdate(u8 *state)
{
    state[0x30]=8; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_EligibilityUpdate(u8 *state)
{
    int loaded=loadLocalBank(state);
    if (loaded<0) state[0x30]=5;
    else if (loaded>0) {
        /* Transfer-Box occupancy is an internal native message path, not outer
           result 0x0F. Resume at substate 7 so message 5 is displayed and the
           stock acknowledgement path returns result 0x10. */
        /* 传送盒占用属于原版内部消息路径，并非外层结果 0x0F。转入 substate 7，
           由原版显示消息 5，并在确认后通过结果 0x10 正常返回。 */
        *(u32 *)(state+0x10)=7;
        return 0;
    }
    else state[0x30]=4;
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_NoTransferUpdate(u8 *state)
{
    /* Preserve the stock connection-screen timer, but never create or wait for
       the final remote no-transfer transaction. */
    /* 保留原版连接界面的计时，但不创建或等待最后的不传送远端事务。 */
    if (!StateTimer_HasElapsed(state,1500u)) return 0;
    state[0x30]=3; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_SaveDisplayDelayUpdate(u8 *state)
{
    /* The local stage write is synchronous. Reuse the stock callback byte as
       a private marker and keep the save message alive for at least two seconds. */
    /* 本地暂存写入是同步的。复用原版回调字节作为私有标记，让保存提示至少显示两秒。 */
    if (!state[0x44]) {
        StateTimer_Reset(state);
        state[0x44]=1;
        return 0;
    }
    if (!StateTimer_HasElapsed(state,2000u)) return 0;
    state[0x44]=0;
    *(u32 *)(state+0x10)=3;
    return 1;
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
    u64 archive=0; s32 r; (void)remote; (void)transaction; (void)zero;
    if (openArchive(&archive)) return 0;
    r=pathCommand(FSUSER_CMD_DELETE_FILE,archive,tempPath,sizeof(tempPath));
    closeArchive(archive);
    /* Repeating rollback after the temp file is gone is idempotent. */
    /* 临时文件已不存在时，重复回滚仍视为成功。 */
    return !r || resultIsNotFound(r);
}
