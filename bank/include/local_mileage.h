#ifndef BANK_OFFLINE_PATCH_LOCAL_MILEAGE_H
#define BANK_OFFLINE_PATCH_LOCAL_MILEAGE_H

#include "patch_types.h"

/* Supply the packed console date consumed by the stock local Poké Mile
   states. Point calculation, rounding, balance updates, and reward UI remain
   in the original program. */
/* 提供原版本地宝可里程状态所读取的主机日期格式。点数计算、取整、余额更新与
   领取界面仍全部由原程序负责。 */
int LocalMileage_GetCurrentDate(u32 packed[2]);

#endif
