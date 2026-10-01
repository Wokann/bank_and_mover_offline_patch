#ifndef MOVER_OFFLINE_PATCH_LOCAL_TICKET_H
#define MOVER_OFFLINE_PATCH_LOCAL_TICKET_H

#include "mover_state.h"
#include "patch_types.h"

/* Confirmed fields supplied to the stock entitlement/ticket consumers. */
/* 提供给原版使用权／票据消费者的已确认字段。 */
typedef struct MoverTicketSharedView {
    u8 reserved00[0x28];
    u32 expiryDate[2];
    u8 reserved30[0x0C];
    u32 entitlementDays;
    u32 entitlementHours;
    u8 entitlementValid;
    u8 reserved45[0x2F0];
    u8 remoteResultFlag;
} MoverTicketSharedView;

enum MoverTicketConstant {
    MOVER_TICKET_ENTITLEMENT_DAYS = 999,
    MOVER_TICKET_HOURS_PER_DAY = 24
};

int OfflinePatch_TicketUpdate(MoverStateView *state);

#endif
