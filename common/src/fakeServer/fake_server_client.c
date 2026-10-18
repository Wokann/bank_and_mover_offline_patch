#include "fakeServer/fake_server_client.h"
#include "fakeServer/fake_servers.h"
#include "fakeServer/sd_adapter.h"
#include "system_time.h"
#include "runtime_environment.h"
#include "fs_system/console_settings.h"

_Static_assert(sizeof(FsServer)<FAKE_SERVER_CONTEXT_SIZE,"local server overlaps client query marker");
_Static_assert((unsigned)FS_TRANSACTION_WORKSPACE_SIZE>=(unsigned)FS_BANK_SIZE,"service workspace cannot hold a complete bank");
_Static_assert((FAKE_SERVER_CONTEXT_SIZE&0xFFFu)==0,"service context must occupy whole pages");

/* The constructor selects a backend. Background sends capture only values;
   storage and policy work executes in the service worker. */
/* 构造阶段选择后端；后台请求只捕获输入值，存储及策略工作由服务线程执行。 */
/* ARMIPS does not allocate imported C .bss sections. Keep the marker in the
   mapped context's reserved final byte, outside the FsServer object. */
/* ARMIPS 不为导入的 C .bss 分配空间；标记使用新增上下文末字节，避开 FsServer 对象。 */
#define ticketQueryPending (*(volatile u8 *)(FAKE_SERVER_CONTEXT_ADDRESS+FAKE_SERVER_CONTEXT_SIZE-1u))
static uint32_t customOptions __attribute__((section(".data.custom_options")))=0;
void FakeServerClient_SetCustomOptions(uint32_t options) { customOptions=options; }
uint32_t FakeServerClient_CustomOptions(void) { return customOptions; }

static int onlineTicket(void)
{
    return *(const volatile u8 *)FAKE_SERVER_TICKET_BACKEND_ADDRESS==FS_TICKET_BACKEND_ONLINE_FREE;
}

int FakeServerClient_ConsoleProfile(FsConsoleProfile *out)
{
    const FsServer *server=(const FsServer *)FAKE_SERVER_CONTEXT_ADDRESS;
    if(!out || !FsServer_Connected(server) || server->configurationStatus!=FS_OK ||
        !server->configuration.console.ready)return 0;
    FsConsole_Resolve(&server->configuration.console,out);
    return 1;
}

__attribute__((target("arm")))
int FakeServerClient_SelectTicket(int offline,int allowEmulatorFallback)
{
    /* Select before any ticket/resource request from the startup capability
       cache, never from a failed online request or the current free period. */
    /* 请求发送前只按启动能力缓存分流；真实请求失败或不在免费期不能改变后端。 */
    int backend=offline?FS_TICKET_BACKEND_LOCAL:
        RuntimeEnvironment_UseServerTicket(0,allowEmulatorFallback)?
        FS_TICKET_BACKEND_ONLINE_FREE:FS_TICKET_BACKEND_NATIVE;
    *(volatile u8 *)FAKE_SERVER_TICKET_BACKEND_ADDRESS=(u8)backend;
    ticketQueryPending=backend==FS_TICKET_BACKEND_LOCAL;
    return backend;
}

FsServer *FakeServerClient_Get(void)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return 0;
    FsServer *server=(FsServer *)FAKE_SERVER_CONTEXT_ADDRESS;
    FsConfigError error;
    if (!FsServer_Connected(server)) {
        FakeServerApi_Connect(server,FAKE_SERVER_CONTEXT_SIZE,FAKE_SERVER_CLIENT_OWNER,0,
            &FsSdStorage,&FsSdTransactions,&error);
        /* File access and recovery do not depend on policy configuration. */
        /* 文件访问和事务恢复不依赖策略配置是否可用。 */
        if (!FsServer_Connected(server)) return 0;
    }
    return server;
}

int FakeServerClient_SettingsStatus(void)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return FS_IO_ERROR;
    return FsSettings_Status(&FsSdTransactions);
}

