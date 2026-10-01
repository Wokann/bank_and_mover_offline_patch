#ifndef MOVER_OFFLINE_PATCH_SYSTEM_TIME_H
#define MOVER_OFFLINE_PATCH_SYSTEM_TIME_H

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

#endif
