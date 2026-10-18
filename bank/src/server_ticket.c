#include "server_ticket.h"
#include "fakeServer/fake_server_client.h"
#include <stddef.h>

_Static_assert(offsetof(BankTicketJobView,purchaseReturn)==0xAA,"native purchase return flag");
_Static_assert(offsetof(BankTicketJobView,rpcClient)==0x24,"native ticket RPC client");
_Static_assert(offsetof(BankTicketJobView,rpcJob)==0x28,"native ticket SDK job");
_Static_assert(offsetof(BankTicketPurchaseStateView,ui)==0x2C,"native purchase UI");
_Static_assert(offsetof(BankTicketPurchaseStateView,view)==0x3C,"native purchase message view");
/* Select once per constructed job so poll and cleanup cannot change backend.
   The startup cache selects server results before sending any request, only
   for a missing emulator interface; online failures never switch the backend.
   Native result handling and ownership remain unchanged. */
/* 每个已构造作业仅选择一次，轮询、活动与清理不会中途切换后端。
   发送请求前读取启动缓存，仅模拟器缺接口时选择伪服务器；联网失败不切换后端。
   保留原生结果处理和对象所有权。 */
__attribute__((used,noinline,target("arm"),section(".text.offline")))
int ServerTicket_InitializeSelected(BankTicketJobView *job,
    BankTicketSharedView *shared,u32 mode,u32 allowEmulatorFallback)
{
    (void)shared;
    int local=FakeServerClient_SelectTicket(mode==TICKET_MODE_OFFLINE,
        allowEmulatorFallback==TICKET_ALLOW_EMULATOR_FALLBACK);
    if (local && !NativeResponse_InitializeRuntime()) return 0;
    return TicketJob_Initialize(job);
}

__attribute__((used,noinline,target("arm"),section(".text.offline")))
int ServerTicket_Send(void *provider,NativeRemoteResult *job,const NativeRpcReaderView *request)
{
    if ((uintptr_t)provider<0x10u) return 0;
    return NativeResponse_QueueTicket((uintptr_t)provider-0x10u,job,request,BankTicket_Parsers,
        BankTicket_Result,BankTicket_Complete);
}

__attribute__((used,noinline,target("arm"),section(".text.offline")))
int ServerTicket_CancelProvider(void *provider)
{
    NativeTransport_Cancel((uintptr_t)provider);
    return 1;
}

static void pollTicket(BankTicketJobView *job)
{
    if (job) NativeResponse_PollTicket((uintptr_t)job->rpcClient,job->rpcJob,
        BankTicket_Parsers,BankTicket_Result,BankTicket_Complete);
}