int FakeServerClient_ReadSettings(uint32_t offset,void *data,uint32_t size)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return FS_IO_ERROR;
    return FsSettings_ReadRange(&FsSdTransactions,offset,data,size);
}

void FakeServerClient_EndSession(void)
{
    ticketQueryPending=0;
    FakeServerClient_SetCustomOptions(0);
    /* The asynchronous disconnect already ended the server session. This is
       its UI-side observation, not a second unsynchronized server operation. */
    /* 异步断开已结束服务端会话；这里只记录界面确认，不再次无锁操作服务上下文。 */
}

int FakeServerClient_Ticket(FsTicketResponse *ticket)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return 0;
    u32 now[2];
    uint32_t currentDate[2],accepted=0;
    if (onlineTicket()) {
        if (!ticket || !SystemTime_GetCurrentDate(now)) return 0;
        currentDate[0]=now[0]; currentDate[1]=now[1];
        return FsTicket_OnlineFallback(currentDate,ticket);
    }
    FsServer *server=FakeServerClient_Get();
    if (ticketQueryPending && server) {
        FsServer_BeginTicketQuery(server);
        ticketQueryPending=0;
    }
    if (!ticket || FsServer_ValidateTicket(server,&accepted)!=FS_OK || !accepted ||
        !SystemTime_GetCurrentDate(now)) return 0;
    u32 region,country; ConsoleSettings_GetLocation(&region,&country);
    (void)FsServer_SetShopLocation(server,region,country);
    currentDate[0]=now[0]; currentDate[1]=now[1];
    return FsServer_Ticket(server,currentDate,ticket)==FS_OK;
}

int FakeServerClient_Campaign(FsCampaignRecord *body)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return 0;
    if (onlineTicket()) {
        if (!body) return 0;
        FsCampaign_OnlineFallback(body);
        return 1;
    }
    u32 now[2],region,country;
    uint32_t size=0,currentDate[2];
    FsServer *server=FakeServerClient_Get();
    if (!body || !server || !SystemTime_GetCurrentDate(now)) return 0;
    currentDate[0]=now[0];currentDate[1]=now[1];
    ConsoleSettings_GetLocation(&region,&country);
    return FsServer_SetShopLocation(server,region,country)==FS_OK &&
        FsServer_Campaign(server,currentDate,body,sizeof(*body),&size)==FS_OK && size==sizeof(*body);
}

int FakeServerClient_TicketReply(uint32_t operation,void *body,uint32_t capacity,uint32_t *size,
    uint32_t textEncoding)
{
    FsTicketResponse ticket;
    if (!size) return FS_INVALID;
    *size=0;
    if (textEncoding>FS_TICKET_TEXT_UTF8) return FS_INVALID;
    if (!FsTicket_QueryOperation(operation)) return FS_UNSUPPORTED;
    if (!FakeServerClient_Ticket(&ticket)) return FS_IO_ERROR;
    return onlineTicket()?FsTicket_OnlineReply(&ticket,operation,body,capacity,size,textEncoding):
        FsTicket_Reply(&ticket,operation,body,capacity,size,textEncoding);
}

int FakeServerClient_PrepareTicketRequest(FsServiceRequest *request,uint32_t operation,uint32_t encoding)
{
    if (!request || encoding>FS_TICKET_TEXT_UTF8) return FS_INVALID;
    if (!FsTicket_QueryOperation(operation)) return FS_UNSUPPORTED;
    *request=(FsServiceRequest){.operation=FS_SERVICE_TICKET_REPLY};
    request->input.ticket.operation=operation;
    request->input.ticket.textEncoding=encoding;
    request->input.ticket.onlineFallback=onlineTicket();
    request->input.ticket.beginQuery=!onlineTicket() && ticketQueryPending;
    u32 now[2];
    if (!SystemTime_GetCurrentDate(now)) return FS_IO_ERROR;
    request->input.ticket.currentDate[0]=now[0];request->input.ticket.currentDate[1]=now[1];
    if (!onlineTicket()) {
        u32 region,country;ConsoleSettings_GetLocation(&region,&country);
        request->input.ticket.region=region;request->input.ticket.country=country;
    }
    return FS_OK;
}

