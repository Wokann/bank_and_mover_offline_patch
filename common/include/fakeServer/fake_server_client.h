#ifndef COMMON_FAKE_SERVER_CLIENT_H
#define COMMON_FAKE_SERVER_CLIENT_H

#include "fakeserverapi.h"
#include "console_profile.h"
#include "fake_server_context.h"
#include "native_transport.h"

/* Context storage is a dedicated zero-filled part of each title's RW mapping. */
/* 上下文位于各软件新增读写映射中的专用零填充区，不借用原版对象或 BSS。 */
FsServer *FakeServerClient_Get(void);
/* Memory-only view of the policy already loaded for this session. */
int FakeServerClient_ConsoleProfile(FsConsoleProfile *);
void FakeServerClient_SetCustomOptions(uint32_t options);
uint32_t FakeServerClient_CustomOptions(void);
/* Startup reads do not create a server session or initialize policy/Bank. */
/* 启动只读不创建服务会话，也不初始化策略／银行。 */
int FakeServerClient_SettingsStatus(void);
int FakeServerClient_ReadSettings(uint32_t offset,void *,uint32_t size);
void FakeServerClient_EndSession(void);
int FakeServerClient_SelectTicket(int offline,int allowEmulatorFallback);
/* Per-request input capture, not a prefetched policy/result cache. */
/* 逐请求捕获输入值，不预取策略或缓存整套票务结果。 */
int FakeServerClient_PrepareTicketRequest(FsServiceRequest *,uint32_t operation,uint32_t textEncoding);
int FakeServerClient_PrepareCampaignRequest(FsServiceRequest *,uint32_t requestId);
void FakeServerClient_TicketRequestAccepted(const FsServiceRequest *);
int FakeServerClient_PrepareTicketAction(FsServiceRequest *,uint32_t applet,uint32_t product);

/* Observe console time once; the service decides approval, dates and policy. */
/* 仅观察一次主机时间；审核、有效日期及活动策略由伪服务器决定。 */
int FakeServerClient_Ticket(FsTicketResponse *);
int FakeServerClient_TicketReply(uint32_t operation,void *body,uint32_t capacity,uint32_t *size,
    uint32_t textEncoding);
int FakeServerClient_Campaign(FsCampaignRecord *);
int FakeServerClient_PurchaseTicket(uint32_t product);
int FakeServerClient_TicketShop(FsTicketShop *);
int FakeServerClient_AddTicketFunds(void);
int FakeServerClient_FormatTicketAmount(const FsTicketAmount *,char *,uint32_t capacity);

#endif
