#include "local_mileage.h"
#include "system_time.h"

/* Packed console date shared by local miles, tickets and Bank-slot updates. */
/* 本地里程、票据和银行槽更新时间共用的主机日期转换。 */
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
    volatile u32 *counter=(volatile u32 *)SYSTEM_TIME_COUNTER_ADDRESS;
    volatile SystemTimeReference *references=
        (volatile SystemTimeReference *)SYSTEM_TIME_REFERENCES_ADDRESS;
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

int LocalMileage_GetCurrentDate(u32 packed[2])
{
    static const u8 monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    u64 remainder,cycles; u64 days; u32 year=1900,month=1,day,hour,minute,second,span;
    u64 current;
    if (!packed) return 0;
    current=currentSystemTimeMs();
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
