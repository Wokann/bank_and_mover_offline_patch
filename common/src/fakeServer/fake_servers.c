#include "fakeServer/fake_servers.h"
#include "fs_system/settings_store.h"
#include <string.h>

static int usable(const FsServer *server)
{
    return server && server->ready && server->transactions;
}

static int prepareBank(FsServer *,FsTransactionFingerprint *);

static int sameFingerprint(const FsTransactionFingerprint *left,const FsTransactionFingerprint *right)
{
    const uint8_t *a=(const uint8_t *)left,*b=(const uint8_t *)right;
    for (uint32_t i=0;i<sizeof(*left);i++) if (a[i]!=b[i]) return 0;
    return 1;
}

static int pendingBankUpdate(const FsServer *server)
{
    FsTransactionRecord record;
    return usable(server) &&
        FsTransaction_ReadRecord(server->transactions,&record)==FS_TRANSACTION_PRESENT &&
        record.operation==FS_TRANSACTION_SOURCE_SAVE && record.serverTransaction.token &&
        record.phase>=FS_TRANSACTION_SOURCE_PENDING && record.phase<=FS_TRANSACTION_CLEANUP_PENDING;
}

static int liveBankDownload(const FsServer *server)
{
    FsTransactionRecord record;
    return usable(server) && server->owner==FS_TRANSACTION_OWNER_BANK &&
        server->bankDownloadToken &&
        FsTransaction_ReadRecord(server->transactions,&record)==FS_TRANSACTION_PRESENT &&
        record.owner==server->owner && record.operation==FS_TRANSACTION_SOURCE_SAVE &&
        record.sourceKind==FS_TRANSACTION_NATIVE_SOURCE &&
        record.phase==FS_TRANSACTION_SOURCE_PENDING &&
        record.serverTransaction.token==server->bankDownloadToken &&
        !(record.flags&(FS_TRANSACTION_SOURCE_ACKNOWLEDGED|FS_TRANSACTION_TURTLE_WRITE_PENDING)) &&
        FsTransaction_Matches(server->transactions,FS_TRANSACTION_BANK,&record,&record.bankBefore);
}

static int policyStatus(const FsServer *server)
{
    return usable(server)?server->configurationStatus:FS_INVALID;
}

static int validBankHeader(const void *data,uint32_t size)
{
    const FsBankHeader *header=data;
    return data && size==FS_BANK_SIZE && header->formatVersion==FS_BANK_FORMAT_VERSION &&
        header->boxCount==FS_BANK_BOX_COUNT;
}

/* Native group views index directly by the low byte of each box's order.
   Require unique, contiguous orders in every group before exposing the data. */
/* 原生分组界面直接按盒子顺序的低字节索引；提供数据前要求各组顺序唯一且连续。 */
static int bankStructure(const FsTransactionIo *io,uint32_t handle,const uint8_t *data)
{
    uint32_t seen[FS_BANK_GROUP_COUNT][4];
    uint8_t counts[FS_BANK_GROUP_COUNT],maximum[FS_BANK_GROUP_COUNT];
    FsBankBoxMetadata box;
    for (uint32_t group=0;group<FS_BANK_GROUP_COUNT;group++) {
        counts[group]=0; maximum[group]=0;
        for (uint32_t word=0;word<4;word++) seen[group][word]=0;
    }
    for (uint32_t i=0;i<FS_BANK_BOX_COUNT;i++) {
        uint32_t offset=sizeof(FsBankHeader)+i*FS_BANK_BOX_SIZE+FS_BANK_SLOT_DATA_SIZE;
        if (io) {
            if (!io->read(handle,offset,&box,sizeof(box))) return FS_IO_ERROR;
        } else {
            for (uint32_t j=0;j<sizeof(box);j++) ((uint8_t *)&box)[j]=data[offset+j];
        }
        uint32_t group=box.group,order=(uint8_t)box.order;
        if (group>=FS_BANK_GROUP_COUNT || order>=FS_BANK_BOX_COUNT ||
            (seen[group][order>>5]&(1u<<(order&31)))) return FS_INVALID;
        seen[group][order>>5]|=1u<<(order&31);
        counts[group]++;
        if (order>maximum[group]) maximum[group]=(uint8_t)order;
    }
    for (uint32_t group=0;group<FS_BANK_GROUP_COUNT;group++)
        if (counts[group] && maximum[group]+1!=counts[group]) return FS_INVALID;
    return FS_OK;
}

static int validBank(const void *data,uint32_t size)
{
    return validBankHeader(data,size) && bankStructure(0,0,data)==FS_OK;
}

static int inspectBankHandle(const FsTransactionIo *io,uint32_t handle,uint64_t size)
{
    FsBankHeader header;
    if (size!=FS_BANK_SIZE) return FS_INVALID;
    if (!io->read(handle,0,&header,sizeof(header))) return FS_IO_ERROR;
    if (!validBankHeader(&header,(uint32_t)size)) return FS_INVALID;
    return bankStructure(io,handle,0);
}

typedef struct BankInspection {
    uint32_t seen[FS_BANK_GROUP_COUNT][4];
    uint8_t counts[FS_BANK_GROUP_COUNT],maximum[FS_BANK_GROUP_COUNT];
    uint32_t nextBox;
    uint8_t group;
    int result;
} BankInspection;

/* Check group/order bytes from the hashing stream, including a split between
   the two bytes. Names and slot contents are still included in the full hash. */
/* 从摘要读取流检查分组／顺序字节，允许两字节跨块；名称及槽位正文仍参与全文摘要。 */
static int inspectBankBlock(void *context,uint64_t size,uint64_t offset,
    const uint8_t *data,uint32_t amount)
{
    BankInspection *inspection=context;
    if (!offset) {
        uint32_t version=__builtin_offsetof(FsBankHeader,formatVersion);
        uint32_t boxes=__builtin_offsetof(FsBankHeader,boxCount);
        if (size!=FS_BANK_SIZE || amount<boxes+2 ||
            (data[version]|((uint32_t)data[version+1]<<8))!=FS_BANK_FORMAT_VERSION ||
            (data[boxes]|((uint32_t)data[boxes+1]<<8))!=FS_BANK_BOX_COUNT) goto invalid;
    }
    while (inspection->nextBox<FS_BANK_BOX_COUNT) {
        uint64_t position=sizeof(FsBankHeader)+inspection->nextBox*FS_BANK_BOX_SIZE+
            FS_BANK_SLOT_DATA_SIZE+__builtin_offsetof(FsBankBoxMetadata,group);
        if (position>=offset+amount) break;
        if (position>=offset) inspection->group=data[(uint32_t)(position-offset)];
        if (position+1>=offset+amount) break;
        if (position+1<offset) goto invalid;
        uint32_t group=inspection->group,order=data[(uint32_t)(position+1-offset)];
        if (group>=FS_BANK_GROUP_COUNT || order>=FS_BANK_BOX_COUNT ||
            (inspection->seen[group][order>>5]&(1u<<(order&31)))) goto invalid;
        inspection->seen[group][order>>5]|=1u<<(order&31);
        inspection->counts[group]++;
        if (order>inspection->maximum[group]) inspection->maximum[group]=(uint8_t)order;
        inspection->nextBox++;
    }
    if (offset+amount==size) {
        if (inspection->nextBox!=FS_BANK_BOX_COUNT) goto invalid;
        for (uint32_t group=0;group<FS_BANK_GROUP_COUNT;group++)
            if (inspection->counts[group] &&
                inspection->maximum[group]+1!=inspection->counts[group]) goto invalid;
    }
    return 1;
invalid:
    inspection->result=FS_INVALID;
    return 0;
}

