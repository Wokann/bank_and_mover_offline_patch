/* Combined Bank patch local-file implementation. */
/* Bank 合并补丁的本地文件实现。 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

enum {
    BANK_FILE_SIZE = 0xBB518,
    FS_ARCHIVE_ID_SDMC = 9,
    FS_PATH_TYPE_EMPTY = 1,
    FS_PATH_TYPE_ASCII = 3,
    OPEN_READ = 1,
    OPEN_WRITE = 2,
    OPEN_CREATE = 4,
    WRITE_FLUSH = 1,
    RESULT_SUMMARY_SHIFT = 21,
    RESULT_SUMMARY_MASK = 0x3F,
    RESULT_SUMMARY_NOT_FOUND = 4
};

#define FSUSER_HandleSlot ((volatile u32 *)0x00390100u)
#define FSUSER_OpenFileDirectly ((FSUSER_OpenFileDirectlyFn)0x0022D338u)
#define FSFILE_Read ((FSFILE_ReadFn)0x001658C8u)
#define FSFILE_Close ((FSFILE_CloseFn)0x00165920u)
#define FSFILE_Write ((FSFILE_WriteFn)0x0016594Cu)
#define FSFILE_GetSize ((FSFILE_GetSizeFn)0x001659ACu)
#define BankFile_DeriveFlowMetadata ((BankFile_DeriveFlowMetadataFn)0x001D5EC0u)
#define StateTimer_HasElapsed ((StateTimer_HasElapsedFn)0x001D5BB0u)
#define StateTimer_Reset ((StateTimer_ResetFn)0x00229DB4u)
#define WaitingUi_Hide ((WaitingUi_HideFn)0x001D5F50u)
#define BankFile_Vtable 0x003626FCu
#define FSUSER_CMD_OPEN_ARCHIVE 0x080C00C2u
#define FSUSER_CMD_DELETE_FILE 0x08040142u
#define FSUSER_CMD_RENAME_FILE 0x08050244u
#define FSUSER_CMD_CREATE_DIRECTORY 0x08090182u
#define FSUSER_CMD_CLOSE_ARCHIVE 0x080E0080u
#define FSFILE_CMD_SET_SIZE 0x08050080u

typedef s32 (*FSUSER_OpenFileDirectlyFn)(volatile u32 *, u32 *, u32, u32, u32, const void *, u32,
    u32, const void *, u32, u32, u32);
typedef s32 (*FSFILE_ReadFn)(u32 *, u32 *, u64, void *, u32);
typedef s32 (*FSFILE_WriteFn)(u32 *, u32 *, u64, const void *, u32, u32);
typedef s32 (*FSFILE_CloseFn)(u32 *);
typedef s32 (*FSFILE_GetSizeFn)(u32 *, u64 *);
typedef u32 (*BankFile_DeriveFlowMetadataFn)(void *);
typedef int (*StateTimer_HasElapsedFn)(void *, u32);
typedef void (*StateTimer_ResetFn)(void *);
typedef void (*WaitingUi_HideFn)(void *);

static const char emptyPath[1] = {0};
static const char directory3ds[] = "/3ds";
static const char directoryBank[] = "/3ds/Bank";
static const char bankPath[] = "/3ds/Bank/bankdata.bin";
static const char tempPath[] = "/3ds/Bank/bankdata.tmp";
static const char backupPath[] = "/3ds/Bank/bankdata.bak";
#define BROKEN_BANK_PATH ((const char *)0x00313F80u)
#define BROKEN_BACKUP_PATH ((const char *)0x00313FA0u)
#define BROKEN_BANK_PATH_SIZE 29u
#define BROKEN_BACKUP_PATH_SIZE 29u

static volatile u32 *commandBuffer(void)
{
    u32 tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));
    return (volatile u32 *)(tls + 0x80u);
}

typedef struct {
    u64 valueMs;
    u64 valueTick;
    s64 systemClockHz;
    s64 driftMs;
} SystemTimeReference;

static u64 systemTick(void)
{
    register u32 low __asm__("r0");
    register u32 high __asm__("r1");
    __asm__ volatile("svc 0x28" : "=r"(low), "=r"(high) : : "r2", "r3", "r12", "memory", "cc");
    return (u64)low | ((u64)high << 32);
}

static void dataMemoryBarrier(void)
{
    u32 zero=0;
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 5" : : "r"(zero) : "memory");
}

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

static u64 currentSystemTimeMs(void)
{
    volatile u32 *counter=(volatile u32 *)0x1FF81000u;
    volatile SystemTimeReference *references=(volatile SystemTimeReference *)0x1FF81020u;
    SystemTimeReference reference; u32 before,after; u64 elapsed,remainder,seconds,adjustment;
    const u64 hourMs=3600000u;
    do {
        before=*counter; reference=references[before&1u];
        dataMemoryBarrier();
        after=*counter;
    } while (before!=after);
    elapsed=systemTick()-reference.valueTick;
    if (reference.systemClockHz>0) {
        seconds=divideU64(elapsed,(u64)reference.systemClockHz,&remainder);
        elapsed=seconds*1000u+divideU64(remainder*1000u,(u64)reference.systemClockHz,0);
    } else {
        elapsed=0;
    }
    /* Apply PTM's measured drift gradually over one hour, matching the system
       time model used by libctru. */
    /* 在一小时内逐步应用 PTM 测得的漂移量，与 libctru 的系统时间模型一致。 */
    if (reference.driftMs!=0 && elapsed<hourMs) {
        u64 magnitude=(reference.driftMs<0)?(u64)(-reference.driftMs):(u64)reference.driftMs;
        adjustment=divideU64(magnitude*(hourMs-elapsed),hourMs,0);
        if (reference.driftMs>0) elapsed+=adjustment;
        else elapsed=(adjustment<elapsed)?elapsed-adjustment:0;
    }
    return reference.valueMs+elapsed;
}

