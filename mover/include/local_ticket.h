#ifndef MOVER_OFFLINE_PATCH_LOCAL_TICKET_H
#define MOVER_OFFLINE_PATCH_LOCAL_TICKET_H

#include "mover_state.h"
#include "patch_types.h"

/* Fields filled by the native ticket state after its job reports success. */
/* 原版票据状态在作业成功后回填的字段。 */
typedef struct MoverTicketSharedView {
    u8 reserved00[0x30];
    u32 ecAccountId[2];
    s32 annualTicketPurchaseCount;
    u32 entitlementDays;
    u32 entitlementHours;
    u8 entitlementValid;
    u8 reserved45[0x2F0];
    u8 freeCampaignActive;
} MoverTicketSharedView;

/* Partial view of the native 0x158-byte job. Its SDK objects and ownership
   remain managed by the native constructor and destructor. */
/* 原版 0x158 字节作业的局部视图。SDK 对象及其所有权仍由原版构造与析构管理。 */
typedef struct MoverTicketJobView {
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
} MoverTicketJobView;

enum MoverTicketConstant {
    MOVER_TICKET_ENTITLEMENT_DAYS = 999,
    MOVER_TICKET_PURCHASE_COUNT_UNKNOWN = -1,
    MOVER_TICKET_JOB_VERIFIED = 10,
    MOVER_TICKET_MIN_YEAR = 1900,
    MOVER_TICKET_MAX_YEAR = 9999,
    MOVER_TICKET_SUBSTATE_CHECK_ENTITLEMENT = 3
};

int LocalTicket_Initialize(MoverTicketJobView *job,MoverTicketSharedView *shared);
int LocalTicket_Poll(MoverTicketJobView *job,u32 *result,u32 minimumDays);
s32 LocalTicket_CampaignResult(MoverStateView *state);

#endif