int FsServer_Open(FsServer *server,uint32_t owner,const FsStorageIo *storage,
    const FsTransactionIo *transactions,FsConfigError *error)
{
    if (server) {
        if (server->ready && server->storage)
            FsConfiguration_Release(server->storage,&server->configuration);
        memset(&server->configuration,0,sizeof(server->configuration));
        server->ready=0;
        server->bankDownloadToken=0;
        server->ticket.valid=0;
        server->ticket.freeActivity=0;
        server->configurationStatus=FS_INVALID;
        server->rollbackCodes.candidateCount=0;
        server->rollbackAuthorized=0;
        server->bufferBusy=0;
        server->shopMarket=FS_SHOP_UNKNOWN;
        server->shopRegion=0xffffffffu; server->shopCountry=0;
    }
    if (!server || !storage || !transactions || !error ||
        (owner!=FS_TRANSACTION_OWNER_BANK && owner!=FS_TRANSACTION_OWNER_MOVER) ||
        !storage->openRead || !storage->openWrite || !storage->read || !storage->write ||
        !storage->close || !storage->remove || !storage->rename || !transactions->openRead || !transactions->read ||
        !transactions->close || !transactions->openWrite || !transactions->setSize ||
        !transactions->write || !transactions->remove || !transactions->rename ||
        !transactions->freeBytes) return FS_INVALID;
    server->ticketReload=0; server->ticketSnapshotReady=0;
    server->ticketQuery=0;
    server->storage=storage; server->owner=owner;
    server->transactionIo=*transactions;
    server->transactionIo.buffer=server->ioBuffer;
    server->transactionIo.bufferSize=sizeof(server->ioBuffer);
    server->transactionIo.bufferBusy=&server->bufferBusy;
    server->transactionIo.view=&server->loaded;
    server->transactions=&server->transactionIo;
    FsTransaction_ViewClear(server->transactions);server->bankDelivered=0;
    uint32_t applied=0;
    server->ready=1;
    /* Recover file decisions independently from current text configuration. */
    /* 文件事务的恢复独立于当前文本配置，不重新执行此前的筛选或礼物选择。 */
    FsTransactionFingerprint observed={0};
    int prepared=prepareBank(server,&observed);
    int loaded=FsServer_ReloadConfiguration(server,&applied);
    *error=server->configuration.ticketError;
    int result=loaded;
    if (result!=FS_OK && result!=error->result) {
        error->result=result; error->path="server.conf"; error->line=0;
    }
    server->configurationStatus=result;
    /* Storage stays available for title-language reads even when a policy file
       is invalid; no partially loaded policy can authorize service results. */
    /* 策略配置无效时仍允许标题读取语言；部分加载的配置不能授权服务响应。 */
    server->ready=1;
    if (result!=FS_OK) return result;
    /* Recovery is a session-entry operation, not a prelude to every write. */
    /* 恢复只在进入会话时执行，不能在每次写入前中止当前正在进行的事务。 */
    /* Classify the actual Bank before recovery: a structurally valid manual
       replacement must not be overwritten by the previous committed backup. */
    /* 恢复前先检查实际银行；合法手动替换不能被旧提交备份覆盖。 */
    result=prepared;
    if (result==FS_MISSING) return FS_OK;
    /* Only this Open invocation may reuse the checked body. Configuration
       loading does not write the Bank; still reread the journal/settings and
       working-file state before accepting the session. Recovery paths that
       changed the body did not supply a snapshot and keep the full guard. */
    /* 仅本次 Open 复用已检查正文。配置载入不写银行，仍重新检查日志、设置和
       暂存状态；发生正文恢复的路径不提供快照，保留完整检查。 */
    return result==FS_OK && !FsTransaction_GuardBank(server->transactions,
        observed.present?&observed:0)?FS_LOCKED:result;
}

int FsServer_ConfigurationError(FsServer *server,uint32_t section,FsConfigError *out)
{
    if (!out || !usable(server)) return FS_INVALID;
    const FsConfigError *error=section==FS_CONFIG_TICKET?&server->configuration.ticketError:
        section==FS_CONFIG_GIFT?&server->configuration.giftError:
        section==FS_CONFIG_MOVER_NDS || section==FS_CONFIG_MOVER_VC ||
        section==FS_CONFIG_WORDS_NDS || section==FS_CONFIG_WORDS_VC?&server->configuration.moverError:0;
    if (!error) return FS_INVALID;
    *out=*error;
    return error->result;
}

static int reloadTicket(FsServer *server,uint32_t *applied)
{
    int result=FsServer_ReloadConfiguration(server,applied);
    server->ticketReload=0;
    return result;
}

void FsServer_UserLocation(const FsServer *server,uint32_t *region,uint32_t *country)
{
    if(server && server->storage && (server->storage->capabilities&FS_STORAGE_ACCOUNT_LAYOUT) &&
        server->configuration.console.ready)
        FsConsole_Location(&server->configuration.console,region,country);
}

static int selectCurrency(FsServer *server)
{
    FsServer_UserLocation(server,&server->shopRegion,&server->shopCountry);
    const FsTicketSettings *settings=&server->configuration.ticketSettings;
    uint32_t market=settings->ready && !settings->autocurrency?settings->currentMarket:
        FsTicket_Market(server->shopRegion,server->shopCountry);
    server->shopMarket=market;
    /* auto is a derived value, not a reason to rewrite any configuration. */
    /* auto 仅计算本次币种，不为更新显示币种而重写配置。 */
    return FS_OK;
}

void FsServer_BeginTicketQuery(FsServer *server)
{
    if (!usable(server)) return;
    server->ticketQuery=1;
    server->ticketReload=1;
    server->ticketSnapshotReady=0;
}

int FsServer_Ticket(FsServer *server,const uint32_t currentDate[2],FsTicketResponse *out)
{
    uint32_t applied=0;
    if (!out || !currentDate || !FsDate_FromPacked(currentDate)) return FS_INVALID;
    if (usable(server) && server->ticketQuery && server->ticketSnapshotReady &&
        !server->ticketReload && policyStatus(server)==FS_OK) {
        for (uint32_t i=0;i<sizeof(*out);i++)
            ((uint8_t *)out)[i]=((const uint8_t *)&server->ticket)[i];
        return FS_OK;
    }
    if (usable(server)) (void)reloadTicket(server,&applied);
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    status=selectCurrency(server);
    if (status!=FS_OK) return status;
    server->ticket.valid=0; server->ticket.freeActivity=0;
    FsTicketPolicy policy=server->configuration.ticket;
    if (!FsTicket_Select(&server->configuration.ticketSettings,currentDate,
        server->shopRegion,server->shopCountry,&policy) ||
        !FsTicket_Build(&policy,currentDate,out)) return FS_INVALID;
    FsCampaign_Encode(&policy,out,&server->campaign);
    server->ticketSnapshotReady=1;
    for (uint32_t i=0;i<sizeof(*out);i++)
        ((uint8_t *)&server->ticket)[i]=((const uint8_t *)out)[i];
    return FS_OK;
}

