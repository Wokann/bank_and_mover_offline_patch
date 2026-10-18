#ifndef COMMON_FAKE_SERVER_TICKET_H
#define COMMON_FAKE_SERVER_TICKET_H

#include "types.h"

typedef struct FsTicketPolicy {
    uint32_t freeMode;
    uint32_t purchased;
    uint32_t starts; /* Custom free window, YYYYMMDD / 自定义免费起始日期。 */
    uint32_t expires; /* Custom free window, YYYYMMDD / 自定义免费截止日期。 */
    uint32_t paidStarts,paidExpires; /* Independent local pass / 独立的本地券期限。 */
    uint32_t trialCount,annualCount;
} FsTicketPolicy;

/* Independent local currencies, in their smallest unit; no exchange rate. */
/* 本地币种各自使用最小金额单位，不做汇率换算。 */
enum FsShopMarket {
    FS_SHOP_JP, FS_SHOP_US, FS_SHOP_GB, FS_SHOP_EU, FS_SHOP_KR, FS_SHOP_TW,
    FS_SHOP_HK, FS_SHOP_AU, FS_SHOP_NZ, FS_SHOP_CA, FS_SHOP_CH, FS_SHOP_ZA,
    FS_SHOP_MARKET_COUNT, FS_SHOP_UNKNOWN=0xffffffffu,
    FS_SHOP_MONEY_LIMIT=0x7fffffffu,
    FS_SHOP_NONE_DEFAULT_PRICE=807
};
typedef struct FsShopPolicy { uint32_t balance[FS_SHOP_MARKET_COUNT]; } FsShopPolicy;

/* Runtime CONF settings. The existing on-disk transaction binding is unchanged. */
/* CONF 运行时设置，不改变已有事务绑定的磁盘格式。 */
enum FsTicketSettingsConstant {
    FS_TICKET_PERIOD_LIMIT=24, FS_TICKET_REGION_FIELDS=9,
    FS_TICKET_FIELD_FREE=0, FS_TICKET_FIELD_TRIAL=9, FS_TICKET_FIELD_PAID=18,
    FS_TICKET_FIELD_PURCHASED=27, FS_TICKET_FIELD_TRIAL_CLAIMED=28,
    FS_TICKET_FIELD_ANNUAL_COUNT=29, FS_TICKET_FIELD_AUTOCURRENCY=30,
    FS_TICKET_FIELD_CURRENT_CURRENCY=31, FS_TICKET_SETTING_MONEY=32,
    FS_TICKET_FIELD_FREE_ENABLE=44, FS_TICKET_FIELD_TRIAL_ENABLE=45,
    FS_TICKET_FIELD_NONE_SHOP=46,
    FS_TICKET_SETTING_FIELDS=47, FS_TICKET_LEGACY_FIELDS=43
};
typedef struct FsTicketPeriod { uint32_t starts,ends,field; } FsTicketPeriod;
typedef struct FsTicketSettings {
    uint32_t present,freeEnable,trialEnable,periodCount;
    FsTicketPeriod periods[FS_TICKET_PERIOD_LIMIT];
    uint32_t price[FS_SHOP_MARKET_COUNT],recharge[FS_SHOP_MARKET_COUNT];
    uint32_t autocurrency,currentMarket;
    uint32_t ready;
    uint32_t noneShop[3]; /* Local unitless price, recharge, balance / 无币种本地商店。 */
} FsTicketSettings;
/* Stock catalog copies a 32-byte amount string; not an eShop receipt. */
/* 原版目录复制 32 字节金额字符串，不是 eShop 回执。 */
typedef struct FsTicketAmount {
    char text[32];
} FsTicketAmount;

enum FsTicketFreeMode {
    FS_TICKET_FREE_NONE=0,
    FS_TICKET_FREE_TRIAL=1,
    FS_TICKET_FREE_OFFICIAL=2,
    FS_TICKET_FREE_CUSTOM=3
};

enum FsTicketCampaignConstant {
    FS_TICKET_OFFICIAL_START=20230214,
    FS_TICKET_OFFICIAL_END=20501231,
    FS_TICKET_ONLINE_START=20000101,
    FS_TICKET_ONLINE_END=20991230
};

enum FsTicketProduct {
    FS_TICKET_TRIAL=0, FS_TICKET_ANNUAL=1, FS_TICKET_PRODUCT_COUNT=2,
    FS_TICKET_TRIAL_DAYS=30, FS_TICKET_ANNUAL_DAYS=365,
    FS_TICKET_REPLY_CAPACITY=160
};