static int leapYear(u32 year)
{
    u64 remainder100,remainder400;
    if ((year&3u)!=0u) return 0;
    (void)divideU64(year,100u,&remainder100);
    (void)divideU64(year,400u,&remainder400);
    return remainder100!=0u || remainder400==0u;
}

static int packCurrentDate(u32 packed[2])
{
    static const u8 monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    u64 remainder,cycles; u64 days; u32 year=1900,month=1,day,hour,minute,second,span;
    u64 current=currentSystemTimeMs();
    days=divideU64(current,86400000ull,&remainder);
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
    day=(u32)days+1u;
    hour=(u32)divideU64(remainder,3600000ull,&remainder);
    minute=(u32)divideU64(remainder,60000ull,&remainder);
    second=(u32)divideU64(remainder,1000ull,0);
    packed[0]=(second&0x3Fu)|((minute&0x3Fu)<<6)|((hour&0x1Fu)<<12)|
        ((day&0x1Fu)<<17)|((month&0x0Fu)<<22)|(year<<26);
    packed[1]=(year>>6)&0xFFu;
    return 1;
}

static s32 sync(u32 handle)
{
    register u32 r0 __asm__("r0") = handle;
    __asm__ volatile("svc 0x32" : "+r"(r0) : : "r1", "r2", "r3", "r12", "memory", "cc");
    return (s32)r0;
}

static s32 openArchive(u64 *archive)
{
    volatile u32 *c = commandBuffer();
    s32 r;
    c[0]=FSUSER_CMD_OPEN_ARCHIVE; c[1]=FS_ARCHIVE_ID_SDMC; c[2]=FS_PATH_TYPE_EMPTY; c[3]=1;
    c[4]=(1u<<14)|2u; c[5]=(u32)emptyPath;
    r=sync(*FSUSER_HandleSlot); if (r) return r; r=(s32)c[1];
    if (!r) *archive=(u64)c[2]|((u64)c[3]<<32);
    return r;
}

static void closeArchive(u64 a)
{
    volatile u32 *c=commandBuffer();
    c[0]=FSUSER_CMD_CLOSE_ARCHIVE; c[1]=(u32)a; c[2]=(u32)(a>>32); (void)sync(*FSUSER_HandleSlot);
}

static s32 pathCommand(u32 command, u64 a, const char *path, u32 size)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=command; c[1]=0; c[2]=(u32)a; c[3]=(u32)(a>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=size; c[6]=(size<<14)|2u; c[7]=(u32)path;
    r=sync(*FSUSER_HandleSlot); return r ? r : (s32)c[1];
}