int FsServer_ReloadConfiguration(FsServer *server,uint32_t *applied)
{
    if (!usable(server) || !applied) return FS_INVALID;
    server->ticketSnapshotReady=0;
    *applied=0;
    FsConfigError error;
    (void)FsConfiguration_LoadService(server->storage,&server->configuration,&error);
    FsServer_UserLocation(server,&server->shopRegion,&server->shopCountry);
    *applied=1;
    return server->configurationStatus=server->configuration.ticketError.result;
}

uint32_t FsServer_ConfigurationWorkspaceSize(void) { return sizeof(FsConfiguration); }

int FsServer_EditConfiguration(FsServer *server,void *workspace,uint32_t capacity,
    uint32_t section,const FsConfigEdit *edits,uint32_t count,FsConfigError *error)
{
    if (!usable(server) || !workspace || capacity<sizeof(FsConfiguration) ||
        workspace==&server->configuration) return FS_INVALID;
    memset(workspace,0,sizeof(FsConfiguration));
    int result=FsConfiguration_Edit(server->storage,workspace,section,edits,count,error);
    FsConfiguration_Release(server->storage,workspace);
    if (result!=FS_OK) return result;
    uint32_t applied=0;
    return FsServer_ReloadConfiguration(server,&applied);
}

int FsServer_TicketShop(FsServer *server,FsTicketShop *shop)
{
    int status=policyStatus(server);
    if (status!=FS_OK || !shop) return status!=FS_OK?status:FS_INVALID;
    status=selectCurrency(server);
    if (status!=FS_OK) return status;
    if (server->shopMarket>=FS_SHOP_MARKET_COUNT && server->shopMarket!=FS_SHOP_UNKNOWN) return FS_UNSUPPORTED;
    FsTicketPolicy policy=server->configuration.ticket;
    if (FsDate_FromPacked(server->ticket.currentDate))
        (void)FsTicket_Select(&server->configuration.ticketSettings,server->ticket.currentDate,
            server->shopRegion,server->shopCountry,&policy);
    FsTicket_Shop(&policy,shop);
    shop->market=server->shopMarket;
    shop->price[FS_TICKET_ANNUAL]=server->configuration.ticketSettings.ready?
        *FsConfiguration_ShopValue(&server->configuration,shop->market,0):FsTicket_AnnualPrice(shop->market);
    shop->balance=*FsConfiguration_ShopValue(&server->configuration,shop->market,2);
    return FS_OK;
}

int FsServer_SetShopLocation(FsServer *server,uint32_t region,uint32_t country)
{
    if (!usable(server)) return FS_INVALID;
    FsServer_UserLocation(server,&region,&country);
    if (region!=server->shopRegion || country!=server->shopCountry)
        server->ticketSnapshotReady=0;
    server->shopRegion=region; server->shopCountry=country;
    const FsTicketSettings *settings=&server->configuration.ticketSettings;
    server->shopMarket=settings->ready && !settings->autocurrency?settings->currentMarket:FsTicket_Market(region,country);
    /* Unmatched/manual NONE uses its own unitless local wallet, not another currency. */
    /* 自动未匹配或手动 NONE 使用独立的无币种本地钱包，不挪用其他币种余额。 */
    return FS_OK;
}

static int reloadShop(FsServer *server)
{
    uint32_t applied=0;
    int result=FsServer_ReloadConfiguration(server,&applied);
    if (result!=FS_OK) return result;
    result=selectCurrency(server);
    if (result!=FS_OK) return result;
    return server->shopMarket<FS_SHOP_MARKET_COUNT || server->shopMarket==FS_SHOP_UNKNOWN?FS_OK:FS_UNSUPPORTED;
}

int FsServer_AddTicketFunds(FsServer *server)
{
    if (!usable(server)) return FS_INVALID;
    int result=reloadShop(server);
    if (result!=FS_OK) return result;
    uint32_t market=server->shopMarket,balance=*FsConfiguration_ShopValue(&server->configuration,market,2);
    uint32_t credit=*FsConfiguration_ShopValue(&server->configuration,market,1);
    if (credit>FS_SHOP_MONEY_LIMIT-balance) return FS_LIMIT;
    if (!credit) return FS_OK;
    FsConfigError error;
    result=FsConfiguration_Purchase(server->storage,&server->configuration,0,market,balance+credit,FS_TICKET_FIELD_PAID,&error);
    server->configurationStatus=result;
    return result;
}

int FsServer_PurchaseTicket(FsServer *server,const uint32_t currentDate[2],uint32_t product)
{
    if (!usable(server) || !currentDate) return FS_INVALID;
    FsConfigError error;
    server->ticket.valid=0; server->ticket.freeActivity=0;
    int result=reloadShop(server);
    server->ticketReload=1;
    if (result!=FS_OK) return result;
    FsTicketPolicy policy;
    FsTicketPolicy selected=server->configuration.ticket;
    if (!FsTicket_Select(&server->configuration.ticketSettings,currentDate,
        server->shopRegion,server->shopCountry,&selected)) return FS_INVALID;
    result=FsTicket_Purchase(&selected,currentDate,product,&policy);
    if (result!=FS_OK) return result;
    uint32_t market=server->shopMarket,balance=*FsConfiguration_ShopValue(&server->configuration,market,2);
    uint32_t price=product==FS_TICKET_ANNUAL?*FsConfiguration_ShopValue(&server->configuration,market,0):0;
    if (balance<price) return FS_INSUFFICIENT_FUNDS;
    uint32_t field=FsTicket_PeriodField(&server->configuration.ticketSettings,FS_TICKET_FIELD_PAID,server->shopRegion,server->shopCountry);
    result=FsConfiguration_Purchase(server->storage,&server->configuration,&policy,market,balance-price,field,&error);
    server->configurationStatus=result;
    return result;
}

int FsServer_Campaign(FsServer *server,const uint32_t currentDate[2],void *body,uint32_t capacity,
    uint32_t *size)
{
    if (!size) return FS_INVALID;
    *size=0;
    FsTicketResponse ticket;
    /* Each explicit resource request observes current policy/time. A prior
       EC query is neither required nor evidence that its snapshot is fresh. */
    /* 每次活动资源请求观察当前策略及时间；不要求已有 EC 查询，也不借旧快照判断。 */
    FsServer_BeginTicketQuery(server);
    int status=FsServer_Ticket(server,currentDate,&ticket);
    return status==FS_OK?FsServer_Resource(server,FS_FREE_CAMPAIGN_ADDRESS,body,capacity,size):status;
}

int FsServer_Resource(FsServer *server,const char *address,void *body,uint32_t capacity,
    uint32_t *size)
{
    if (!size) return FS_INVALID;
    *size=0;
    if (!usable(server) || !server->ticketSnapshotReady) return FS_INVALID;
    const char *expected=FS_FREE_CAMPAIGN_ADDRESS;
    if (!address) return FS_INVALID;
    while (*address && *address==*expected) { address++; expected++; }
    if (*address || *expected) return FS_UNSUPPORTED;
    if (!server->ticket.freeActivity) return FS_MISSING;
    if (!body || capacity<sizeof(server->campaign)) return FS_LIMIT;
    for (uint32_t i=0;i<sizeof(server->campaign);i++)
        ((uint8_t *)body)[i]=((const uint8_t *)&server->campaign)[i];
    *size=sizeof(server->campaign);
    return FS_OK;
}

static int readBankSnapshot(FsServer *,void *,uint32_t,FsTransactionFingerprint *);