/* Match the receiving SDK's text mode, without changing its global settings. */
/* 匹配接收方 SDK 的文本编码，不修改其全局设置。 */
enum FsTicketTextEncoding { FS_TICKET_TEXT_UTF16=0, FS_TICKET_TEXT_UTF8=1 };

/* Job-local routing: online fallback never uses the offline account policy. */
/* 作业级路由：联网回退不使用离线账户策略。 */
enum FsTicketBackend {
    FS_TICKET_BACKEND_NATIVE=0, FS_TICKET_BACKEND_LOCAL=1,
    FS_TICKET_BACKEND_ONLINE_FREE=2
};

typedef struct FsTicketShop {
    uint32_t available[FS_TICKET_PRODUCT_COUNT];
    char itemCode[FS_TICKET_PRODUCT_COUNT][17];
    uint32_t market,price[FS_TICKET_PRODUCT_COUNT],balance;
} FsTicketShop;

typedef struct FsTicketResponse {
    uint32_t currentDate[2];
    uint32_t expiryDate[2];
    int32_t purchaseCounts[5];
    uint64_t campaignStart;
    uint64_t campaignEnd;
    uint32_t valid;
    uint32_t freeActivity;
} FsTicketResponse;

/* Decoded native free_campaign body: calendar bytes, not a BOSS container. */
/* 原生 free_campaign 解封装正文：日历字节，不是 BOSS 容器。 */
typedef struct FsCampaignDate {
    uint8_t year[2];
    uint8_t month,day,hour,minute,second;
} FsCampaignDate;
typedef struct FsCampaignRecord {
    uint8_t magic[4];
    FsCampaignDate start,end;
    uint8_t checksum[2];
} FsCampaignRecord;

#define FS_FREE_CAMPAIGN_ADDRESS "fake://bank/free_campaign"

int FsDate_Valid(uint32_t date);
int FsDate_Parse(const char *text,uint32_t *date);
int FsDate_ParsePeriod(const char *text,uint32_t length,uint32_t *start,uint32_t *end);
uint32_t FsDate_FromPacked(const uint32_t packed[2]);
int FsTicket_Build(const FsTicketPolicy *,const uint32_t now[2],FsTicketResponse *);
int FsTicket_Allowed(const FsTicketResponse *,uint32_t minimumDays);
int FsTicket_Purchase(const FsTicketPolicy *,const uint32_t now[2],uint32_t product,FsTicketPolicy *);
int FsTicket_Select(const FsTicketSettings *,const uint32_t now[2],uint32_t region,
    uint32_t country,FsTicketPolicy *);
uint32_t FsTicket_PeriodField(const FsTicketSettings *,uint32_t base,uint32_t region,uint32_t country);
void FsTicket_Shop(const FsTicketPolicy *,FsTicketShop *);
uint32_t FsTicket_Market(uint32_t region,uint32_t country);
uint32_t FsTicket_AnnualPrice(uint32_t market);
const char *FsTicket_WalletKey(uint32_t market);
void FsTicket_Amount(FsTicketAmount *,uint32_t market,uint32_t units);
int FsTicket_FormatAmount(const FsTicketAmount *,char *text,uint32_t capacity);
/* Implemented EC operations requiring account policy. Other operations keep
   their immediate unsupported response without opening storage. */
/* 需要账户策略的已实现 EC 操作；其他操作保持立即“不支持”响应，不打开存储。 */
int FsTicket_QueryOperation(uint32_t operation);
/* EC service response bodies, before the original SDK parser consumes them. */
/* EC 服务响应正文，由原 SDK 解析器继续消费。 */
int FsTicket_Reply(const FsTicketResponse *,uint32_t operation,void *body,
    uint32_t capacity,uint32_t *size,uint32_t textEncoding);
void FsCampaign_Encode(const FsTicketPolicy *,const FsTicketResponse *,FsCampaignRecord *);
/* Read-only online fallback: independent fixed window, no products or history. */
/* 只读联网回退：独立固定免费窗口，不提供商品或购买历史。 */
int FsTicket_OnlineFallback(const uint32_t now[2],FsTicketResponse *);
int FsTicket_OnlineReply(const FsTicketResponse *,uint32_t operation,void *body,
    uint32_t capacity,uint32_t *size,uint32_t textEncoding);
void FsCampaign_OnlineFallback(FsCampaignRecord *);

#endif
