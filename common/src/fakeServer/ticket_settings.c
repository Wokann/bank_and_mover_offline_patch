#include "fakeServer/configuration.h"
#include "fakeServer/numbers.h"

/* Regional keys replace, rather than extend, the common schedule. */
/* 地区键替代通用日期；历史试用地区项与其他基础项一起内置。 */
static const char *const keys[FS_TICKET_SETTING_MONEY]={
    "free_periods","free_periods.JPN","free_periods.USA","free_periods.EUR",
    "free_periods.AUS","free_periods.CHN","free_periods.KOR","free_periods.TWN","free_periods.HK",
    "trial_periods","trial_periods.JPN","trial_periods.USA","trial_periods.EUR",
    "trial_periods.AUS","trial_periods.CHN","trial_periods.KOR","trial_periods.TWN","trial_periods.HK",
    "paid_periods","paid_periods.JPN","paid_periods.USA","paid_periods.EUR",
    "paid_periods.AUS","paid_periods.CHN","paid_periods.KOR","paid_periods.TWN","paid_periods.HK",
    "purchased","trial_claimed","annual_count","autocurrency","currentCurrency"
};
static const char *const shopDefaults[FS_SHOP_MARKET_COUNT]={
    "500,500,0","499,499,0","449,449,0","499,499,0","5000,5000,0","150,150,0",
    "4000,4000,0","650,650,0","850,850,0","699,699,0","700,700,0","6000,6000,0"
};
static int equal(const char *a,const char *b)
{
    while (*a && *a==*b) { a++; b++; }
    return *a==*b;
}
const char *FsConfiguration_TicketKey(uint32_t field)
{
    if (field>=FS_TICKET_SETTING_FIELDS) return 0;
    if (field==FS_TICKET_FIELD_FREE_ENABLE) return "free_enable";
    if (field==FS_TICKET_FIELD_TRIAL_ENABLE) return "trial_enable";
    if (field==FS_TICKET_FIELD_NONE_SHOP) return "shop.$NONE";
    return field<FS_TICKET_SETTING_MONEY?keys[field]:FsTicket_WalletKey(field-FS_TICKET_SETTING_MONEY);
}
uint32_t FsConfiguration_TicketOrder(uint32_t position)
{
    static const uint8_t order[]={44,0,1,2,3,4,5,6,7,8,45,9,10,11,12,13,14,15,16,17,
        30,31,46,32,33,34,35,36,37,38,39,40,41,42,43,27,18,19,20,21,22,23,24,25,26,28,29};
    return position<sizeof(order)?order[position]:FS_TICKET_SETTING_FIELDS;
}
const char *FsConfiguration_TicketValue(uint32_t field)
{
    if (field>=FS_TICKET_SETTING_FIELDS) return 0;
    if (field==FS_TICKET_FIELD_FREE_ENABLE || field==FS_TICKET_FIELD_TRIAL_ENABLE) return "true";
    if (field==FS_TICKET_FIELD_NONE_SHOP) return "807,807,0";
    if (field>=FS_TICKET_SETTING_MONEY) return shopDefaults[field-FS_TICKET_SETTING_MONEY];
    if (field==FS_TICKET_FIELD_AUTOCURRENCY) return "true";
    if (field==FS_TICKET_FIELD_CURRENT_CURRENCY) return "$NONE";
    if (field==FS_TICKET_FIELD_FREE) return "2020-02-12..2020-03-12,2023-02-14..2050-12-31";
    if (field>=FS_TICKET_FIELD_TRIAL && field<FS_TICKET_FIELD_PAID && field!=FS_TICKET_FIELD_TRIAL+5) {
        if (field==FS_TICKET_FIELD_TRIAL+2) return "2014-02-05..2014-03-14";
        if (field==FS_TICKET_FIELD_TRIAL+3 || field==FS_TICKET_FIELD_TRIAL+4) return "2014-02-04..2014-03-14";
        return "2013-12-25..2014-02-21";
    }
    if (field<FS_TICKET_FIELD_PURCHASED) return "0000-00-00..0000-00-00";
    return field==FS_TICKET_FIELD_ANNUAL_COUNT?"0":"false";
}
int FsConfiguration_TicketField(const char *key)
{
    if (key) for (uint32_t i=0;i<FS_TICKET_SETTING_FIELDS;i++)
        if (equal(key,FsConfiguration_TicketKey(i))) return (int)i;
    return -1;
}
const char *FsConfiguration_TicketDefault(const char *key)
{
    int field=FsConfiguration_TicketField(key);
    return field<0?0:FsConfiguration_TicketValue((uint32_t)field);
}
int FsConfiguration_TicketRequired(uint32_t field)
{
    return field<FS_TICKET_SETTING_FIELDS && (field>=FS_TICKET_FIELD_PURCHASED ||
        field==FS_TICKET_FIELD_FREE || field==FS_TICKET_FIELD_PAID ||
        (field>=FS_TICKET_FIELD_TRIAL && field<FS_TICKET_FIELD_PAID && field!=FS_TICKET_FIELD_TRIAL+5));
}
int FsConfiguration_TicketLegacyField(const char *key)
{
    static const char *const legacy[]={"free_mode","starts","expires","mode","paid_starts","paid_expires","trial_count"};
    for (uint32_t i=0;i<7;i++) if (equal(key,legacy[i])) return (int)i;
    static const char *const prefixes[]={"price_","recharge_","balance_"};
    for (uint32_t kind=0;kind<3;kind++) {
        uint32_t n=0;
        while (prefixes[kind][n] && key[n]==prefixes[kind][n]) n++;
        if (prefixes[kind][n]) continue;
        const char *currency=key+n;
        for (uint32_t market=0;market<FS_SHOP_MARKET_COUNT;market++) {
            const char *code=FsTicket_WalletKey(market)+6;
            if (currency[0] && currency[0]==code[0]+32 && currency[1] && currency[1]==code[1]+32 &&
                currency[2] && currency[2]==code[2]+32 && !currency[3])
                return (int)(7+market*3+kind);
        }
    }
    return -1;
}
static void removePeriods(FsTicketSettings *settings,uint32_t field)
{
    uint32_t out=0,previous=settings->periodCount;
    for (uint32_t i=0;i<previous;i++)
        if (settings->periods[i].field!=field) settings->periods[out++]=settings->periods[i];
    settings->periodCount=out;
    while (out<previous) settings->periods[out++]=(FsTicketPeriod){0,0,0};
}
static int periods(FsTicketSettings *settings,uint32_t field,const char *value)
{
    int empty=equal(value,"0000-00-00..0000-00-00");
    /* Validate before replacing: a bad duplicate must not erase a valid window. */
    /* 先验证再替换，非法重复项不能清掉先前有效的日期区间。 */
    if (!empty) {
        uint32_t count=0;
        for (uint32_t i=0;i<settings->periodCount;i++)
            if (settings->periods[i].field!=field) count++;
        const char *cursor=value;
        for (;;) {
            uint32_t length=0,start,end;
            while (cursor[length] && cursor[length]!=',') length++;
            if (!FsDate_ParsePeriod(cursor,length,&start,&end)) return FS_INVALID;
            if (++count>FS_TICKET_PERIOD_LIMIT) return FS_LIMIT;
            cursor+=length;
            if (!*cursor) break;
            cursor++;
        }
    }
    removePeriods(settings,field);
    if (!empty) for (;;) {
        uint32_t length=0,start=0,end=0;
        while (value[length] && value[length]!=',') length++;
        (void)FsDate_ParsePeriod(value,length,&start,&end);
        settings->periods[settings->periodCount++]=(FsTicketPeriod){start,end,field};
        value+=length;
        if (!*value) break;
        value++;
    }
    settings->present|=1u<<field;
    return FS_OK;
}
uint32_t *FsConfiguration_ShopValue(FsConfiguration *config,uint32_t market,uint32_t kind)
{
    if (!config || kind>=3) return 0;
    if (market==FS_SHOP_UNKNOWN) return config->ticketSettings.noneShop+kind;
    if (market>=FS_SHOP_MARKET_COUNT) return 0;
    return kind==0?config->ticketSettings.price+market:
        kind==1?config->ticketSettings.recharge+market:config->shop.balance+market;
}
static int shop(FsConfiguration *config,uint32_t market,const char *value,int repair)
{
    uint32_t values[3];
    for (uint32_t kind=0;kind<3;kind++) {
        const char *end=value;
        while (*end && *end!=',') end++;
        const char *begin=value;
        while (begin<end && (*begin==' ' || *begin=='\t')) begin++;
        const char *last=end;
        while (last>begin && (last[-1]==' ' || last[-1]=='\t')) last--;
        char number[11]; uint32_t size=(uint32_t)(last-begin);
        int valid=size && size<sizeof(number);
        if (valid) {
            for (uint32_t i=0;i<size;i++) number[i]=begin[i];
            number[size]=0; valid=FsParseU32(number,FS_SHOP_MONEY_LIMIT,values+kind);
        }
        if (!valid) {
            if (!repair) return FS_INVALID;
            values[kind]=kind<2?FsTicket_AnnualPrice(market):0;
        }
        if (((kind<2 && !*end) || (kind==2 && *end)) && !repair) return FS_INVALID;
        value=*end?end+1:end;
    }
    for (uint32_t kind=0;kind<3;kind++) *FsConfiguration_ShopValue(config,market,kind)=values[kind];
    return FS_OK;
}
int FsConfiguration_ParseTicket(FsConfiguration *config,uint32_t field,const char *value)
{
    if (!config || !value || field>=FS_TICKET_SETTING_FIELDS) return FS_INVALID;
    if (field<FS_TICKET_FIELD_PURCHASED) return periods(&config->ticketSettings,field,value);
    if (field==FS_TICKET_FIELD_NONE_SHOP) return shop(config,FS_SHOP_UNKNOWN,value,0);
    if (field>=FS_TICKET_SETTING_MONEY && field<FS_TICKET_FIELD_FREE_ENABLE)
        return shop(config,field-FS_TICKET_SETTING_MONEY,value,0);
    if (field==FS_TICKET_FIELD_CURRENT_CURRENCY) {
        if (equal(value,"$NONE")) { config->ticketSettings.currentMarket=FS_SHOP_UNKNOWN; return FS_OK; }
        for (uint32_t market=0;market<FS_SHOP_MARKET_COUNT;market++)
            if (equal(value,FsTicket_WalletKey(market)+5)) {
                config->ticketSettings.currentMarket=market; return FS_OK;
            }
        return FS_INVALID;
    }
    uint32_t *target=field==FS_TICKET_FIELD_PURCHASED?&config->ticket.purchased:
        field==FS_TICKET_FIELD_TRIAL_CLAIMED?&config->ticket.trialCount:
        field==FS_TICKET_FIELD_FREE_ENABLE?&config->ticketSettings.freeEnable:
        field==FS_TICKET_FIELD_TRIAL_ENABLE?&config->ticketSettings.trialEnable:
        field==FS_TICKET_FIELD_AUTOCURRENCY?&config->ticketSettings.autocurrency:&config->ticket.annualCount;
    if (field!=FS_TICKET_FIELD_ANNUAL_COUNT) {
        if (!equal(value,"true") && !equal(value,"false")) return FS_INVALID;
        *target=equal(value,"true"); return FS_OK;
    }
    return FsParseU32(value,0x7fffffffu,target)?FS_OK:FS_INVALID;
}
int FsConfiguration_RepairTicket(FsConfiguration *config,uint32_t field,const char *value)
{
    if (field==FS_TICKET_FIELD_NONE_SHOP) return shop(config,FS_SHOP_UNKNOWN,value,1);
    if (field>=FS_TICKET_SETTING_MONEY && field<FS_TICKET_FIELD_FREE_ENABLE)
        return shop(config,field-FS_TICKET_SETTING_MONEY,value,1);
    if ((field==FS_TICKET_FIELD_PURCHASED || field==FS_TICKET_FIELD_TRIAL_CLAIMED) &&
        (equal(value,"0") || equal(value,"1")))
        return FsConfiguration_ParseTicket(config,field,equal(value,"1")?"true":"false");
    /* Old date switches are migrated once; newly written files contain dates only. */
    /* 旧日期开关仅在加载时迁移；新配置不再用开关值表示日期。 */
    if (field<FS_TICKET_FIELD_PURCHASED) {
        if (equal(value,"disable") || equal(value,"none"))
            return FsConfiguration_ParseTicket(config,field,
                field==FS_TICKET_FIELD_FREE || field==FS_TICKET_FIELD_TRIAL?
                FsConfiguration_TicketValue(field):"0000-00-00..0000-00-00");
        if (equal(value,"enable") && field<FS_TICKET_FIELD_PAID)
            return FsConfiguration_ParseTicket(config,field,FsConfiguration_TicketValue(
                field<FS_TICKET_FIELD_TRIAL?FS_TICKET_FIELD_FREE:field));
    }
    return FsConfiguration_ParseTicket(config,field,FsConfiguration_TicketValue(field));
}
static uint32_t formatNumber(char *text,uint32_t value,int date)
{
    char reverse[10]; uint32_t size=0,out=0;
    do { uint32_t digit; value=FsDivideU32(value,10,&digit); reverse[size++]=(char)('0'+digit); } while (value);
    if (date) while (size<8) reverse[size++]='0';
    while (size) {
        text[out++]=reverse[--size];
        if (date && (size==4 || size==2)) text[out++]='-';
    }
    text[out]=0; return out;
}
int FsConfiguration_FormatTicket(const FsConfiguration *config,uint32_t field,char *text)
{
    if (!config || !text || field>=FS_TICKET_SETTING_FIELDS) return FS_INVALID;
    uint32_t size=0;
    if (field<FS_TICKET_FIELD_PURCHASED) {
        for (uint32_t i=0;i<config->ticketSettings.periodCount;i++) {
            const FsTicketPeriod *period=config->ticketSettings.periods+i;
            if (period->field!=field) continue;
            if (size+23>=FS_CONFIG_LINE_SIZE) return FS_LIMIT;
            if (size) text[size++]=',';
            size+=formatNumber(text+size,period->starts,1);
            text[size++]='.'; text[size++]='.';
            size+=formatNumber(text+size,period->ends,1);
        }
        if (!size) {
            const char *value="0000-00-00..0000-00-00";
            while (*value) text[size++]=*value++;
        }
    } else if ((field>=FS_TICKET_SETTING_MONEY && field<FS_TICKET_FIELD_FREE_ENABLE) || field==FS_TICKET_FIELD_NONE_SHOP) {
        uint32_t market=field==FS_TICKET_FIELD_NONE_SHOP?FS_SHOP_UNKNOWN:field-FS_TICKET_SETTING_MONEY;
        const uint32_t *price,*recharge,*balance;
        if (market==FS_SHOP_UNKNOWN) {
            price=config->ticketSettings.noneShop; recharge=price+1; balance=price+2;
        } else {
            price=config->ticketSettings.price+market; recharge=config->ticketSettings.recharge+market; balance=config->shop.balance+market;
        }
        size=formatNumber(text,*price,0);
        text[size++]=','; size+=formatNumber(text+size,*recharge,0);
        text[size++]=','; size+=formatNumber(text+size,*balance,0);
    } else if (field==FS_TICKET_FIELD_CURRENT_CURRENCY) {
        const char *value=config->ticketSettings.currentMarket==FS_SHOP_UNKNOWN?"$NONE":
            config->ticketSettings.currentMarket<FS_SHOP_MARKET_COUNT?FsTicket_WalletKey(config->ticketSettings.currentMarket)+5:0;
        if (!value) return FS_INVALID;
        while (*value) text[size++]=*value++;
    } else if (field==FS_TICKET_FIELD_ANNUAL_COUNT) size=formatNumber(text,config->ticket.annualCount,0);
    else {
        uint32_t enabled=field==FS_TICKET_FIELD_PURCHASED?config->ticket.purchased:
            field==FS_TICKET_FIELD_TRIAL_CLAIMED?config->ticket.trialCount:
            field==FS_TICKET_FIELD_FREE_ENABLE?config->ticketSettings.freeEnable:
            field==FS_TICKET_FIELD_TRIAL_ENABLE?config->ticketSettings.trialEnable:config->ticketSettings.autocurrency;
        const char *value=enabled?"true":"false";
        while (*value) text[size++]=*value++;
    }
    text[size]=0; return FS_OK;
}
void FsConfiguration_CompleteTicket(FsConfiguration *config)
{
    config->ticketSettings.ready=1; config->ticket.freeMode=FS_TICKET_FREE_NONE;
    config->ticket.starts=FS_TICKET_OFFICIAL_START; config->ticket.expires=FS_TICKET_OFFICIAL_END;
    config->ticket.paidStarts=config->ticket.paidExpires=0;
    for (uint32_t i=0;i<config->ticketSettings.periodCount;i++) {
        const FsTicketPeriod *period=config->ticketSettings.periods+i;
        if (config->ticket.purchased && period->field>=FS_TICKET_FIELD_PAID) {
            config->ticket.paidStarts=period->starts; config->ticket.paidExpires=period->ends; break;
        }
    }
}
uint32_t FsTicket_PeriodField(const FsTicketSettings *settings,uint32_t base,uint32_t region,uint32_t country)
{
    if (country==((uint32_t)'H'|((uint32_t)'K'<<8)) && (settings->present&(1u<<(base+8)))) return base+8;
    if (region<7 && (settings->present&(1u<<(base+region+1)))) return base+region+1;
    return base;
}
static int activeWindow(const FsTicketSettings *settings,uint32_t field,uint32_t today,uint32_t *start,uint32_t *end)
{
    *start=*end=0;
    for (uint32_t i=0;i<settings->periodCount;i++) {
        const FsTicketPeriod *period=settings->periods+i;
        if (period->field==field && today>=period->starts && today<=period->ends) {
            if (!*start || period->starts<*start) *start=period->starts;
            if (period->ends>*end) *end=period->ends;
        }
    }
    if (!*start) return 0;
    /* Merge intersecting windows without inventing coverage across a gap. */
    /* 合并相交期间，不把中间的空档变成有效期。 */
    for (uint32_t pass=0;pass<settings->periodCount;pass++)
        for (uint32_t i=0;i<settings->periodCount;i++) {
            const FsTicketPeriod *period=settings->periods+i;
            if (period->field!=field || period->starts>*end || period->ends<*start) continue;
            if (period->starts<*start) *start=period->starts;
            if (period->ends>*end) *end=period->ends;
        }
    return 1;
}
static int selectedWindow(const FsTicketSettings *settings,uint32_t base,uint32_t today,
    uint32_t region,uint32_t country,uint32_t *start,uint32_t *end)
{
    uint32_t field=FsTicket_PeriodField(settings,base,region,country);
    return activeWindow(settings,field,today,start,end);
}
int FsTicket_Select(const FsTicketSettings *settings,const uint32_t now[2],
    uint32_t region,uint32_t country,FsTicketPolicy *policy)
{
    uint32_t today=FsDate_FromPacked(now),start,end;
    if (!settings || !policy || !today) return 0;
    /* Pending transactions use their already frozen native-size policy. */
    /* 未决事务使用已冻结的原尺寸使用权策略。 */
    if (!settings->ready) return 1;
    policy->freeMode=FS_TICKET_FREE_NONE;
    policy->starts=FS_TICKET_OFFICIAL_START; policy->expires=FS_TICKET_OFFICIAL_END;
    if (settings->freeEnable && selectedWindow(settings,FS_TICKET_FIELD_FREE,today,region,country,&start,&end)) {
        policy->freeMode=FS_TICKET_FREE_CUSTOM; policy->starts=start; policy->expires=end;
    } else if (settings->trialEnable && selectedWindow(settings,FS_TICKET_FIELD_TRIAL,today,region,country,&start,&end))
        policy->freeMode=FS_TICKET_FREE_TRIAL;
    /* An unpurchased pass cannot be activated by leftover date ranges. */
    /* purchased=false 时完全忽略残留券日期，不据此恢复使用权。 */
    policy->paidStarts=policy->paidExpires=0;
    if (policy->purchased && selectedWindow(settings,FS_TICKET_FIELD_PAID,today,region,country,&start,&end)) {
        policy->paidStarts=start; policy->paidExpires=end;
    } else policy->purchased=0;
    return 1;
}
int FsConfiguration_RecordPurchase(FsConfiguration *config,uint32_t field,const FsTicketPolicy *updated)
{
    if (!config || !updated || field<FS_TICKET_FIELD_PAID || field>=FS_TICKET_FIELD_PURCHASED ||
        !updated->purchased || !FsDate_Valid(updated->paidStarts) || !FsDate_Valid(updated->paidExpires) ||
        updated->paidStarts>updated->paidExpires) return FS_INVALID;
    FsTicketSettings *settings=&config->ticketSettings;
    uint32_t out=0,previous=settings->periodCount;
    uint32_t start=updated->paidStarts,end=updated->paidExpires;
    if (config->ticket.purchased) for (uint32_t pass=0;pass<previous;pass++)
        for (uint32_t i=0;i<previous;i++) {
            const FsTicketPeriod *period=settings->periods+i;
            if (period->field!=field || period->starts>end || period->ends<start) continue;
            if (period->starts<start) start=period->starts;
            if (period->ends>end) end=period->ends;
        }
    for (uint32_t i=0;i<previous;i++) {
        FsTicketPeriod period=settings->periods[i];
        if (period.field!=field || (config->ticket.purchased &&
            (period.starts>end || period.ends<start))) settings->periods[out++]=period;
    }
    if (out==FS_TICKET_PERIOD_LIMIT) return FS_LIMIT;
    settings->periodCount=out;
    settings->periods[out++]=(FsTicketPeriod){start,end,field};
    settings->periodCount=out;
    while (out<previous) settings->periods[out++]=(FsTicketPeriod){0,0,0};
    settings->present|=1u<<field;
    config->ticket=*updated;
    config->ticket.paidStarts=start; config->ticket.paidExpires=end;
    return FS_OK;
}