void FakeServerClient_TicketRequestAccepted(const FsServiceRequest *request)
{
    if (request && request->operation==FS_SERVICE_TICKET_REPLY &&
        !request->input.ticket.onlineFallback && request->input.ticket.beginQuery)
        ticketQueryPending=0;
}

int FakeServerClient_PrepareCampaignRequest(FsServiceRequest *request,uint32_t requestId)
{
    if (!request) return FS_INVALID;
    *request=(FsServiceRequest){.operation=FS_SERVICE_CAMPAIGN_RESOURCE};
    request->input.campaign.requestId=requestId;
    request->input.campaign.onlineFallback=onlineTicket();
    /* The fixed online fallback has no console-date or offline-policy input. */
    /* 固定联网回退不依赖主机日期，不读取离线策略。 */
    if (!onlineTicket()) {
        u32 now[2],region,country;
        if (!SystemTime_GetCurrentDate(now)) return FS_IO_ERROR;
        request->input.campaign.currentDate[0]=now[0];request->input.campaign.currentDate[1]=now[1];
        ConsoleSettings_GetLocation(&region,&country);
        request->input.campaign.region=region;request->input.campaign.country=country;
    }
    return FS_OK;
}

int FakeServerClient_PrepareTicketAction(FsServiceRequest *request,uint32_t applet,uint32_t product)
{
    if (!request || (applet!=5 && applet!=6 && applet!=7)) return FS_INVALID;
    *request=(FsServiceRequest){.operation=FS_SERVICE_TICKET_ACTION};
    request->input.shop.applet=applet;request->input.shop.product=product;
    request->input.shop.onlineFallback=onlineTicket();
    if (onlineTicket()) return FS_UNSUPPORTED;
    u32 now[2],region,country;
    if (applet!=6 && !SystemTime_GetCurrentDate(now)) return FS_IO_ERROR;
    if (applet==6) now[0]=now[1]=0;
    ConsoleSettings_GetLocation(&region,&country);
    request->input.shop.currentDate[0]=now[0];request->input.shop.currentDate[1]=now[1];
    request->input.shop.region=region;request->input.shop.country=country;
    return FS_OK;
}

int FakeServerClient_TicketShop(FsTicketShop *shop)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return FS_IO_ERROR;
    if (onlineTicket()) return FS_UNSUPPORTED;
    FsTicketResponse ticket;
    if (!shop || !FakeServerClient_Ticket(&ticket)) return FS_IO_ERROR;
    FsServer *server=FakeServerClient_Get();
    u32 region,country; ConsoleSettings_GetLocation(&region,&country);
    int status=FsServer_SetShopLocation(server,region,country);
    return status==FS_OK?FsServer_TicketShop(server,shop):status;
}

int FakeServerClient_PurchaseTicket(uint32_t product)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return FS_IO_ERROR;
    if (onlineTicket()) return FS_UNSUPPORTED;
    u32 now[2];
    uint32_t currentDate[2];
    if (!SystemTime_GetCurrentDate(now)) return FS_IO_ERROR;
    currentDate[0]=now[0]; currentDate[1]=now[1];
    FsServer *server=FakeServerClient_Get();
    u32 region,country; ConsoleSettings_GetLocation(&region,&country);
    int status=FsServer_SetShopLocation(server,region,country);
    return status==FS_OK?FsServer_PurchaseTicket(server,currentDate,product):status;
}

int FakeServerClient_AddTicketFunds(void)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return FS_IO_ERROR;
    if (onlineTicket()) return FS_UNSUPPORTED;
    FsServer *server=FakeServerClient_Get();
    u32 region,country; ConsoleSettings_GetLocation(&region,&country);
    int status=FsServer_SetShopLocation(server,region,country);
    return status==FS_OK?FsServer_AddTicketFunds(server):status;
}

int FakeServerClient_FormatTicketAmount(const FsTicketAmount *amount,char *text,uint32_t capacity)
{
    if (onlineTicket()) return FS_UNSUPPORTED;
    return FsTicket_FormatAmount(amount,text,capacity);
}
