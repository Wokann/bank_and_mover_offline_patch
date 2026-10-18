#ifndef BANK_OFFLINE_PATCH_SERVER_TICKET_H
#define BANK_OFFLINE_PATCH_SERVER_TICKET_H

#include "patch_types.h"
#include "fakeServer/native_response.h"
#include "fake_server_context.h"

/* The native entitlement state fills this view from its native job after
   consuming responses from the selected provider. */
/* 原版使用权状态从自身作业消费所选提供者的响应，再回填此共享视图。 */
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
    /* Decimal YYYYMMDDhhmmss bounds / 十进制 YYYYMMDDhhmmss 活动边界。 */
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
    u8 reserved08[0x1C];
    void *rpcClient;
    NativeRemoteResult *rpcJob;
    u8 reserved2C[0x0C];
    void *catalog;
    void *catalogBuffer;
    u8 reserved40[0x30];
    u32 expiryDate[2];
    s32 ticketPurchaseCounts[5];
    u8 reserved8C[0x14];
    u32 matchedCatalogCount;
    u32 substate;
    u8 firstShopUse;
    u8 expiryKnown;
    /* Native nonzero means return to the purchase menu after an unfinished
       purchase. Values 2/3 are local prompt stages, consumed before returning
       control to the native state machine. */
    /* 原版非零表示未完成购买后返回菜单；2/3 仅用于本地提示阶段，返回原状态机前转为 1。 */
    u8 purchaseReturn;
    u8 reservedAB[0x05];
    u32 ecAccountId[2];
    u32 currentDate[2];
    u8 reservedC0[0x88];
    u32 selectedProduct;
} BankTicketJobView;

typedef struct BankTicketMessageView {
    u8 reserved00[0x5C];
    void *messageView;
} BankTicketMessageView;

typedef struct BankTicketPurchaseStateView {
    u8 reserved00[0x2C];
    void *ui;
    u8 reserved30[0x0C];
    BankTicketMessageView *view;
} BankTicketPurchaseStateView;

/* Native read-only EC views used at the unavailable system-shop boundary.
   Storage remains the original 0x2800-byte allocation and native destructor. */
/* 不可离线执行的系统商店边界使用的原只读 EC 视图；存储和析构仍归原版分配。 */
typedef struct BankTicketEcReader {
    u32 account;
    void *record;
    void *buffer;
    u32 capacity;
} BankTicketEcReader;

typedef struct BankTicketEcAttribute { const char *key,*value; } BankTicketEcAttribute;
typedef struct BankTicketEcItem {
    u8 reserved00[0x10];
    void *price;
    u32 available;
    BankTicketEcAttribute *attributes;
    u32 attributeCount;
} BankTicketEcItem;

typedef struct BankTicketEcCatalog {
    BankTicketEcItem *items;
    u32 count,available;
} BankTicketEcCatalog;

typedef struct BankTicketEcBody {
    BankTicketEcCatalog record;
    BankTicketEcItem items[2];
    u8 prices[2][36];
    BankTicketEcAttribute attributes[2];
    char codes[2][32];
} BankTicketEcBody;

typedef int (*TicketJob_InitializeFn)(BankTicketJobView *);
typedef int (*TicketJob_PollPurchaseFn)(BankTicketJobView *,u32 *);
typedef int (*TicketUi_FadeFn)(void *,u32);
typedef void (*TicketUi_ShowMessageFn)(BankTicketMessageView *,u32,u32,u32);
typedef int (*TicketUi_MessageResultFn)(void *);

#define TicketJob_Initialize ((TicketJob_InitializeFn)0x002A04C0u)
#define TicketJob_PollPurchase ((TicketJob_PollPurchaseFn)0x002A0ADCu)
#define TicketUi_Fade ((TicketUi_FadeFn)0x001D4B5Cu)
#define TicketUi_ShowMessage ((TicketUi_ShowMessageFn)0x001D6650u)
#define TicketUi_MessageResult ((TicketUi_MessageResultFn)0x001D6600u)
#define ServerTicket_BackendStorage ((volatile u8 *)FAKE_SERVER_TICKET_BACKEND_ADDRESS)
#define BankTicket_Parsers ((NativeRpcParserFn const *)0x003C4530u)
#define BankTicket_Result ((NativeRemoteResultBuildFn)0x0022A338u)
#define BankTicket_Complete NativeResponse_Complete

enum TicketBackendConstant {
    TICKET_MODE_OFFLINE = 0,
    TICKET_ALLOW_EMULATOR_FALLBACK = 1,
    TICKET_BACKEND_NATIVE = 0,
    TICKET_BACKEND_SERVER = 1
};

enum TicketPurchaseReturn {
    TICKET_PURCHASE_ABORT = 0,
    TICKET_PURCHASE_RETURN = 1,
    TICKET_PURCHASE_LOW_BALANCE = 2,
    TICKET_PURCHASE_LOW_BALANCE_SHOWN = 3,
    TICKET_PURCHASE_LOW_BALANCE_MESSAGE = 121,
    TICKET_PURCHASE_MESSAGE_CONFIRMED = 4
};

int ServerTicket_InitializeSelected(BankTicketJobView *job,
    BankTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback);
int ServerTicket_Send(void *,NativeRemoteResult *,const NativeRpcReaderView *);
int ServerTicket_Poll(BankTicketJobView *,u32 *,u32);
int ServerTicket_OriginalPoll(BankTicketJobView *,u32 *,u32);
int ServerTicket_PollCredit(BankTicketJobView *,u32 *);
int ServerTicket_OriginalCredit(BankTicketJobView *,u32 *);
int ServerTicket_CancelProvider(void *);
void ServerTicket_RequestApplet(BankTicketJobView *,u32);
int ServerTicket_PollPurchase(BankTicketJobView *,u32 *,BankTicketPurchaseStateView *);
void ServerTicket_EcCatalog(u32 result[2],BankTicketEcReader *,u32,u32,const void *);
void ServerTicket_EcBalance(u32 result[2],const void *,void *);
void ServerTicket_EcFormat(u32 result[2],const void *,char *,const void *);

#endif
