#ifndef BANK_OFFLINE_PATCH_LOCAL_MILEAGE_H
#define BANK_OFFLINE_PATCH_LOCAL_MILEAGE_H

#include "patch_types.h"

/* Supply the packed console date shared by local mileage, ticket results and
   Bank-slot timestamps. A null output or invalid date returns failure. Point
   calculation, rounding, balance updates and reward UI remain native. */
/* 提供本地里程、票据和银行槽时间戳共用的主机日期格式。输出为空或日期无效时
   返回失败。点数计算、取整、余额更新与领取界面仍全部由原程序负责。 */
int LocalMileage_GetCurrentDate(u32 packed[2]);

#endif