static void createDirectory(u64 a,const char *path,u32 size)
{
    volatile u32 *c=commandBuffer();
    c[0]=FSUSER_CMD_CREATE_DIRECTORY; c[1]=0; c[2]=(u32)a; c[3]=(u32)(a>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=size; c[6]=0; c[7]=(size<<14)|2u; c[8]=(u32)path;
    (void)sync(*FSUSER_HandleSlot);
}

static s32 renamePath(u64 a,const char *from,u32 fromSize,const char *to,u32 toSize)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=FSUSER_CMD_RENAME_FILE; c[1]=0; c[2]=(u32)a; c[3]=(u32)(a>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=fromSize; c[6]=(u32)a; c[7]=(u32)(a>>32);
    c[8]=FS_PATH_TYPE_ASCII; c[9]=toSize; c[10]=(fromSize<<14)|0x402u; c[11]=(u32)from;
    c[12]=(toSize<<14)|0x802u; c[13]=(u32)to;
    r=sync(*FSUSER_HandleSlot); return r ? r : (s32)c[1];
}

static s32 setSize(u32 handle,u64 size)
{
    volatile u32 *c=commandBuffer(); s32 r;
    c[0]=FSFILE_CMD_SET_SIZE; c[1]=(u32)size; c[2]=(u32)(size>>32);
    r=sync(handle); return r ? r : (s32)c[1];
}

static void ensureDirectories(void)
{
    u64 a=0; if (openArchive(&a)) return;
    createDirectory(a,directory3ds,sizeof(directory3ds));
    createDirectory(a,directoryBank,sizeof(directoryBank));
    closeArchive(a);
}

static s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle)
{
    return FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,emptyPath,1,FS_PATH_TYPE_ASCII,path,pathSize,flags,0);
}

static int validHeader(const u8 header[4])
{
    return (u16)(header[0]|((u16)header[1]<<8))==2 &&
           (u16)(header[2]|((u16)header[3]<<8))==100;
}

static int resultIsNotFound(s32 result)
{
    return (((u32)result>>RESULT_SUMMARY_SHIFT)&RESULT_SUMMARY_MASK)==
        RESULT_SUMMARY_NOT_FOUND;
}

static int writeCompleteFile(const char *path,u32 pathSize,const void *data)
{
    u32 h=0,n=0; s32 r,closeResult=0;
    ensureDirectories();
    r=openFile(path,pathSize,OPEN_READ|OPEN_WRITE|OPEN_CREATE,&h);
    if (!r) r=setSize(h,BANK_FILE_SIZE);
    if (!r) r=FSFILE_Write(&h,&n,0,data,BANK_FILE_SIZE,WRITE_FLUSH);
    if (h) closeResult=FSFILE_Close(&h);
    return !r && !closeResult && n==BANK_FILE_SIZE;
}

static int writeTemporary(const void *data)
{
    return writeCompleteFile(tempPath,sizeof(tempPath),data);
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
    u64 a=0;
    int complete;

    /* First use writes only the final bankdata.bin. If a size, write, short-write,
       flush, or close check fails, remove the incomplete new file. */
    /* 首次使用只写最终的 bankdata.bin。若长度设置、写入、短写、刷新或关闭检查
       失败，则删除不完整的新文件。 */
    complete=writeCompleteFile(bankPath,sizeof(bankPath),data);
    if (!complete && !openArchive(&a)) {
        (void)pathCommand(FSUSER_CMD_DELETE_FILE,a,bankPath,sizeof(bankPath));
        closeArchive(a);
    }
    return complete;
}

enum {
    LOCAL_FILE_INVALID = -1,
    LOCAL_FILE_MISSING = 0,
    LOCAL_FILE_VALID = 1
};

static int inspectBankFile(const char *path,u32 pathSize)
{
    u8 header[4];
    u32 h=0,n=0; u64 size=0; s32 r,closeResult=0;

    r=openFile(path,pathSize,OPEN_READ,&h);
    if (r) return resultIsNotFound(r)?LOCAL_FILE_MISSING:LOCAL_FILE_INVALID;
    r=FSFILE_GetSize(&h,&size);
    if (!r && size==BANK_FILE_SIZE) r=FSFILE_Read(&h,&n,0x15Cu,header,sizeof(header));
    else if (!r) r=-1;
    closeResult=FSFILE_Close(&h);
    if (!r && !closeResult && n==sizeof(header) && validHeader(header)) return LOCAL_FILE_VALID;
    return LOCAL_FILE_INVALID;
}

static int preserveBrokenFile(const char *source,u32 sourceSize,
    const char *destination,u32 destinationSize)
{
    u8 buffer[0x400];
    u32 sourceHandle=0,destinationHandle=0,readCount=0,writeCount=0;
    u32 size32=0,offset=0,chunk;
    u64 size=0,archive=0;
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
    if (!openArchive(&archive)) {
        (void)pathCommand(FSUSER_CMD_DELETE_FILE,archive,destination,destinationSize);
        closeArchive(archive);
    }
    return 0;
}