int FsServer_ReadBankRange(FsServer *server,uint32_t offset,void *data,uint32_t size)
{
    if (!usable(server) || (!data && size) || offset>FS_BANK_SIZE ||
        size>FS_BANK_SIZE-offset) return FS_INVALID;
    const FsTransactionIo *io=server->transactions;
    FsTransactionFingerprint observed;
    int result=readBankSnapshot(server,io->buffer,io->bufferSize,&observed);
    if (result==FS_OK && size) {
        uint8_t *out=data;const uint8_t *in=io->buffer+offset;
        if ((uintptr_t)out>(uintptr_t)in && (uintptr_t)out<(uintptr_t)in+size)
            for (uint32_t i=size;i;i--) out[i-1]=in[i-1];
        else for (uint32_t i=0;i<size;i++) out[i]=in[i];
        if (out!=in && (uintptr_t)out<(uintptr_t)io->buffer+io->bufferSize &&
            (uintptr_t)out+size>(uintptr_t)io->buffer) server->loaded.bodyValid=0;
    }
    return result;
}

static int readBankSnapshot(FsServer *server,void *data,uint32_t capacity,
    FsTransactionFingerprint *observed)
{
    if (!usable(server) || !data || capacity<FS_BANK_SIZE) return FS_INVALID;
    const FsTransactionIo *io=server->transactions;
    int cached=FsTransaction_ViewGet(io,FS_TRANSACTION_BANK,0,observed) &&
        server->loaded.bankStructured && FsTransaction_ViewBody(io,FS_TRANSACTION_BANK,0,observed);
    if (cached) {
        if (data!=io->buffer) memcpy(data,io->buffer,FS_BANK_SIZE);
    }
    else {
        uint32_t handle=0; uint64_t size=0;
        int status=io->openRead(FS_TRANSACTION_BANK,0,&handle,&size);
        if (status==FS_TRANSACTION_MISSING) {
            FsTransaction_ViewForget(io,FS_TRANSACTION_BANK);return FS_MISSING;
        }
        if (status!=FS_TRANSACTION_PRESENT) { FsTransaction_ViewClear(io);return FS_IO_ERROR; }
        int result=size==FS_BANK_SIZE?FS_OK:FS_INVALID;
        if (result==FS_OK && !io->read(handle,0,data,FS_BANK_SIZE)) result=FS_IO_ERROR;
        if (!io->close(handle)) result=FS_IO_ERROR;
        if (result!=FS_OK) { FsTransaction_ViewClear(io);return result; }
        if (!validBank(data,FS_BANK_SIZE)) { FsTransaction_ViewClear(io);return FS_INVALID; }
        *observed=(FsTransactionFingerprint){0};
        observed->present=1; observed->size=FS_BANK_SIZE;
        FsTransaction_Hash(data,FS_BANK_SIZE,observed->hash);
        FsTransaction_ViewPut(io,FS_TRANSACTION_BANK,0,observed);
        FsTransaction_ViewRememberBody(io,FS_TRANSACTION_BANK,0,observed,data,FS_BANK_SIZE);
        server->loaded.bankStructured=1;
    }
    /* Validate the exact snapshot handed to the native client, rather than
       hashing one read and then returning a different second read. */
    /* 校验实际交给原版客户端的内存快照，不先摘要一遍再另读一份正文。 */
    server->bankDelivered=1;
    if (FsTransaction_GuardBank(io,observed)) return FS_OK;
    FsTransactionRecord record;
    if (server->owner==FS_TRANSACTION_OWNER_BANK && server->bankDownloadToken &&
        FsTransaction_ReadRecord(io,&record)==FS_TRANSACTION_PRESENT &&
        record.owner==server->owner && record.operation==FS_TRANSACTION_SOURCE_SAVE &&
        record.sourceKind==FS_TRANSACTION_NATIVE_SOURCE && record.phase==FS_TRANSACTION_SOURCE_PENDING &&
        record.serverTransaction.token==server->bankDownloadToken &&
        !(record.flags&(FS_TRANSACTION_SOURCE_ACKNOWLEDGED|FS_TRANSACTION_TURTLE_WRITE_PENDING))) {
        const uint8_t *a=(const uint8_t *)observed,*b=(const uint8_t *)&record.bankBefore;
        uint32_t equal=1;
        for (uint32_t i=0;i<sizeof(*observed);i++) if (a[i]!=b[i]) equal=0;
        if (equal) return FS_OK;
    }
    return pendingBankUpdate(server)?FS_TRANSACTION_MISMATCH:FS_LOCKED;
}

int FsServer_ReadBank(FsServer *server,void *data,uint32_t capacity)
{
    FsTransactionFingerprint observed;
    return readBankSnapshot(server,data,capacity,&observed);
}

static int stageBank(FsServer *,const void *,uint32_t,const FsSource *,
    const FsTransactionFingerprint *);

int FsServer_DownloadBank(FsServer *server,void *data,uint32_t capacity,
    const FsSourceMetadata *metadata,FsTransactionParameters *transaction)
{
    if (!metadata || !transaction || !usable(server) ||
        server->owner!=FS_TRANSACTION_OWNER_BANK) return FS_INVALID;
    FsTransactionFingerprint observed;
    int result=readBankSnapshot(server,data,capacity,&observed);
    if (result!=FS_OK) return result;
    const FsSource source={FS_TRANSACTION_NATIVE_SOURCE,0,0,0,0,0,metadata};
    result=stageBank(server,data,FS_BANK_SIZE,&source,&observed);
    if (result!=FS_OK) return result;
    result=FsServer_DescribeTransaction(server,transaction);
    if (result==FS_OK) server->bankDownloadToken=transaction->token;
    return result;
}

int FsServer_CreateBank(FsServer *server,const void *data,uint32_t size)
{
    if (!usable(server) || server->owner!=FS_TRANSACTION_OWNER_BANK ||
        !validBank(data,size)) return FS_INVALID;
    if (!FsTransaction_Guard(server->transactions)) return FS_LOCKED;
    uint32_t handle=0; uint64_t fileSize=0;
    int status=server->transactions->openRead(FS_TRANSACTION_BANK,0,&handle,&fileSize);
    if (status==FS_TRANSACTION_PRESENT) {
        if (!server->transactions->close(handle)) return FS_IO_ERROR;
        return FS_LOCKED;
    }
    if (status!=FS_TRANSACTION_MISSING) return FS_IO_ERROR;
    status=server->transactions->openRead(FS_TRANSACTION_BACKUP,0,&handle,&fileSize);
    if (status==FS_TRANSACTION_PRESENT) {
        if (!server->transactions->close(handle)) return FS_IO_ERROR;
        return FS_LOCKED;
    }
    if (status!=FS_TRANSACTION_MISSING) return FS_IO_ERROR;
    return FsTransaction_Write(server->transactions,server->owner,
        FS_TRANSACTION_BANK_ONLY,data,size)?FS_OK:FS_IO_ERROR;
}

