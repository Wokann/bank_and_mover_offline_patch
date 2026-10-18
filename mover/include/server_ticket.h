#ifndef MOVER_OFFLINE_PATCH_SERVER_TICKET_H
#define MOVER_OFFLINE_PATCH_SERVER_TICKET_H

#include "patch_types.h"
#include "fakeServer/fakeserverapi.h"
#include "fakeServer/native_response.h"
#include "fake_server_context.h"

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
    u8 reserved08[0x1C];
    void *rpcClient;
    NativeRemoteResult *rpcJob;
    u8 reserved2C[0x44];
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

/* Native ticket-state and campaign callback inputs, independent of other
   state classes that reuse the same offsets. */
/* 原版票务状态及活动回调输入，不与复用相同偏移的其他状态类混用。 */
typedef struct MoverTicketStateView {
    u8 reserved00[0x10];
    u32 substate;
    u8 reserved14[0x14];
    MoverTicketSharedView *sharedData;
    u8 reserved2C[0x14];
    MoverTicketJobView *job;
    u32 campaignRequestId;
} MoverTicketStateView;

typedef struct MoverCampaignSourceView {
    u8 reserved00[0x14];
    u32 requestId;
} MoverCampaignSourceView;

typedef int (*TicketJob_InitializeFn)(MoverTicketJobView *);
typedef s32 (*MoverTicketState_CampaignCallbackFn)(MoverTicketStateView *,
    const MoverCampaignSourceView *,u32,const FsCampaignRecord *);

#define TicketJob_Initialize ((TicketJob_InitializeFn)0x0023D3C0u)
#define MoverTicketState_CampaignCallback ((MoverTicketState_CampaignCallbackFn)0x0024991Cu)
#define ServerTicket_BackendStorage ((volatile u8 *)FAKE_SERVER_TICKET_BACKEND_ADDRESS)
#define MoverTicket_Parsers ((NativeRpcParserFn const *)0x00341F18u)
#define MoverTicket_Result ((NativeRemoteResultBuildFn)0x001DC5CCu)
#define MoverTicket_Complete NativeResponse_Complete

enum TicketBackendConstant {
    TICKET_MODE_OFFLINE = 0,
    TICKET_ALLOW_EMULATOR_FALLBACK = 1,
    TICKET_BACKEND_NATIVE = 0,
    TICKET_BACKEND_SERVER = 1
};

int ServerTicket_InitializeSelected(MoverTicketJobView *job,
    MoverTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback);
int ServerTicket_Send(void *,NativeRemoteResult *,const NativeRpcReaderView *);
int ServerTicket_CancelProvider(void *);
int ServerTicket_Poll(MoverTicketJobView *,u32 *,u32);
int ServerTicket_OriginalPoll(MoverTicketJobView *,u32 *,u32);
s32 ServerTicket_UpdateState(MoverTicketStateView *);
s32 ServerTicket_OriginalState(MoverTicketStateView *);
s32 ServerTicket_CampaignResult(MoverTicketStateView *state);

#endif
