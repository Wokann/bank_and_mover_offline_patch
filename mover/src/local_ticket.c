#include "local_ticket.h"
#include "code_expansion.h"
#include "fs_helpers.h"
#include "system_time.h"

/* Local date input for the native entitlement job's result consumers. */
/* 为原版使用权作业的结果处理提供本地日期。 */
static u64 divideU64(u64 numerator,u64 denominator,u64 *remainder)
{
    u64 quotient=0,rest=0;
    u32 bit;
    if (!denominator) { if (remainder) *remainder=0; return 0; }
    for (bit=0;bit<64;bit++) {
        rest=(rest<<1)|(numerator>>63);
        numerator<<=1;
        quotient<<=1;
        if (rest>=denominator) { rest-=denominator; quotient|=1; }
    }
    if (remainder) *remainder=rest;
    return quotient;
}

static u64 systemTick(void)
{
    register u32 low __asm__("r0");
    register u32 high __asm__("r1");
    __asm__ volatile("svc 0x28" : "=r"(low),"=r"(high) : :
        "r2","r3","r12","memory","cc");
    return (u64)low|((u64)high<<32);
}

static u64 currentSystemTimeMs(void)
{
    volatile u32 *counter=(volatile u32 *)SYSTEM_TIME_COUNTER_ADDRESS;
    volatile SystemTimeReference *references=
        (volatile SystemTimeReference *)SYSTEM_TIME_REFERENCES_ADDRESS;
    SystemTimeReference reference;
    u32 before,after,zero=0;
    u64 elapsed,remainder,seconds,adjustment;
    const u64 hourMs=3600000u;
    do {
        before=*counter;
        reference=references[before&1u];
        __asm__ volatile("mcr p15, 0, %0, c7, c10, 5" : : "r"(zero) : "memory");
        after=*counter;
    } while (before!=after);
    elapsed=systemTick()-reference.valueTick;
    if (reference.systemClockHz>0) {
        seconds=divideU64(elapsed,(u64)reference.systemClockHz,&remainder);
        elapsed=seconds*1000u+divideU64(remainder*1000u,
            (u64)reference.systemClockHz,0);
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
    if (year&3u) return 0;
    (void)divideU64(year,100u,&remainder100);
    (void)divideU64(year,400u,&remainder400);
    return remainder100!=0u || remainder400==0u;
}

static int packTicketDates(MoverTicketJobView *job)
{
    static const u8 monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    u64 remainder,cycles,days;
    u64 now=currentSystemTimeMs();
    u32 year=MOVER_TICKET_MIN_YEAR,month=1,day,hour,minute,second,span,time;
    days=divideU64(now,86400000ull,&remainder);
    /* Skip whole Gregorian 400-year cycles so malformed timestamps can never
       turn the year conversion into an unbounded loop. */
    /* 先跳过完整的公历 400 年周期，避免异常时间戳造成无界的逐年循环。 */
    cycles=divideU64(days,146097u,&days);
    if (cycles>20u) return 0;
    year+=(u32)cycles*400u;
    while (days>=(u32)(365+leapYear(year))) {
        days-=365u+leapYear(year);
        year++;
    }
    if (year>MOVER_TICKET_MAX_YEAR) return 0;
    while (month<=12u) {
        span=monthDays[month-1u]+((month==2u)?leapYear(year):0);
        if (days<span) break;
        days-=span;
        month++;
    }
    day=(u32)days+1u;
    hour=(u32)divideU64(remainder,3600000u,&remainder);
    minute=(u32)divideU64(remainder,60000u,&remainder);
    second=(u32)divideU64(remainder,1000u,0);
    time=(second&63u)|((minute&63u)<<6)|((hour&31u)<<12);
    job->currentDate[0]=time|
        ((day&31u)<<17)|((month&15u)<<22)|(year<<26);
    job->currentDate[1]=year>>6;
    /* Add days only to the expiry date, not to the job's current time. */
    /* 只给到期日加天数，不改变作业的当前时间。 */
    day+=MOVER_TICKET_ENTITLEMENT_DAYS;
    for (;;) {
        span=monthDays[month-1u]+((month==2u)?leapYear(year):0);
        if (day<=span) break;
        day-=span;
        if (++month>12u) { month=1; year++; }
    }
    if (year>MOVER_TICKET_MAX_YEAR) return 0;
    job->expiryDate[0]=time|(day<<17)|(month<<22)|(year<<26);
    job->expiryDate[1]=year>>6;
    return 1;
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
int LocalTicket_Initialize(MoverTicketJobView *job,MoverTicketSharedView *shared)
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
    /* No purchase history is available locally; do not invent purchases or
       alter native SDK pointers, containers and strings. */
    /* 本地没有购票历史；不伪造购买记录，不改变原版 SDK 指针、容器与字符串。 */
    for (index=0;index<5;index++) {
        job->ticketPurchaseCounts[index]=MOVER_TICKET_PURCHASE_COUNT_UNKNOWN;
    }
    shared->freeCampaignActive=0;
    return packTicketDates(job);
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
int LocalTicket_Poll(MoverTicketJobView *job,u32 *result,u32 minimumDays)
{
    if (!result) return 1;
    *result=0;
    if (!job || minimumDays>MOVER_TICKET_ENTITLEMENT_DAYS) return 1;
    job->expiryKnown=1;
    job->substate=MOVER_TICKET_JOB_VERIFIED;
    *result=1;
    return 1;
}

/* Encode the same current-to-expiry window that Bank supplies to its state.
   Mover's native callback accepts calendar bytes and verifies their CRC. */
/* 编码与 Bank 相同的当前时间至到期日窗口。
   Mover 原版回调接收日期字节并校验 CRC。 */
static void setCampaignDate(MoverCampaignDate *output,const u32 input[2])
{
    u32 date=input[0];
    u32 year=(date>>26)|(input[1]<<6);
    output->year[0]=(u8)year;
    output->year[1]=(u8)(year>>8);
    output->month=(u8)((date>>22)&0x0Fu);
    output->day=(u8)((date>>17)&0x1Fu);
    output->hour=(u8)((date>>12)&0x1Fu);
    output->minute=(u8)((date>>6)&0x3Fu);
    output->second=(u8)(date&0x3Fu);
}

__attribute__((used,noinline,section(".text.offline.03_ticket")))
s32 LocalTicket_CampaignResult(MoverTicketStateView *state)
{
    MoverCampaignRecord campaign;
    MoverCampaignSourceView source;
    u32 checksum;
    campaign.reserved00=0;
    setCampaignDate(&campaign.start,state->job->currentDate);
    setCampaignDate(&campaign.end,state->job->expiryDate);
    checksum=Crc16_Calculate(&campaign,sizeof(campaign)-sizeof(campaign.checksum),
        MOVER_TICKET_CAMPAIGN_CRC_SEED);
    campaign.checksum[0]=(u8)checksum;
    campaign.checksum[1]=(u8)(checksum>>8);
    source.requestId=state->campaignRequestId;
    /* Deliver only a local result. The native callback validates the window,
       computes remaining time, sets free/valid flags and resumes the state. */
    /* 仅交付本地结果，不发送请求；由原版回调验证窗口、计算剩余时间、
       设置免费及有效标志并推进状态，不手动覆盖这些输出。 */
    return MoverTicketState_CampaignCallback(state,&source,sizeof(campaign),&campaign);
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

/* Select once per constructed job so poll, campaign and cleanup keep one backend.
   Public online builds stay native; tests may detect a missing Azahar interface.
   Hardware, unknown environments and normal ticket errors never use fallback. */
/* 每个已构造作业仅选择一次，轮询、活动与清理不会中途切换后端。
   公开版联网始终使用原版；测试版可识别 Azahar 缺失接口。
   实机、未知环境及正常票务错误均不启用本地替代。 */
__attribute__((used,noinline,section(".text.offline.03_ticket")))
int LocalTicket_InitializeSelected(MoverTicketJobView *job,
    MoverTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback)
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