static int stageBank(FsServer *server,const void *bank,uint32_t size,const FsSource *source,
    const FsTransactionFingerprint *observed)
{
    if (!usable(server) || (!bank && size) || (bank && !validBank(bank,size))) return FS_INVALID;
    if (!FsTransaction_GuardBank(server->transactions,observed)) {
        if (!bank || !source || source->kind!=FS_TRANSACTION_NATIVE_SOURCE ||
            !liveBankDownload(server)) return FS_LOCKED;
        uint64_t token=server->bankDownloadToken;
        server->bankDownloadToken=0;
        return FsTransaction_UpdateBankReservation(server->transactions,server->owner,
            token,bank,size,source->metadata)?FS_OK:FS_IO_ERROR;
    }
    if (source && source->kind==FS_TRANSACTION_NDS_FILE) {
        if (server->owner!=FS_TRANSACTION_OWNER_MOVER) return FS_INVALID;
        int staged=FsTransaction_StageSource(server->transactions,server->owner,source->mode,
            source->path,source->pathSize,source->gameCode,bank,size,source->expected,
            source->metadata);
        return staged?FS_OK:FS_IO_ERROR;
    }
    if (!bank || (source && source->kind!=FS_TRANSACTION_NATIVE_SOURCE &&
        source->kind!=FS_TRANSACTION_NO_SOURCE)) return FS_INVALID;
    uint32_t operation=source && source->kind==FS_TRANSACTION_NATIVE_SOURCE?
        FS_TRANSACTION_SOURCE_SAVE:FS_TRANSACTION_BANK_ONLY;
    int staged=observed?FsTransaction_StageSnapshot(server->transactions,server->owner,
        bank,size,source?source->metadata:0,observed):
        FsTransaction_Stage(server->transactions,server->owner,operation,bank,size,
            source?source->metadata:0);
    return staged?FS_OK:FS_IO_ERROR;
}

int FsServer_Stage(FsServer *server,const void *bank,uint32_t size,const FsSource *source)
{
    /* A real upload is a write boundary, not another read-only query. Check
       the actual primary once before allowing an existing loaded snapshot to
       overwrite it, including SD-source matching. */
    /* 实际上传是写入边界；先重新核对主银行及 SD 来源一次，再允许已载入快照
       写回，避免覆盖载入期间由外部更换的数据。只读查询不这样重读。 */
    if (usable(server)) {
        if (bank) FsTransaction_ViewForget(server->transactions,FS_TRANSACTION_BANK);
        if (source && source->kind==FS_TRANSACTION_NDS_FILE)
            FsTransaction_ViewForget(server->transactions,FS_TRANSACTION_SOURCE);
    }
    return stageBank(server,bank,size,source,0);
}

int FsServer_Transaction(FsServer *server,enum FsTransactionAction action)
{
    if (!usable(server)) return FS_INVALID;
    const FsTransactionIo *io=server->transactions;
    uint32_t owner=server->owner;
    int result;
    switch (action) {
    case FS_TRANSACTION_RECOVER: result=FsTransaction_Check(io); break;
    case FS_TRANSACTION_SOURCE_SAVED: result=FsTransaction_SourceSaved(io,owner); break;
    case FS_TRANSACTION_COMMIT: result=FsTransaction_Commit(io,owner); break;
    case FS_TRANSACTION_BEGIN_CLEANUP: result=FsTransaction_BeginCleanup(io,owner); break;
    case FS_TRANSACTION_CLEANUP_SAVED: result=FsTransaction_CleanupSaved(io,owner); break;
    case FS_TRANSACTION_FINISH:
        result=FsTransaction_Guard(io) || FsTransaction_Finish(io,owner); break;
    case FS_TRANSACTION_ROLLBACK: result=FsTransaction_Rollback(io,owner); break;
    case FS_TRANSACTION_FORCE_ROLLBACK:
    case FS_TRANSACTION_FORCE_BANK_ONLY_ROLLBACK:
    {
        FsTransactionRecord record;
        if (FsTransaction_ReadRecord(io,&record)!=FS_TRANSACTION_PRESENT) return FS_LOCKED;
        result=FsTransaction_ForceRollback(io,record.owner,
            action==FS_TRANSACTION_FORCE_BANK_ONLY_ROLLBACK);
        break;
    }
    default: return FS_UNSUPPORTED;
    }
    /* Failure can leave a persisted decision: inspect/recover, do not restage. */
    /* 失败可能已留下持久化决策，应查询或恢复，不能直接重新暂存。 */
    if (!result) return FS_LOCKED;
    return FS_OK;
}

int FsServer_WriteTurtle(FsServer *server,const void *data,uint32_t size,int activeSave)
{
    if (!usable(server)) return FS_INVALID;
    return FsTransaction_WriteTurtle(server->transactions,server->owner,data,size,activeSave)?
        FS_OK:FS_IO_ERROR;
}

int FsServer_UpdateTurtleRecord(FsServer *server,const void *data,uint32_t size,int activeSave)
{
    return usable(server)?FsSettings_UpdateRecord(server->transactions,server->owner,data,size,activeSave):FS_INVALID;
}

int FsServer_TransferAllowed(FsServer *server,int nativeTransferBoxEmpty)
{
    if (!usable(server) || server->owner!=FS_TRANSACTION_OWNER_MOVER) return FS_INVALID;
    FsTransactionFingerprint bank;
    int result;
    if (server->loaded.bankStructured &&
        FsTransaction_ViewGet(server->transactions,FS_TRANSACTION_BANK,0,&bank) && bank.present)
        result=FsTransaction_Guard(server->transactions)?FS_OK:
            (pendingBankUpdate(server)?FS_TRANSACTION_MISMATCH:FS_LOCKED);
    else {
        FsBankHeader header;
        result=FsServer_ReadBankRange(server,0,&header,sizeof(header));
    }
    /* The native client checks the loaded transfer box. The header previously
       read here supplied no additional decision once that Bank was validated. */
    /* 原客户端检查已载入的传送盒；银行已校验后，原先这里读取但未使用的头部
       不提供新的决策依据，无须因来源工作镜像覆盖 I/O 缓冲就重读银行正文。 */
    if (result!=FS_OK) return result;
    return nativeTransferBoxEmpty?FS_OK:FS_BOX_OCCUPIED;
}

int FsServer_CheckCandidates(FsServer *server,const FsMoverCandidate slots[30],
    uint32_t maximumSpecies,int32_t results[30],FsMoverExtraCheck extra,void *user)
{
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    if (server->configuration.moverError.result!=FS_OK)
        return server->configuration.moverError.result;
    if (server->owner!=FS_TRANSACTION_OWNER_MOVER ||
        !slots || !results || !maximumSpecies || maximumSpecies>1025) return FS_INVALID;
    for (uint32_t i=0;i<30;i++) {
        const FsMoverCandidate *slot=&slots[i];
        int32_t result;
        int checked=FsServer_CheckPokemon(server,slot,0,maximumSpecies,&result);
        if (checked!=FS_OK) return checked;
        if (result==FS_SLOT_ACCEPTED && extra && !extra(slot,i,user)) result=FS_SLOT_REJECTED;
        results[i]=result;
    }
    return FS_OK;
}

int FsServer_CheckPokemon(FsServer *server,const FsMoverCandidate *slot,const FsMoverDetails *details,
    uint32_t maximumSpecies,int32_t *out)
{
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    if (server->configuration.moverError.result!=FS_OK) return server->configuration.moverError.result;
    if (!FsMover_ValidateWords(&server->configuration.mover)) return FS_INVALID;
    if (server->owner!=FS_TRANSACTION_OWNER_MOVER || !slot || !out || !maximumSpecies ||
        maximumSpecies>1025 || (details && details->source!=FS_MOVER_GEN5 && details->source!=FS_MOVER_VC))
        return FS_INVALID;
    *out=FS_SLOT_REJECTED;
    if (!slot->structurallyValid || slot->excluded || slot->species>maximumSpecies) return FS_OK;
    if (!slot->species) { *out=FS_SLOT_EMPTY; return FS_OK; }
    if (!FsMover_CheckRules(&server->configuration.mover,slot->species,details)) return FS_OK;
    int named=FsMover_NameResult(&server->configuration.mover,details,server->shopRegion);
    if (named<0) return named;
    *out=named;
    return FS_OK;
}

