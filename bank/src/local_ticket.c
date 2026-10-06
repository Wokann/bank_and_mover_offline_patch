#include "local_ticket.h"
#include "code_expansion.h"
#include "fs_helpers.h"
#include "local_mileage.h"

/* Add entitlement days without changing the reference time used for mileage.
   The input is the validated date supplied by LocalMileage_GetCurrentDate. */
/* 只增加使用期限，不改变里程使用的基准时间。输入为主机日期转换函数提供的有效
   日期；按公历月长进位，保留时、分、秒。 */
static int setExpiryDate(BankTicketJobView *job)
{
    static const u8 monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    u32 date=job->currentDate[0];
    u32 year=(date>>26)|(job->currentDate[1]<<6);
    u32 month=(date>>22)&0x0Fu;
    u32 day=((date>>17)&0x1Fu)+BANK_TICKET_ENTITLEMENT_DAYS;
    u32 span,cycle;
    if (year<BANK_TICKET_MIN_YEAR || year>BANK_TICKET_MAX_YEAR) return 0;
    for (;;) {
        span=monthDays[month-1u];
        if (month==2u && (year&3u)==0u) {
            cycle=year;
            while (cycle>=400u) cycle-=400u;
            if (cycle!=100u && cycle!=200u && cycle!=300u) span++;
        }
        if (day<=span) break;
        day-=span;
        if (++month>12u) { month=1; year++; }
    }
    if (year>BANK_TICKET_MAX_YEAR) return 0;
    job->expiryDate[0]=(date&0x1FFFFu)|(day<<17)|(month<<22)|(year<<26);
    job->expiryDate[1]=year>>6;
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int LocalTicket_Initialize(BankTicketJobView *job,BankTicketSharedView *shared)
{
    u32 index;
    if (!job || !shared) return 0;
    job->ecAccountId[0]=shared->ecAccountId[0];
    job->ecAccountId[1]=shared->ecAccountId[1];
    job->appletResult=0;
    job->substate=0;
    job->matchedCatalogCount=0;
    job->firstShopUse=0;
    job->expiryKnown=0;
    job->cancelled=0;
    /* Unknown purchase counts must not invent a ticket-purchase reward. SDK
       pointers, vectors and strings retain their native constructed values. */
    /* 购买次数保持未知，不能伪造购票奖励；SDK 指针、容器与字符串保留原版构造值。 */
    for (index=0;index<5;index++) {
        job->ticketPurchaseCounts[index]=BANK_TICKET_PURCHASE_COUNT_UNKNOWN;
    }
    if (!LocalMileage_GetCurrentDate(job->currentDate) || !setExpiryDate(job)) return 0;
    shared->freeCampaignActive=0;
    shared->freeCampaignStart[0]=0;
    shared->freeCampaignStart[1]=0;
    shared->freeCampaignEnd[0]=0;
    shared->freeCampaignEnd[1]=0;
    return 1;
}

__attribute__((used,noinline,section(".text.offline")))
int LocalTicket_Poll(BankTicketJobView *job,u32 *result,u32 minimumDays)
{
    if (!result) return 1;
    *result=0;
    if (!job || minimumDays>BANK_TICKET_ENTITLEMENT_DAYS) return 1;
    job->expiryKnown=1;
    job->substate=BANK_TICKET_JOB_VERIFIED;
    *result=1;
    return 1;
}
/* Query only a nonexistent title/ticket. The exact short-success reply is
   Azahar's unimplemented-handler signature, not a ticket or network error. */
/* 只查询不存在的 title/ticket。精确的成功短响应是 Azahar 未实现接口的特征，
   不是票据缺失或网络错误；临时会话关闭失败时也不启用本地替代。 */
static int ticketInterfaceMissing(void)
{
    static const char serviceName[8]="am:net";
    u32 handle=0,buffer=0;
    volatile u32 *command;
    s32 result;
    int missing;
    result=SRV_GetServiceHandle(&handle,serviceName,
        TICKET_SERVICE_NAME_LENGTH,TICKET_SERVICE_NONBLOCKING);
    if (result || !handle) return 0;
    command=fsCommandBuffer();
    command[0]=TICKET_RIGHTS_REQUEST;
    command[1]=TICKET_PROBE_BUFFER_SIZE;
    command[2]=0;
    command[3]=0;
    command[4]=0;
    command[5]=0;
    command[6]=(TICKET_PROBE_BUFFER_SIZE<<4)|TICKET_MAPPED_BUFFER_WRITE;
    command[7]=(u32)&buffer;
    result=fsSync(handle);
    missing=!result && command[0]==TICKET_RIGHTS_UNIMPLEMENTED_REPLY &&
        command[1]==0;
    result=fsCloseHandle(handle);
    return !result && missing;
}

/* Select once per constructed job so poll and cleanup cannot change backend.
   Public online builds stay native; tests may detect a missing Azahar interface.
   Hardware, unknown environments and normal ticket errors never use fallback. */
/* 每个已构造作业仅选择一次，轮询、活动与清理不会中途切换后端。
   公开版联网始终使用原版；测试版可识别 Azahar 缺失接口。
   实机、未知环境及正常票务错误均不启用本地替代。 */
__attribute__((used,noinline,section(".text.offline")))
int LocalTicket_InitializeSelected(BankTicketJobView *job,
    BankTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback)
{
    int local=mode==TICKET_MODE_OFFLINE;
    *LocalTicket_BackendStorage=TICKET_BACKEND_NATIVE;
    if (!local && allowEmulatorFallback==TICKET_ALLOW_EMULATOR_FALLBACK &&
        CodeExpansion_IsAzahar()) {
        local=ticketInterfaceMissing();
    }
    if (!local) return TicketJob_Initialize(job);
    *LocalTicket_BackendStorage=TICKET_BACKEND_LOCAL;
    return LocalTicket_Initialize(job,shared);
}