static int restoreBankFromBackup(u8 *state)
{
    u8 *flow=*(u8 **)(state+8);
    u8 *object=flow?*(u8 **)(flow+0xCC):0;
    u32 h=0,n=0; u64 size=0; s32 r,closeResult=0;

    if (!object || *(u32 *)object!=BankFile_Vtable) return 0;
    r=openFile(backupPath,sizeof(backupPath),OPEN_READ,&h);
    if (!r) r=FSFILE_GetSize(&h,&size);
    if (!r && size==BANK_FILE_SIZE) r=FSFILE_Read(&h,&n,0,object+8,BANK_FILE_SIZE);
    else if (!r) r=-1;
    if (h) closeResult=FSFILE_Close(&h);
    if (r || closeResult || n!=BANK_FILE_SIZE || !validHeader(object+8+0x15C)) return 0;

    /* Preserve bankdata.bak and create a checked byte-for-byte bankdata.bin copy. */
    /* 保留 bankdata.bak，并创建经过完整检查、逐字节一致的 bankdata.bin 副本。 */
    return writeInitialBank(object+8);
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_NetworkUpdate(u8 *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,1500u)) return 0;
    u8 *flow=*(u8 **)(state+4); if (flow) { u8 *network=*(u8 **)(flow+0x24); if (network) network[0x11]=1; }
    state[0x30]=4; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_PostSelectionConnectionUpdate(u8 *state)
{
    /* This stock screen represents the server transaction after game checking.
       Its initializer does not start a timer, so start a local 2-second delay
       before reporting completion without a remote job. */
    /* 此原版界面表示游戏检查后的服务器事务。其初始化函数不会启动计时器，
       因此先启动本地 2 秒延时，再在不创建远端作业的情况下报告完成。 */
    if (!state[0x61]) {
        StateTimer_Reset(state);
        state[0x61]=1;
        return 0;
    }
    if (!StateTimer_HasElapsed(state,2000u)) return 0;
    state[0x61]=0;

    /* Report completion through the stock state transition. Its native exit
       path owns the wait UI, animation and sound cleanup. */
    /* 通过原版状态转换报告完成；等待界面、动画及声音均由其原生退出路径负责清理。 */
    state[0x30]=4; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_DisconnectUpdate(u8 *state)
{
    /* Retain the native timer started by the state initializer. */
    /* 沿用状态初始化函数启动的原版计时器。 */
    if (!StateTimer_HasElapsed(state,1500u)) return 0;
    u8 *flow=*(u8 **)(state+4); if (flow) { u8 *network=*(u8 **)(flow+0x24); if (network) network[0x11]=0; }
    state[0x30]=4; return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_InitialRemoteRecordUpdate(u8 *state)
{
    u8 *shared=*(u8 **)(state+0x28);
    int bankStatus,backupStatus;

    if (!shared) { state[0x30]=3; return 1; }
    bankStatus=inspectBankFile(bankPath,sizeof(bankPath));
    if (bankStatus==LOCAL_FILE_VALID) {
        /* ASCII '4' is the stock existing-record mode; '5' is first use. */
        /* ASCII“4”是原版既有记录模式；“5”表示首次使用。 */
        shared[0x20]='4';
        *(u16 *)(shared+0x48)=0;
        shared[0x4C]=0; shared[0x4D]=0; shared[0x4E]=0;
        state[0x30]=4;
        return 1;
    }

    /* A missing or invalid primary always checks the backup first. A valid
       backup must be restored; a missing or invalid backup selects stock
       first-use creation instead. */
    /* 主文件缺失或无效时一律优先检查备份。有效备份必须恢复；备份缺失或
       无效时则进入原版首次创建。 */
    backupStatus=inspectBankFile(backupPath,sizeof(backupPath));
    if (backupStatus==LOCAL_FILE_VALID) {
        if (restoreBankFromBackup(state)) {
            shared[0x20]='4';
            *(u16 *)(shared+0x48)=0;
            shared[0x4C]=0; shared[0x4D]=0; shared[0x4E]=0;
            state[0x30]=4;
        } else {
            state[0x30]=3;
        }
    } else {
        int attempt;
        /* Preserve only files that exist but cannot be recognized. After the
           initial failure, retry each copy up to three times, but do not block
           stock first use if all four attempts fail. The original invalid
           files are never removed. */
        /* 只保全实际存在但无法识别的文件。初次失败后，每个副本最多重试三次；即使四次尝试全部失败，
           也不阻止原版首次创建。原始无效文件始终不会被删除。 */
        if (bankStatus==LOCAL_FILE_INVALID) {
            for (attempt=0;attempt<4;attempt++) {
                if (preserveBrokenFile(bankPath,sizeof(bankPath),
                    BROKEN_BANK_PATH,BROKEN_BANK_PATH_SIZE)) break;
            }
        }
        if (backupStatus==LOCAL_FILE_INVALID) {
            for (attempt=0;attempt<4;attempt++) {
                if (preserveBrokenFile(backupPath,sizeof(backupPath),
                    BROKEN_BACKUP_PATH,BROKEN_BACKUP_PATH_SIZE)) break;
            }
        }
        shared[0x20]='5';
        *(u16 *)(shared+0x48)=0;
        shared[0x4C]=0; shared[0x4D]=0; shared[0x4E]=0;
        state[0x30]=4;
    }
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_OptionalRewardBypassUpdate(u8 *state)
{
    u8 *shared=*(u8 **)(state+0x28); u32 *p=(u32 *)shared;
    if (!shared) { state[0x30]=3; return 1; }
    /* This state normally obtains a trusted current timestamp and entitlement
       values remotely. Supply the current console time for the stock local
       mileage calculation while retaining a 999-day offline entitlement. */
    /* 此状态原本会从远端取得可信当前时间和使用权数值。为原版本地里程计算提供
       主机当前时间，同时保留 999 天离线使用权。 */
    if (!packCurrentDate(&p[0x28/4])) { state[0x30]=3; return 1; }
    p[0x3C/4]=999; p[0x40/4]=999u*24u; shared[0x44]=1; shared[0x4F]=0;
    /* State 9 itself checks mode '5'. Existing mode '4' skips creation and
       reaches the feature menu; missing mode '5' runs native initialization. */
    /* state 9 自行检查模式“5”。既有模式“4”跳过创建并进入功能选单；
       缺失模式“5”则执行原版初始化。 */
    state[0x30]=4;
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_LoadBankData(u8 *state)
{
    u8 *flow=*(u8 **)(state+8);
    u8 *object=flow?*(u8 **)(flow+0xCC):0;
    u32 h=0,n=0; u64 size=0; s32 r,closeResult=0;

    if (!object || *(u32 *)object!=BankFile_Vtable) return 0;
    r=openFile(bankPath,sizeof(bankPath),OPEN_READ,&h);
    if (!r) r=FSFILE_GetSize(&h,&size);
    if (!r && size==BANK_FILE_SIZE) r=FSFILE_Read(&h,&n,0,object+8,BANK_FILE_SIZE);
    else if (!r) r=-1;
    if (h) closeResult=FSFILE_Close(&h);

    if (!r && !closeResult && n==BANK_FILE_SIZE && validHeader(object+8+0x15C)) {
        void *metadata=*(void **)(object+0xBB520);
        /* Rebuild the flow metadata value set by the stock download callback. */
        /* 重建原版完整数据下载回调写入流程对象的元数据值。 */
        *(u32 *)(flow+0xF8)=BankFile_DeriveFlowMetadata(metadata);
        return 1;
    }
    return 0;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_SaveDisplayDelayUpdate(u8 *state)
{
    /* The local write is synchronous. Reuse the stock callback byte as a
       private marker and keep the save message alive for at least two seconds. */
    /* 本地写入是同步的。复用原版回调字节作为私有标记，让保存提示至少显示两秒。 */
    if (!state[0x48]) {
        StateTimer_Reset(state);
        state[0x48]=1;
        return 0;
    }
    if (!StateTimer_HasElapsed(state,2000u)) return 0;
    state[0x48]=0;
    *(u32 *)(state+0x10)=3;
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_CreateInitial(void *remote,const void *data,u32 size)
{
    (void)remote;
    return size==BANK_FILE_SIZE && writeInitialBank(data);
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
    u64 a=0; s32 r; (void)remote; (void)transaction; (void)zero;
    if (openArchive(&a)) return 0;
    r=pathCommand(FSUSER_CMD_DELETE_FILE,a,tempPath,sizeof(tempPath));
    closeArchive(a);
    /* Repeating rollback after the temp file is gone is idempotent. */
    /* 临时文件已不存在时，重复回滚仍视为成功。 */
    return !r || resultIsNotFound(r);
}