int FsServer_Home(FsServer *server)
{
    (void)server;
    return FS_UNSUPPORTED;
}

int FsServer_QueryRemoteStatus(FsServer *server,FsRemoteStatus *out)
{
    if (usable(server) && (server->bankDelivered || !server->loaded.bodyValid ||
        server->loaded.bodyFile!=FS_TRANSACTION_BANK)) {
        /* A new native load observes a new file snapshot. Source selection
           evidence is separate and must not be lost when reloading Bank. */
        /* 新一轮原生载入重新观察银行快照；来源选择证据独立，不随银行重载丢失。 */
        for (uint32_t file=FS_TRANSACTION_BANK;file<=FS_TRANSACTION_BACKUP;file++)
            FsTransaction_ViewForget(server->transactions,file);
        server->bankDelivered=0;
    }
    if (!out) return FS_INVALID;
    int prepared=prepareBank(server,0);
    if (prepared!=FS_OK && !(prepared==FS_MISSING && usable(server)))
        return prepared;
    int result;
    FsTransactionParameters descriptor;
    FsTransactionRecord record;
    for (uint32_t i=0;i<sizeof(descriptor);i++) ((uint8_t *)&descriptor)[i]=0;
    uint32_t owner=server->owner;
    /* First use has no Bank descriptor yet. The native creator supplies the
       initial body; do not require an existing Bank to reach that creator. */
    /* 首次使用尚无银行描述符；原版创建器提供初始正文，不能先要求银行存在。 */
    if (prepared!=FS_MISSING) {
        result=FsServer_DescribeTransaction(server,&descriptor);
        if (result!=FS_OK) return result;
        if (FsTransaction_ReadRecord(server->transactions,&record)!=FS_TRANSACTION_PRESENT)
            return FS_IO_ERROR;
        owner=record.owner;
    }
    out->dataId=descriptor.dataId;
    out->currentVersion=descriptor.currentVersion;
    out->updateVersion=descriptor.updateVersion;
    out->size=descriptor.size;
    out->transactionPassword=descriptor.token;
    out->bankReady=1;
    /* Both native clients use '4' for a pending reservation. The owner is the
       producing client, not the querying client or a recovery-permission bit. */
    /* 两端均以 '4' 表示未决预留；所属方是产生事务的软件，不是查询方或允许恢复标志。 */
    out->replyMode=prepared==FS_MISSING && owner==FS_TRANSACTION_OWNER_BANK?'5':(descriptor.token?'4':'2');
    out->transactionOwner=owner==FS_TRANSACTION_OWNER_MOVER?1:0;
    /* Status/metadata queries belong to the upcoming download, not another
       complete body load. Only delivery of the Bank body consumes this view;
       the next load after that, or a real upload, re-observes physical files. */
    /* 状态／元数据查询属于即将进行的下载，不另开正文载入；实际提供银行正文后
       才消费此视图。其后的新载入及实际上传重新观察物理文件。 */
    return FS_OK;
}

int FsServer_QueryRecordStatus(FsServer *server,FsRecordStatus *out)
{
    if (!out || !usable(server) || server->owner!=FS_TRANSACTION_OWNER_BANK)
        return FS_INVALID;
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    /* No local account migration/service notice is configured. Missing Bank
       data is reported by the following transaction query as first use. */
    /* 本地未配置账户迁移／服务公告；银行缺失由下一次事务查询返回首次使用模式。 */
    out->fileStatus=0;
    out->supportCode=0;
    return FS_OK;
}


int FsServer_Guard(FsServer *server)
{
    if (!usable(server)) return FS_INVALID;
    return FsTransaction_Guard(server->transactions)?FS_OK:FS_LOCKED;
}


static int inspectBank(const FsTransactionIo *io,uint32_t file)
{
    uint32_t handle=0; uint64_t size=0;
    int status=io->openRead(file,0,&handle,&size);
    if (!status) return FS_MISSING;
    if (status<0) return FS_IO_ERROR;
    int result=inspectBankHandle(io,handle,size);
    return io->close(handle)?result:FS_IO_ERROR;
}

