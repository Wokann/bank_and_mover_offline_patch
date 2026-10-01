#ifndef BANK_OFFLINE_PATCH_LOCAL_TICKET_H
#define BANK_OFFLINE_PATCH_LOCAL_TICKET_H

#include "bankdata_redirect.h"
#include "patch_types.h"

/* Confirmed fields supplied by the offline entitlement/ticket state. The
   current date is later consumed by the native mileage states; this module
   does not calculate Poké Miles. */
/* 离线使用权／票据状态提供的已确认字段。当前日期随后由原版里程状态读取；
   本模块不计算宝可里程。 */
typedef struct BankTicketSharedView {
    u8 reserved00[0x28];
    u32 currentDate[2];
    u8 reserved30[0x0C];
    u32 entitlementDays;
    u32 entitlementHours;
    u8 entitlementValid;
    u8 reserved45[0x0A];
    u8 remoteRewardFlag;
} BankTicketSharedView;

enum BankTicketConstant {
    BANK_TICKET_ENTITLEMENT_DAYS = 999,
    BANK_TICKET_HOURS_PER_DAY = 24
};

int OfflinePatch_OptionalRewardBypassUpdate(BankStateView *state);

#endif
