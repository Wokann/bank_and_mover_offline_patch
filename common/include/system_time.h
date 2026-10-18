#ifndef COMMON_OFFLINE_PATCH_SYSTEM_TIME_H
#define COMMON_OFFLINE_PATCH_SYSTEM_TIME_H

#include "patch_types.h"

/* Kernel-published system-time reference selected by the shared counter. */
/* 由共享计数器选择的内核系统时间参考结构。 */
typedef struct SystemTimeReference {
    u64 valueMs;
    u64 valueTick;
    s64 systemClockHz;
    s64 driftMs;
} SystemTimeReference;

#define SYSTEM_TIME_COUNTER_ADDRESS 0x1FF81000u
#define SYSTEM_TIME_REFERENCES_ADDRESS 0x1FF81020u
/* libctru SYSCLOCK_ARM11: SoC clock * 16; SVC ticks do not use CPU boost. */
#define SYSTEM_TIME_TICKS_PER_SECOND (16756991ull*16u)

/* Monotonic kernel ticks for UI repeat timing; independent of console date. */
u64 SystemTime_GetMonotonicTicks(void);

/* Native packed console date shared by Bank and Mover. No entitlement or
   mileage decision is made here; a null output or invalid date fails. */
/* Bank／Mover 共用的原版压缩主机日期；不决定使用权或里程，输出为空或日期无效则失败。 */
int SystemTime_GetCurrentDate(u32 packed[2]);

#endif