static int prepareBank(FsServer *server,FsTransactionFingerprint *snapshot)
{
    if (snapshot) *snapshot=(FsTransactionFingerprint){0};
    if (!usable(server)) return FS_INVALID;
    const FsTransactionIo *io=server->transactions;
    FsTransactionRecord local;
    int localState=FsTransaction_ReadRecord(io,&local);
    if (localState==FS_TRANSACTION_MISSING && !FsTransaction_ReconcileOrphanParticipants(io)) return FS_LOCKED;
    if (localState==FS_TRANSACTION_PRESENT &&
        (local.operation==FS_TRANSACTION_SOURCE_ONLY || local.operation==FS_TRANSACTION_SETTINGS_ONLY) &&
        local.phase!=FS_TRANSACTION_COMPLETED && local.phase!=FS_TRANSACTION_ABORTED &&
        local.phase!=FS_TRANSACTION_DETACHED_ROLLED_BACK && !FsTransaction_Check(io)) return FS_LOCKED;
    /* Finish a local participant decision before classifying an unrelated
       Bank. A source/settings-only bootstrap may have no Bank baseline. */
    /* 判别无关银行之前先完成本地参与文件决定；来源／设置单独初始化可能没有
       银行基线，不能因其摘要为空就提前废弃本地恢复证据。 */
    /* A missing settings file must not turn an unchanged Bank into an import.
       Failure here is reconsidered only if the Bank itself is unrelated. */
    /* 设置缺失不等于换入银行；仅正文自身无关时才允许另行登记新银行。 */
    (void)FsTransaction_ReconcileTurtle(io);
    FsTransactionRecord record;
    int state=FsTransaction_ReadRecord(io,&record);
    if (state==FS_TRANSACTION_IO_ERROR) return FS_LOCKED;
    {
        BankInspection inspection={0};
        FsTransactionFingerprint observed;
        int known=server->loaded.bankStructured &&
            FsTransaction_ViewGet(io,FS_TRANSACTION_BANK,&record,&observed);
        int found=known?(observed.present?FS_TRANSACTION_PRESENT:FS_TRANSACTION_MISSING):
            FsTransaction_ObserveChecked(io,FS_TRANSACTION_BANK,&record,&observed,inspectBankBlock,&inspection);
        if (found==FS_TRANSACTION_PRESENT) server->loaded.bankStructured=1;
        /* One stream supplies structural validation and the loaded fingerprint.
           Mutations, failed I/O and the next native load invalidate that view. */
        /* 一份读取流同时完成结构检查及已载入摘要；修改、I/O 失败及下一次原生
           载入使该视图失效。 */
        if (found==FS_TRANSACTION_PRESENT && FsTransaction_GuardBank(io,&observed)) {
            if (state==FS_TRANSACTION_MISSING &&
                !FsTransaction_AdoptBankSnapshot(io,server->owner,&observed)) return FS_LOCKED;
            if (snapshot) *snapshot=observed;
            return FS_OK;
        }
        /* A valid main differing from BOTH pending images is an import, not
           a normal publication gap. Retire the old service reservation; the
           native client still owns cleanup of any obsolete game-side token. */
        /* 合法主正文与未决前／后版本均不同才视为导入，不把正常发布误判为换档。
           只取消旧服务预留，游戏残留令牌仍由原客户端恢复回调清理。 */
        int ended=state==FS_TRANSACTION_PRESENT &&
            (record.phase==FS_TRANSACTION_COMPLETED || record.phase==FS_TRANSACTION_ABORTED ||
             record.phase==FS_TRANSACTION_DETACHED_ROLLED_BACK);
        if (found==FS_TRANSACTION_PRESENT && (state==FS_TRANSACTION_MISSING ||
            (ended && !sameFingerprint(&observed,&record.bankAfter)) ||
            (!ended && !sameFingerprint(&observed,&record.bankBefore) &&
             !sameFingerprint(&observed,&record.bankAfter))))
        {
            if (!FsTransaction_AdoptBankSnapshot(io,server->owner,&observed)) return FS_LOCKED;
            if (snapshot) *snapshot=observed;
            return FS_OK;
        }
        if (found==FS_TRANSACTION_IO_ERROR && inspection.result==FS_OK) return FS_LOCKED;
        if (found==FS_TRANSACTION_MISSING && state==FS_TRANSACTION_MISSING) {
            BankInspection saved={0};
            FsTransactionFingerprint backup;
            int candidate=FsTransaction_ObserveChecked(io,FS_TRANSACTION_BACKUP,0,
                &backup,inspectBankBlock,&saved);
            if (candidate==FS_TRANSACTION_PRESENT) {
                /* No old descriptor is reconstructed from .bak: publish a new
                   baseline decision, then restore this structurally checked body. */
                /* 无日志时不声称还原旧事务；先登记新基线决定，再恢复已校验的备份正文。 */
                if (!FsTransaction_AdoptBankBackupSnapshot(io,server->owner,&backup)) return FS_LOCKED;
                if (snapshot) *snapshot=backup;
                return FS_OK;
            }
            if (candidate==FS_TRANSACTION_IO_ERROR)
                return saved.result==FS_OK?FS_LOCKED:FS_INVALID;
            if (!FsTransaction_ResetOrphanBank(io,server->owner)) return FS_LOCKED;
            return FsTransaction_Check(io)?FS_MISSING:FS_LOCKED;
        }
        if (found==FS_TRANSACTION_MISSING && state==FS_TRANSACTION_PRESENT &&
            (record.bankBefore.present || record.bankAfter.present || record.serverTransaction.token) &&
            FsTransaction_AdoptBankSnapshot(io,server->owner,0)) return FS_MISSING;
    }
    int checked=FsTransaction_Check(server->transactions);
    if (!checked) {
        if (!pendingBankUpdate(server)) return FS_LOCKED;
        /* Read-only existence check lets the native source-selection/recovery
           states run. Bank reads and all writes remain guarded until settled. */
        /* 只读存在性检查允许进入原版选游戏／恢复状态；未结束前仍禁止银行读取和写入。 */
        int result=inspectBank(server->transactions,FS_TRANSACTION_BANK);
        return result==FS_MISSING?FS_LOCKED:result;
    }
    int main=inspectBank(io,FS_TRANSACTION_BANK);
    if (main==FS_OK || main==FS_IO_ERROR) return main;
    int backup=inspectBank(io,FS_TRANSACTION_BACKUP);
    if (backup==FS_IO_ERROR) return backup;
    if (backup==FS_OK) {
        /* A published journal, if present, must authorize its own recovery.
           Missing-history backup adoption was handled separately above. */
        /* 已有正式日志仍须按其授权恢复；无日志的备份接纳已在上方单独处理。 */
        return FS_LOCKED;
    }
    if (main!=FS_MISSING || backup!=FS_MISSING) return FS_INVALID;
    return FS_MISSING;
}

int FsServer_PrepareBank(FsServer *server)
{
    return prepareBank(server,0);
}

int FsServer_WriteBank(FsServer *server,const void *data,uint32_t size)
{
    if (!usable(server) || !data || size!=FS_BANK_SIZE || !validBank(data,size)) return FS_INVALID;
    FsTransaction_ViewForget(server->transactions,FS_TRANSACTION_BANK);
    if (FsServer_Guard(server)!=FS_OK) return FS_LOCKED;
    return FsTransaction_Write(server->transactions,server->owner,
        FS_TRANSACTION_BANK_ONLY,data,size)?FS_OK:FS_IO_ERROR;
}

int FsSettings_Status(const FsTransactionIo *io)
{
    if (!io || !io->openRead || !io->read || !io->close) return FS_INVALID;
    if (!FsTransaction_TurtleGuard(io)) return FS_LOCKED;
    uint32_t handle=0; uint64_t size=0;
    int found=io->openRead(FS_TRANSACTION_TURTLE,0,&handle,&size);
    if (!found) return FS_MISSING;
    if (found<0) return FS_IO_ERROR;
    if (!io->close(handle)) return FS_IO_ERROR;
    return size==512?FS_OK:FS_INVALID;
}

int FsSettings_ReadRange(const FsTransactionIo *io,uint32_t offset,void *data,uint32_t size)
{
    if (!io || !io->openRead || !io->read || !io->close ||
        (!data && size) || offset>512 || size>512-offset) return FS_INVALID;
    if (!FsTransaction_TurtleGuard(io)) return FS_LOCKED;
    return FsSettings_ReadPreviewRange(io,offset,data,size);
}

int FsSettings_ReadPreviewRange(const FsTransactionIo *io,uint32_t offset,void *data,uint32_t size)
{
    if (!io || !io->openRead || !io->read || !io->close ||
        (!data && size) || offset>512 || size>512-offset) return FS_INVALID;
    uint32_t handle=0; uint64_t length=0;
    int found=io->openRead(FS_TRANSACTION_TURTLE,0,&handle,&length);
    if (!found) return FS_MISSING;
    if (found<0) return FS_IO_ERROR;
    int result=length!=512?FS_INVALID:
        (io->read(handle,offset,data,size)?FS_OK:FS_IO_ERROR);
    return io->close(handle)?result:FS_IO_ERROR;
}

int FsServer_TurtleStatus(FsServer *server)
{
    return usable(server)?FsSettings_Status(server->transactions):FS_INVALID;
}

int FsServer_ReadTurtleRange(FsServer *server,uint32_t offset,void *data,uint32_t size)
{
    return usable(server)?FsSettings_ReadRange(server->transactions,offset,data,size):FS_INVALID;
}

static int languageChange(const uint8_t record[512],uint32_t languageId,
    uint32_t useJapaneseKanji,int *needed)
{
    if (!FsSettings_ValidPrefix(record)) return FS_INVALID;
    *needed=record[0x28]!=(uint8_t)languageId || record[0x29] ||
        record[0x2a]!=(uint8_t)useJapaneseKanji || record[0x2b];
    return FS_OK;
}

int FsSettings_LanguageNeedsWrite(const FsTransactionIo *io,uint32_t languageId,
    uint32_t useJapaneseKanji,int *needed)
{
    if (!needed || !languageId || languageId>10 || languageId==6 || useJapaneseKanji>1)
        return FS_INVALID;
    *needed=0;
    uint8_t record[512];
    int result=FsSettings_ReadRange(io,0,record,sizeof(record));
    if (result==FS_MISSING) return FS_OK;
    return result==FS_OK?languageChange(record,languageId,useJapaneseKanji,needed):result;
}

