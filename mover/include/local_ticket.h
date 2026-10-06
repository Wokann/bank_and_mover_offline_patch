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

typedef s32 (*SRV_GetServiceHandleFn)(u32 *,const char *,s32,u32);
typedef int (*TicketJob_InitializeFn)(MoverTicketJobView *);

#define SRV_GetServiceHandle ((SRV_GetServiceHandleFn)0x001E596Cu)
#define TicketJob_Initialize ((TicketJob_InitializeFn)0x0023D3C0u)
#define LocalTicket_BackendStorage ((volatile u8 *)0x00363FF8u)

enum TicketProbeConstant {
    TICKET_MODE_OFFLINE = 0,
    TICKET_ALLOW_EMULATOR_FALLBACK = 1,
    TICKET_BACKEND_NATIVE = 0,
    TICKET_BACKEND_LOCAL = 1,
    TICKET_SERVICE_NAME_LENGTH = 6,
    TICKET_SERVICE_NONBLOCKING = 1,
    TICKET_RIGHTS_REQUEST = 0x08210142,
    TICKET_RIGHTS_UNIMPLEMENTED_REPLY = 0x08210040,
    TICKET_PROBE_BUFFER_SIZE = 4,
    TICKET_MAPPED_BUFFER_WRITE = 0x0C
};

enum MoverTicketConstant {
    MOVER_TICKET_ENTITLEMENT_DAYS = 999,
    MOVER_TICKET_PURCHASE_COUNT_UNKNOWN = -1,
    MOVER_TICKET_JOB_VERIFIED = 10,
    MOVER_TICKET_MIN_YEAR = 1900,
    MOVER_TICKET_MAX_YEAR = 9999,
    MOVER_TICKET_SUBSTATE_CHECK_ENTITLEMENT = 3
};

int LocalTicket_Initialize(MoverTicketJobView *job,MoverTicketSharedView *shared);
int LocalTicket_InitializeSelected(MoverTicketJobView *job,
    MoverTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback);
int LocalTicket_Poll(MoverTicketJobView *job,u32 *result,u32 minimumDays);
s32 LocalTicket_CampaignResult(MoverStateView *state);

#endif
