#include "fs_system/diagnostic_log.h"

#if FS_LOCAL_IO_TRACE
#include "fs_system/fs_helpers.h"
#include "fakeServer/fake_servers.h"
#include "fakeServer/sd_adapter.h"
#include "fake_server_context.h"
#include "system_time.h"
#include <string.h>

enum { TRACE_COUNT=256, TRACE_LIMIT=0x100000, TRACE_MAGIC=0x31474f4c };
typedef struct TraceEntry {
    uint64_t tick;
    uint32_t sequence;
    const char *stage;
    int32_t result;
    uint32_t a,b,c;
} TraceEntry;
typedef struct TraceState {
    volatile uint32_t lock,flushing;
    uint32_t magic,sequence,flushed,sessionWritten,disabled;
    int32_t sinkResult;
    TraceEntry entries[TRACE_COUNT];
} TraceState;

/* Reuse unused bytes after FsServer, separately from the final ticket marker.
   No extra mapped pages, heap allocation or worker stack reservation. */
/* 复用 FsServer 后的空闲区，避开末尾票务标记；不增映射页、不申请堆内存。 */
#define TRACE_OFFSET ((sizeof(FsServer)+7u)&~7u)
_Static_assert(TRACE_OFFSET+sizeof(TraceState)<FAKE_SERVER_CONTEXT_SIZE-1u,
    "diagnostic ring overlaps service data/ticket marker");
_Static_assert(sizeof(TraceEntry)==32,"ARM diagnostic ring ABI");
static TraceState *state(void)
{ return (TraceState *)(FAKE_SERVER_CONTEXT_ADDRESS+TRACE_OFFSET); }

static uint32_t logEnabled __attribute__((section(".data.diagnostic_log")))=0;

/* Bootstrap reads use the same fixed-schema repair as service reads, without
   selecting a user. Logging remains disabled during that read/publication. */
/* 启动读取也走同一基础配置修复，不选择账户；读取及发布期间日志仍保持关闭。 */

void FsDiag_Startup(void)
{
    static uint32_t started __attribute__((section(".data.diagnostic_log")))=0;
    if (__sync_lock_test_and_set(&started,1)) return;
    uint32_t enabled=0;int loaded=FsSd_ReadLogSetting(&enabled);
    __atomic_store_n(&logEnabled,enabled,__ATOMIC_RELEASE);
    if(!__atomic_load_n(&logEnabled,__ATOMIC_ACQUIRE))return;
    FsDiag_Record("process.start",0,FAKE_SERVER_CLIENT_OWNER,0,0);
    if(loaded!=FS_OK)FsDiag_Record("configuration.startup",loaded,0,0,0);
    FsDiag_Flush();
}

void FsDiag_Record(const char *stage,int32_t result,uint32_t a,uint32_t b,uint32_t c)
{
    if(!__atomic_load_n(&logEnabled,__ATOMIC_ACQUIRE))return;
    TraceState *s=state();
    if (!stage || __atomic_load_n(&s->flushing,__ATOMIC_ACQUIRE) ||
        __sync_lock_test_and_set(&s->lock,1)) return;
    if (s->magic!=TRACE_MAGIC) {
        s->magic=TRACE_MAGIC;s->sequence=s->flushed=s->sessionWritten=s->disabled=0;
        s->sinkResult=0;
    }
    if (s->sequence!=UINT32_MAX) {
        uint32_t seq=++s->sequence;
        s->entries[(seq-1u)&(TRACE_COUNT-1u)]=(TraceEntry){
            SystemTime_GetMonotonicTicks(),seq,stage,result,a,b,c};
    }
    __sync_lock_release(&s->lock);
}

void FsDiag_Checkpoint(const char *stage,int32_t result,uint32_t a,uint32_t b,uint32_t c)
{
    if(!__atomic_load_n(&logEnabled,__ATOMIC_ACQUIRE))return;
    FsDiag_Record(stage,result,a,b,c);
    TraceState *s=state();
    /* Batch ordinary filesystem traces; request boundaries flush explicitly. */
    /* 普通文件操作先合批；请求开始和结束处另行明确刷新。 */
    if (s->sequence-s->flushed>=32u) FsDiag_Flush();
}