int FsServer_UpdateTurtleLanguage(FsServer *server,uint32_t languageId,uint32_t useJapaneseKanji)
{
    return usable(server)?FsSettings_UpdateLanguage(server->transactions,server->owner,
        languageId,useJapaneseKanji):FS_INVALID;
}

static int workingSource(FsServer *server,FsTransactionRecord *record,uint32_t offset,uint32_t size)
{
    if (!usable(server)) return FS_INVALID;
    const FsTransactionIo *io=server->transactions;
    if (FsTransaction_ReadRecord(io,record)!=FS_TRANSACTION_PRESENT ||
        record->owner!=server->owner || !FsTransaction_SourceIsWorking(record) ||
        offset>record->sourceBefore.size || size>record->sourceBefore.size-offset)
        return FS_LOCKED;
    return FS_OK;
}

int FsServer_ReadSourceRange(FsServer *server,uint32_t offset,void *data,uint32_t size)
{
    if (!data && size) return FS_INVALID;
    FsTransactionRecord record;
    int result=workingSource(server,&record,offset,size);
    if (result!=FS_OK) return result;
    const FsTransactionIo *io=server->transactions;
    uint32_t handle=0; uint64_t length=0;
    if (io->openRead(FS_TRANSACTION_SOURCE_TEMP,&record,&handle,&length)!=FS_TRANSACTION_PRESENT) {
        FsTransaction_ViewClear(io);return FS_IO_ERROR;
    }
    result=length==record.sourceBefore.size && (!size || io->read(handle,offset,data,size))?
        FS_OK:FS_IO_ERROR;
    if (!io->close(handle)) result=FS_IO_ERROR;
    if (result!=FS_OK) FsTransaction_ViewClear(io);
    return result;
}

int FsServer_WriteSourceRange(FsServer *server,uint32_t offset,const void *data,uint32_t size)
{
    if (!data || !size) return FS_INVALID;
    FsTransactionRecord record;
    int result=workingSource(server,&record,offset,size);
    if (result!=FS_OK) return result;
    const FsTransactionIo *io=server->transactions;
    uint32_t handle=0; uint64_t length=0;
    if (io->openRead(FS_TRANSACTION_SOURCE_TEMP,&record,&handle,&length)!=FS_TRANSACTION_PRESENT) {
        FsTransaction_ViewClear(io);return FS_IO_ERROR;
    }
    if (!io->close(handle) || length!=record.sourceBefore.size) {
        FsTransaction_ViewClear(io);return FS_IO_ERROR;
    }
    if (!FsTransaction_OpenWrite(io,FS_TRANSACTION_SOURCE_TEMP,&record,&handle)) return FS_IO_ERROR;
    int written=io->write(handle,offset,data,size);
    result=io->close(handle) && written?FS_OK:FS_IO_ERROR;
    if (result!=FS_OK) FsTransaction_ViewClear(io);
    return result;
}

static int retryFailedPolicy(FsServer *server)
{
    int status=policyStatus(server);
    if (status!=FS_OK && usable(server)) {
        /* A retained storage connection is not proof that a prior policy
           read succeeded forever. A new admission request may retry a fixed
           file; ongoing I/O failure still fails, without resetting data. */
        /* 存储上下文仍在不等于旧配置错误永久有效。新准入请求可重读已修复文件；
           持续 I/O 错误仍失败，不清银行、日志或购买记录。 */
        uint32_t applied=0;
        status=FsServer_ReloadConfiguration(server,&applied);
    }
    return status;
}

int FsServer_Authenticate(FsServer *server,FsAccountResponse *out)
{
    int status=retryFailedPolicy(server);
    if (status!=FS_OK) return status;
    if (!out) return FS_INVALID;
    /* Account zero, no maintenance, accepted locally. Reserved private-server
       identity policy; this never contacts NNID. */
    /* 账户为零、无维护、默认通过；预留私服身份策略，不连接 NNID。 */
    out->accountId[0]=0; out->accountId[1]=0;
    out->authenticated=1; out->maintenance=0;
    return FS_OK;
}

int FsServer_ValidateTicket(FsServer *server,uint32_t *accepted)
{
    int status=retryFailedPolicy(server);
    if (status!=FS_OK) return status;
    if (!accepted) return FS_INVALID;
    /* Offline authenticity is deliberately accepted, not signature-verified. */
    /* 离线真实性默认通过，并未验证官方签名。 */
    *accepted=1;
    return FS_OK;
}

void FsServer_Connection(FsServer *server,uint32_t disconnect,FsConnectionResponse *out)
{
    if (!out) return;
    out->result=2; out->errorCode=0;
    if (disconnect) {
        FsServer_Disconnect(server); out->result=1; return;
    }
    FsAccountResponse account;
    if (FsServer_Authenticate(server,&account)==FS_OK &&
        account.authenticated && !account.maintenance) out->result=1;
}

int FsServer_InitialValues(FsServer *server,uint32_t values[2])
{
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    if (!values || !server->transactions->randomBytes) return FS_INVALID;
    /* Local first-use values, not official account credentials. The original
       client incorporates them into the bank body before requesting creation. */
    /* 本地首次使用值，不是官方账户凭据；原客户端先写入银行正文再请求创建。 */
    for (uint32_t attempt=0;attempt<4;attempt++) {
        if (!server->transactions->randomBytes(values,8)) return FS_IO_ERROR;
        if (values[0] || values[1]) return FS_OK;
    }
    return FS_IO_ERROR;
}

int FsServer_VerifyTicket(FsServer *server,uint32_t minimumDays)
{
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    return FsTicket_Allowed(&server->ticket,minimumDays)?FS_OK:FS_UNSUPPORTED;
}

int FsServer_NoSave(FsServer *server)
{
    if (!usable(server)) return FS_INVALID;
    FsTransactionRecord record;
    int state=FsTransaction_ReadRecord(server->transactions,&record);
    int result;
    if (state==FS_TRANSACTION_MISSING) {
        return FsServer_Guard(server);
    }
    if (state!=FS_TRANSACTION_PRESENT) return FS_LOCKED;
    if (record.phase==FS_TRANSACTION_COMPLETED || record.phase==FS_TRANSACTION_ABORTED ||
        record.phase==FS_TRANSACTION_DETACHED_ROLLED_BACK) result=FsServer_Guard(server);
    else result=FsServer_Transaction(server,FS_TRANSACTION_ROLLBACK);
    return result;
}

void FsServer_Disconnect(FsServer *server)
{
    if (server) {
        FsTransaction_ViewClear(server->transactions);
        FsConfiguration_Release(server->storage,&server->configuration);
        server->ready=0; server->bankDownloadToken=0;
        server->rollbackCodes.candidateCount=0; server->rollbackAuthorized=0;
    }
}

int FsServer_NameResult(FsServer *server,uint32_t *result)
{
    int status=policyStatus(server);
    if (status!=FS_OK) return status;
    if (!result) return FS_INVALID;
    /* Reserved name policy, currently keeps both names unchanged. */
    /* 预留姓名策略，当前保持昵称和原训练家名不变。 */
    *result=FS_NAME_UNCHANGED;
    return FS_OK;
}

int FsServer_GiftDeliverySupport(FsServer *server)
{
    if (!usable(server) || server->owner!=FS_TRANSACTION_OWNER_BANK) return FS_INVALID;
    return FsTransaction_Guard(server->transactions) || liveBankDownload(server)?FS_OK:FS_LOCKED;
}