static void pollAction(BankTicketJobView *job)
{
    FsServiceRequest input;
    if (!job || NativeTransport_Request((uintptr_t)job,(uintptr_t)job,&input)!=FS_ASYNC_ACCEPTED ||
        input.operation!=FS_SERVICE_TICKET_ACTION) return;
    FsAsyncDelivery delivery;
    int received=NativeTransport_Poll((uintptr_t)job,(uintptr_t)job,&input,0,0,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return;
    int status=FS_IO_ERROR;
    if (received==FS_ASYNC_READY) {
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return;
        if (released==FS_ASYNC_ACCEPTED) status=delivery.result;
    }
    job->purchaseReturn=input.input.shop.applet==7 && status==FS_INSUFFICIENT_FUNDS?
        TICKET_PURCHASE_LOW_BALANCE:TICKET_PURCHASE_ABORT;
    job->appletResult=status==FS_OK?1:2;
}

__attribute__((used,noinline,target("arm"),section(".text.offline")))
int ServerTicket_Poll(BankTicketJobView *job,u32 *result,u32 minimumDays)
{
    pollAction(job);pollTicket(job);
    return ServerTicket_OriginalPoll(job,result,minimumDays);
}

__attribute__((used,noinline,target("arm"),section(".text.offline")))
int ServerTicket_PollCredit(BankTicketJobView *job,u32 *result)
{
    pollAction(job);
    return ServerTicket_OriginalCredit(job,result);
}

/* Only emulate the system-shop boundary. Native product selection, request
   polling, history/expiry queries and entitlement decisions are untouched. */
/* 仅转接系统商店边界；原券种选择、请求轮询、历史／期限回查和使用权判断不改。 */
__attribute__((used,noinline,section(".text.offline")))
void ServerTicket_RequestApplet(BankTicketJobView *job,u32 applet)
{
    NativeTransport_Cancel((uintptr_t)job);
    NativeTicketUi *ui=NativeTransport_TicketUi((uintptr_t)job,(uintptr_t)job->rpcClient);
    if (ui) ui->purchasePhase=0;
    job->purchaseReturn=TICKET_PURCHASE_ABORT;job->appletResult=0;
    FsServiceRequest request;
    int status=FakeServerClient_PrepareTicketAction(&request,applet,job->selectedProduct);
    if (status==FS_OK && NativeTransport_Accept((uintptr_t)job,(uintptr_t)job,&request)==FS_ASYNC_ACCEPTED) {
        FsAsyncDelivery delivery;
        int queued=NativeTransport_Poll((uintptr_t)job,(uintptr_t)job,&request,0,0,&delivery);
        if (queued==FS_ASYNC_ACCEPTED || queued==FS_ASYNC_BUSY || queued==FS_ASYNC_READY) return;
        NativeTransport_Cancel((uintptr_t)job);
    }
    job->appletResult=2;
}

/* The stock checkout layer owns its error dialogs. Local checkout has no
   system applet, so acknowledge the service's insufficient-funds response
   through the native message view, then use the native retry branch. Never
   turn a denied purchase into success or retry a failed SD publication. */
/* 原结账层负责错误提示。本地结账没有系统小程序，因此用原消息窗确认余额不足，
   再交原版重试分支；不得把拒绝改成成功，也不得重试失败的 SD 发布。 */
__attribute__((used,noinline,section(".text.offline")))
int ServerTicket_PollPurchase(BankTicketJobView *job,u32 *result,
    BankTicketPurchaseStateView *state)
{
    pollAction(job);pollTicket(job);
    if (job->purchaseReturn==TICKET_PURCHASE_LOW_BALANCE ||
        job->purchaseReturn==TICKET_PURCHASE_LOW_BALANCE_SHOWN) {
        if (!state || !state->ui || !state->view || !state->view->messageView) {
            job->purchaseReturn=TICKET_PURCHASE_ABORT;
        } else if (job->purchaseReturn==TICKET_PURCHASE_LOW_BALANCE) {
            /* Purchase entry faded for the system checkout. Restore the native
               UI before showing a local prompt, not a blank applet screen. */
            /* 购买入口原本渐隐进入系统结账；先恢复原界面，再显示本地提示。 */
            TicketUi_Fade(state->ui,0);
            TicketUi_ShowMessage(state->view,0,TICKET_PURCHASE_LOW_BALANCE_MESSAGE,1);
            job->purchaseReturn=TICKET_PURCHASE_LOW_BALANCE_SHOWN;
            *result=0;
            return 0;
        } else if (TicketUi_MessageResult(state->view->messageView)!=TICKET_PURCHASE_MESSAGE_CONFIRMED) {
            *result=0;
            return 0;
        } else {
            job->purchaseReturn=TICKET_PURCHASE_RETURN;
        }
    }
    if (job->appletResult!=1) return TicketJob_PollPurchase(job,result);
    /* The stock checkout poll queries history and expiry synchronously. Use
       those same native send/result functions with their existing async mode;
       only this transport phase is local, not product/entitlement decisions. */
    /* 原结账轮询同步回查历史及期限；改用相同原发送／结果函数已有的异步模式。
       仅传输阶段由本地持有，不重做券种或使用权判断。 */
    NativeTicketUi *ui=NativeTransport_TicketUi((uintptr_t)job,(uintptr_t)job->rpcClient);
    if (!ui) { *result=0;return 1; }
    typedef int (*HistorySendFn)(BankTicketJobView *,u32);
    typedef int (*HistoryResultFn)(BankTicketJobView *,u32 *);
    typedef int (*ExpirySendFn)(BankTicketJobView *,u32 *,u32 *,u32);
    typedef int (*ExpiryResultFn)(BankTicketJobView *,u32 *,u32 *,u32 *);
    u32 ok=0;
    *result=0;
    switch (ui->purchasePhase) {
    case 0:
        if (!((HistorySendFn)0x002A0900u)(job,0)) { ui->purchasePhase=5;return 1; }
        ui->purchasePhase=1;return 0;
    case 1:
        if (!((HistoryResultFn)0x002A0B78u)(job,&ok)) return 0;
        if (!ok) { ui->purchasePhase=5;return 1; }
        ui->purchasePhase=2;
        /* fall through */
    case 2:
        if (!((ExpirySendFn)0x001D6FB8u)(job,job->currentDate,job->expiryDate,0)) {
            ui->purchasePhase=5;return 1;
        }
        ui->purchasePhase=3;return 0;
    case 3:
        if (!((ExpiryResultFn)0x002A1940u)(job,job->currentDate,job->expiryDate,&ok)) return 0;
        ui->purchasePhase=ok?4:5;
        if (ok) job->expiryKnown=1;
        /* fall through */
    default:
        *result=ui->purchasePhase==4;return 1;
    }
}

static void resultCode(u32 result[2],int status)
{
    result[0]=status==FS_OK?0:UINT32_MAX; result[1]=0;
}

__attribute__((used,noinline,section(".text.offline")))
void ServerTicket_EcCatalog(u32 result[2],BankTicketEcReader *reader,u32 first,u32 count,const void *request)
{
    (void)first; (void)count; (void)request;
    FsTicketShop shop;
    int status=FakeServerClient_TicketShop(&shop);
    if (!reader || !reader->buffer || reader->capacity<sizeof(BankTicketEcBody)) status=FS_LIMIT;
    if (status==FS_OK) {
        BankTicketEcBody *body=reader->buffer;
        for (u32 i=0;i<sizeof(*body);i++) ((u8 *)body)[i]=0;
        body->record.items=body->items; body->record.count=FS_TICKET_PRODUCT_COUNT;
        for (u32 i=0;i<FS_TICKET_PRODUCT_COUNT;i++) {
            BankTicketEcItem *item=body->items+i;
            for (u32 j=0;j<sizeof(shop.itemCode[i]);j++) body->codes[i][j]=shop.itemCode[i][j];
            item->available=shop.available[i]; body->record.available+=item->available;
            FsTicket_Amount((FsTicketAmount *)(body->prices[i]+4),shop.market,shop.price[i]);
            item->price=body->prices[i]; item->attributes=body->attributes+i; item->attributeCount=1;
            body->attributes[i].key="sys.ItemCode"; body->attributes[i].value=body->codes[i];
        }
        reader->record=&body->record;
    }
    resultCode(result,status);
}

__attribute__((used,noinline,section(".text.offline")))
void ServerTicket_EcBalance(u32 result[2],const void *account,void *amount)
{
    (void)account;
    FsTicketShop shop;
    int status=FakeServerClient_TicketShop(&shop);
    if (!amount) status=FS_INVALID;
    if (status==FS_OK) FsTicket_Amount(amount,shop.market,shop.balance);
    resultCode(result,status);
}

__attribute__((used,noinline,section(".text.offline")))
void ServerTicket_EcFormat(u32 result[2],const void *account,char *text,const void *amount)
{
    (void)account;
    int status=FS_INVALID;
    if (text) {
        for (u32 i=0;i<0x80;i++) text[i]=0;
        status=FakeServerClient_FormatTicketAmount(amount,text+0x20,0x60);
    }
    resultCode(result,status);
}