/* Classify a path without retaining any user-controlled text. */
/* 路径只转成固定类别，不保留玩家输入的文本。 */
uint32_t FsDiag_Path(uint32_t type,const void *data,uint32_t size)
{
    if(!__atomic_load_n(&logEnabled,__ATOMIC_RELAXED))return 0;
    if (!data || !size || (type!=FS_PATH_TYPE_ASCII && type!=FS_PATH_TYPE_UTF16)) return 0;
    uint32_t units=type==FS_PATH_TYPE_UTF16?size/2u:size;
    if (units>512u) return 0xffu;
    char name[64];uint32_t used=0;
    for (uint32_t i=0;i<units;i++) {
        uint32_t c=type==FS_PATH_TYPE_UTF16?((const uint16_t *)data)[i]:((const uint8_t *)data)[i];
        if (!c) break;
        if (c=='/') used=0;
        else if (used+1u<sizeof(name)) name[used++]=c<128u?(char)c:'?';
    }
    name[used]=0;
    static const char *const names[]={
        "3ds","Bank","client","fakeServers","users","shared",
        "bankdata.bin","bankdata.tmp","bankdata.bak",
        "transaction.bin","transaction.tmp","transaction.bak",
        "sav.bin","sav.tmp","sav.bak","client.conf","client.tmp","client.bak",
        "server.conf","server.tmp","server.bak","user.conf","user.tmp","user.bak",
        "gift.conf","checkPokemon.conf","checkName.conf"};
    for (uint32_t i=0;i<sizeof(names)/sizeof(*names);i++) if (!strcmp(name,names[i])) return i+1u;
    if (!strncmp(name,"nnid_",5)) return 0x80u;
    if (!strncmp(name,"pnid_",5)) return 0x81u;
    if (!strncmp(name,"localid_",8)) return 0x82u;
    return 0xffu;
}

static char *hex(char *out,uint32_t value)
{
    static const char digits[]="0123456789ABCDEF";
    for (uint32_t i=0;i<8;i++) *out++=digits[(value>>(28u-i*4u))&15u];
    return out;
}
static uint32_t line(char *out,const TraceEntry *entry)
{
    char *p=hex(out,entry->sequence);*p++=' ';
    p=hex(p,(uint32_t)(entry->tick>>32));p=hex(p,(uint32_t)entry->tick);*p++=' ';
    for (uint32_t i=0;entry->stage[i] && i<40u;i++) *p++=entry->stage[i];
    *p++=' ';p=hex(p,(uint32_t)entry->result);
    *p++=' ';p=hex(p,entry->a);*p++=' ';p=hex(p,entry->b);*p++=' ';p=hex(p,entry->c);*p++='\n';
    return (uint32_t)(p-out);
}
static int append(uint32_t handle,uint64_t *offset,const void *bytes,uint32_t size,int32_t *error)
{
    if (*offset>TRACE_LIMIT || size>TRACE_LIMIT-*offset) {*error=-0x7001;return 0;}
    u32 nativeHandle=handle,actual=0;
    int32_t result=FSFILE_Write(&nativeHandle,&actual,*offset,bytes,size,WRITE_FLUSH);
    if (result || actual!=size) {*error=result?result:-0x7002;return 0;}
    *offset+=size;return 1;
}

static int32_t createLogParents(void)
{
    u64 archive=0;
    int32_t error=openArchive(&archive);
    if (error) return error;
    static const char *const parents[]={"/3ds","/3ds/Bank","/3ds/Bank/client","/3ds/Bank/client/log"};
    for (uint32_t i=0;i<sizeof(parents)/sizeof(*parents);i++) {
        uint32_t size=(uint32_t)strlen(parents[i])+1u;
        volatile u32 *command=fsCommandBuffer();
        command[0]=FSUSER_CMD_CREATE_DIRECTORY;command[1]=0;
        command[2]=(u32)archive;command[3]=(u32)(archive>>32);
        command[4]=FS_PATH_TYPE_ASCII;command[5]=size;command[6]=0;
        command[7]=(size<<14)|2u;command[8]=(u32)parents[i];
        /* Existing parents are harmless; the final file open verifies access. */
        /* 父目录已存在不影响操作；最终打开日志文件时确认访问是否成功。 */
        (void)fsSync(*FSUSER_HandleSlot);
    }
    return closeArchive(archive);
}

