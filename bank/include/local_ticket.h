#ifndef BANK_OFFLINE_PATCH_LOCAL_TICKET_H
#define BANK_OFFLINE_PATCH_LOCAL_TICKET_H

#include "patch_types.h"

/* Fields consumed by the native entitlement state. Its result handling and
   cleanup copy the local job's date and ticket values into this shared view. */
/* 原版使用权状态读取的字段。其结果处理与清理会把本地作业的日期和票据值回填
   到此共享视图。 */
typedef struct BankTicketSharedView {
    u8 reserved00[0x28];
    u32 currentDate[2];
    u32 ecAccountId[2];
    s32 annualTicketPurchaseCount;
    u32 entitlementDays;
    u32 entitlementHours;
    u8 entitlementValid;
    u8 reserved45[0x0A];
    u8 freeCampaignActive;
    u32 freeCampaignStart[2];
    u32 freeCampaignEnd[2];
} BankTicketSharedView;

/* Partial view of the native 0x158-byte job, not the 0x50-byte state controller.
   Unlisted SDK objects remain owned by the native constructor/destructor. */
/* 原版 0x158 字节作业的局部视图，不是 0x50 字节状态控制器。未列出的 SDK 对象
   仍由原版构造与析构函数管理。 */
typedef struct BankTicketJobView {
    u8 reserved00[0x04];
    u32 appletResult;
    u8 reserved08[0x68];
    u32 expiryDate[2];
    s32 ticketPurchaseCounts[5];
    u8 reserved8C[0x14];
    u32 matchedCatalogCount;
    u32 substate;
    u8 firstShopUse;
    u8 expiryKnown;
    u8 cancelled;
    u8 reservedAB[0x05];
    u32 ecAccountId[2];
    u32 currentDate[2];
} BankTicketJobView;

enum BankTicketConstant {
    BANK_TICKET_ENTITLEMENT_DAYS = 999,
    BANK_TICKET_PURCHASE_COUNT_UNKNOWN = -1,
    BANK_TICKET_JOB_VERIFIED = 10,
    BANK_TICKET_MIN_YEAR = 1900,
    BANK_TICKET_MAX_YEAR = 9999
};

int LocalTicket_Initialize(BankTicketJobView *job,BankTicketSharedView *shared);
int LocalTicket_Poll(BankTicketJobView *job,u32 *result,u32 minimumDays);

#endif