void FsDiag_Flush(void)
{
    if(!__atomic_load_n(&logEnabled,__ATOMIC_ACQUIRE))return;
    TraceState *s=state();
    if (s->magic!=TRACE_MAGIC || s->disabled || s->flushed==s->sequence ||
        __sync_lock_test_and_set(&s->flushing,1)) return;
    u32 handle=0;
    uint32_t end=s->sequence,first=s->flushed+1u;
    uint64_t offset=0;int32_t error=0;int ok=0;
    const char *path=FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_BANK?
        "/3ds/Bank/client/log/bank.log":"/3ds/Bank/client/log/mover.log";
    static const char header[]="bank-mover local I/O trace v1\n"
        "# hex fields: sequence ticks stage result arg0 arg1 arg2\n";
    /* Truncate only this title's log, once per process. Account selection and
       returning to the title screen never discard the current process log. */
    /* 每次进程启动只清空本软件日志一次；切换用户或返回标题不清空本次日志。 */
    error=FSUSER_OpenFileDirectly(FSUSER_HandleSlot,&handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,"",1,FS_PATH_TYPE_ASCII,path,(uint32_t)strlen(path)+1u,
        OPEN_READ|OPEN_WRITE|OPEN_CREATE,0);
    if (resultIsNotFound(error)) {
        error=createLogParents();if(error)goto finish;
        /* The retry is bounded even when directory creation was denied. */
        /* 建目录被拒绝时也只重试一次，不在错误的卡上无限循环。 */
        error=FSUSER_OpenFileDirectly(FSUSER_HandleSlot,&handle,0,FS_ARCHIVE_ID_SDMC,
            FS_PATH_TYPE_EMPTY,"",1,FS_PATH_TYPE_ASCII,path,(uint32_t)strlen(path)+1u,
            OPEN_READ|OPEN_WRITE|OPEN_CREATE,0);
    }
    if (error) goto finish;
    if (!s->sessionWritten) {
        error=setSize(handle,0);if(error)goto finish;
    }
    error=FSFILE_GetSize(&handle,&offset);if(error)goto finish;
    if (!offset) {
        if (!append(handle,&offset,header,sizeof(header)-1u,&error))goto finish;
    } else {
        char existing[sizeof(header)-1u];u32 actual=0;
        if (offset<sizeof(existing) || offset>TRACE_LIMIT) {error=-0x7003;goto finish;}
        error=FSFILE_Read(&handle,&actual,0,existing,sizeof(existing));
        if (error || actual!=sizeof(existing) || memcmp(existing,header,sizeof(existing))) {
            if(!error)error=-0x7004;
            goto finish;
        }
    }
    if (!s->sessionWritten) {
        const char *session=FAKE_SERVER_CLIENT_OWNER==FS_TRANSACTION_OWNER_BANK?
            "# new process: Bank\n":"# new process: Mover\n";
        if (!append(handle,&offset,session,(uint32_t)strlen(session),&error))goto finish;
    }
    if (end-first>=TRACE_COUNT) {
        static const char lost[]="# ring overrun: earlier unflushed entries omitted\n";
        first=end-TRACE_COUNT+1u;
        if (!append(handle,&offset,lost,sizeof(lost)-1u,&error))goto finish;
    }
    char buffer[1024];uint32_t used=0;
    for(uint32_t seq=first;seq<=end;seq++) {
        if (sizeof(buffer)-used<128u) {
            if (!append(handle,&offset,buffer,used,&error))goto finish;
            used=0;
        }
        used+=line(buffer+used,&s->entries[(seq-1u)&(TRACE_COUNT-1u)]);
        if(seq==UINT32_MAX)break;
    }
    if (used && !append(handle,&offset,buffer,used,&error))goto finish;
    ok=1;
finish:
    if(handle) {
        u32 local=handle;
        int32_t closed=FSFILE_Close(&local);
        int32_t released=fsCloseHandle(handle);
        if(closed || released){if(!error)error=closed?closed:released;ok=0;}
    }
    s->sinkResult=error;
    if(ok){s->flushed=end;s->sessionWritten=1;}else s->disabled=1;
    __sync_lock_release(&s->flushing);
}
#endif
