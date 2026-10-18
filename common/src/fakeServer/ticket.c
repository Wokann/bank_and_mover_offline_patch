#include "fakeServer/ticket.h"
#include "fakeServer/numbers.h"

static const FsTicketPolicy onlinePolicy={FS_TICKET_FREE_CUSTOM,0,
    FS_TICKET_ONLINE_START,FS_TICKET_ONLINE_END,0,0,0,0};

static void splitDate(uint32_t date,uint32_t *year,uint32_t *month,uint32_t *day)
{
    uint32_t value=FsDivideU32(date,100,day);
    *year=FsDivideU32(value,100,month);
}

static uint32_t monthDays(uint32_t year,uint32_t month)
{
    static const uint8_t days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t rest100,rest400;
    (void)FsDivideU32(year,100,&rest100);
    (void)FsDivideU32(year,400,&rest400);
    return days[month-1]+(month==2 && (year&3)==0 && (rest100!=0 || rest400==0));
}

int FsDate_Valid(uint32_t date)
{
    uint32_t year,month,day;
    splitDate(date,&year,&month,&day);
    return year>=1900 && year<=9999 && month>=1 && month<=12 &&
        day>=1 && day<=monthDays(year,month);
}

int FsDate_Parse(const char *text,uint32_t *date)
{
    uint32_t value=0;
    if (!text || !date) return 0;
    for (uint32_t i=0;i<10;i++) {
        if (!text[i]) return 0;
        if (i==4 || i==7) { if (text[i]!='-') return 0; }
        else {
            if (text[i]<'0' || text[i]>'9') return 0;
            value=value*10+(uint32_t)(text[i]-'0');
        }
    }
    if (text[10] || !FsDate_Valid(value)) return 0;
    *date=value;
    return 1;
}

uint32_t FsDate_FromPacked(const uint32_t packed[2])
{
    if (!packed || packed[1]>156) return 0;
    uint32_t year=(packed[1]<<6)|(packed[0]>>26);
    uint32_t month=(packed[0]>>22)&15,day=(packed[0]>>17)&31;
    uint32_t date=year*10000+month*100+day;
    return FsDate_Valid(date) && ((packed[0]>>12)&31)<24 &&
        ((packed[0]>>6)&63)<60 && (packed[0]&63)<60?date:0;
}

int FsDate_ParsePeriod(const char *text,uint32_t length,uint32_t *start,uint32_t *end)
{
    if (!text || !start || !end) return 0;
    while (length && (*text==' ' || *text=='\t')) { text++; length--; }
    while (length && (text[length-1]==' ' || text[length-1]=='\t')) length--;
    if (length!=22 || text[10]!='.' || text[11]!='.') return 0;
    char date[11]; date[10]=0;
    for (uint32_t i=0;i<10;i++) date[i]=text[i];
    if (!FsDate_Parse(date,start)) return 0;
    for (uint32_t i=0;i<10;i++) date[i]=text[i+12];
    return FsDate_Parse(date,end) && *start<=*end;
}

static uint32_t ordinal(uint32_t date)
{
    uint32_t year,month,days;
    splitDate(date,&year,&month,&days);
    uint32_t previous=year-1;
    days+=previous*365+(previous>>2)-FsDivideU32(previous,100,0)+FsDivideU32(previous,400,0);
    for (uint32_t i=1;i<month;i++) days+=monthDays(year,i);
    return days;
}

int FsTicket_Allowed(const FsTicketResponse *ticket,uint32_t minimumDays)
{
    if (!ticket || !ticket->valid) return 0;
    uint32_t now=FsDate_FromPacked(ticket->currentDate);
    uint32_t end=FsDate_FromPacked(ticket->expiryDate);
    if (!now || !end || end<now) return 0;
    uint32_t days=ordinal(end)-ordinal(now);
    if ((ticket->expiryDate[0]&0x1ffff)<(ticket->currentDate[0]&0x1ffff) && days) days--;
    return days>=minimumDays;
}

int FsTicket_QueryOperation(uint32_t operation)
{
    return operation==1 || operation==5 || operation==6 || operation==10;
}

int FsTicket_Reply(const FsTicketResponse *ticket,uint32_t operation,void *body,
    uint32_t capacity,uint32_t *size,uint32_t textEncoding)
{
    if (!ticket || !body || !size) return FS_INVALID;
    uint32_t words[2]={0,0},length=4;
    *size=0;
    if (textEncoding>FS_TICKET_TEXT_UTF8) return FS_INVALID;
    if (!FsTicket_QueryOperation(operation)) return FS_UNSUPPORTED;
    if (operation==1 || operation==5) {
        uint8_t *bytes=body;
        length=operation==1?(textEncoding==FS_TICKET_TEXT_UTF8?90:124):50;
        if (capacity<length) return FS_LIMIT;
        for (uint32_t i=0;i<length;i++) bytes[i]=0;
        bytes[0]=FS_TICKET_PRODUCT_COUNT;
        uint32_t offset=4;
        for (uint32_t product=0;product<FS_TICKET_PRODUCT_COUNT;product++) {
            if (operation==1) { bytes[offset]=(uint8_t)product; offset+=4; }
            bytes[offset]=16; offset+=2;
            /* Local product identifier, not an official encrypted receipt. */
            /* 自制商品标识，不是官方加密票据。末字节对应原次数槽 1/2。 */
            bytes[offset+15]=(uint8_t)(product+1); offset+=16;
            if (operation==5) {
                offset++; /* History boolean / 历史布尔字段。 */
                uint32_t count=(uint32_t)ticket->purchaseCounts[product];
                for (uint32_t i=0;i<4;i++) bytes[offset++]=(uint8_t)(count>>(i*8));
            } else {
                offset+=2; /* Empty display-name string / 空显示名字段。 */
                /* Native text lengths include the terminator. UTF-8 decoding
                   otherwise drops the last product-code character. */
                /* 原文本长度包含结束符；否则 UTF-8 解码会截掉商品码末字符。 */
                bytes[offset]=17; offset+=2;
                static const char code[]="FAKE_BANK_PASS_1";
                for (uint32_t i=0;i<sizeof(code);i++) {
                    bytes[offset++]=(uint8_t)(i==15?'1'+product:(uint8_t)code[i]);
                    if (textEncoding==FS_TICKET_TEXT_UTF16) offset++;
                }
            }
        }
        *size=offset;
        return offset==length?FS_OK:FS_INVALID;
    }
    switch (operation) {
    case 6: /* Entitlement expiry / 使用权截止时间。 */
        if (ticket->valid) {
            words[0]=ticket->expiryDate[0]; words[1]=ticket->expiryDate[1];
        }
        length=8;
        break;
    case 10: /* Server current date / 服务当前时间。 */
        words[0]=ticket->currentDate[0]; words[1]=ticket->currentDate[1];
        length=8;
        break;
    default:
        return FS_UNSUPPORTED;
    }
    if (capacity<length) return FS_LIMIT;
    for (uint32_t i=0;i<length;i++) ((uint8_t *)body)[i]=((const uint8_t *)words)[i];
    *size=length;
    return FS_OK;
}

static void packDate(uint32_t date,uint32_t time,uint32_t packed[2])
{
    uint32_t year,month,day;
    splitDate(date,&year,&month,&day);
    packed[0]=((year&63)<<26)|(month<<22)|(day<<17)|(time&0x1ffff);
    packed[1]=year>>6;
}

static uint64_t appendDecimal(uint64_t value,uint32_t field)
{
    uint32_t low=(uint32_t)value,high=(uint32_t)(value>>32);
    uint32_t first=low<<6,second=low<<5,third=low<<2;
    high=(high<<6)+(low>>26)+(high<<5)+(low>>27)+(high<<2)+(low>>30);
    uint32_t sum=first+second;
    high+=sum<first;
    uint32_t result=sum+third;
    high+=result<sum;
    sum=result+field;
    high+=sum<result;
    return ((uint64_t)high<<32)|sum;
}

static uint64_t decimalDate(uint32_t date,uint32_t time)
{
    uint64_t value=appendDecimal(date,(time>>12)&31);
    value=appendDecimal(value,(time>>6)&63);
    return appendDecimal(value,time&63);
}

static void freeWindow(const FsTicketPolicy *policy,uint32_t *start,uint32_t *end)
{
    *start=policy->freeMode==FS_TICKET_FREE_CUSTOM?policy->starts:FS_TICKET_OFFICIAL_START;
    *end=policy->freeMode==FS_TICKET_FREE_CUSTOM?policy->expires:FS_TICKET_OFFICIAL_END;
}

int FsTicket_Build(const FsTicketPolicy *policy,const uint32_t now[2],FsTicketResponse *out)
{
    uint32_t today=FsDate_FromPacked(now);
    if (!policy || !out || !today || policy->freeMode>FS_TICKET_FREE_CUSTOM ||
        policy->purchased>1 || !FsDate_Valid(policy->starts) ||
        !FsDate_Valid(policy->expires) || policy->starts>policy->expires ||
        policy->trialCount>0x7fffffff || policy->annualCount>0x7fffffff ||
        (policy->purchased &&
         (!FsDate_Valid(policy->paidStarts) || !FsDate_Valid(policy->paidExpires) ||
          policy->paidStarts>policy->paidExpires))) return 0;
    uint32_t endTime=(23u<<12)|(59u<<6)|59u;
    out->currentDate[0]=now[0]; out->currentDate[1]=now[1];
    uint32_t start,end;
    freeWindow(policy,&start,&end);
    uint32_t freeValid=(policy->freeMode==FS_TICKET_FREE_OFFICIAL ||
        policy->freeMode==FS_TICKET_FREE_CUSTOM) && today>=start && end>=today;
    uint32_t paidValid=policy->purchased &&
        today>=policy->paidStarts && policy->paidExpires>=today;
    uint32_t expiry=paidValid?policy->paidExpires:end;
    if (freeValid && expiry<end) expiry=end;
    packDate(expiry,endTime,out->expiryDate);
    for (uint32_t i=0;i<5;i++) out->purchaseCounts[i]=-1;
    out->purchaseCounts[0]=(int32_t)policy->trialCount;
    out->purchaseCounts[1]=(int32_t)policy->annualCount;
    out->valid=freeValid || paidValid;
    out->freeActivity=freeValid;
    out->campaignStart=out->freeActivity?decimalDate(start,0):0;
    out->campaignEnd=out->freeActivity?decimalDate(end,endTime):0;
    return 1;
}

int FsTicket_OnlineFallback(const uint32_t now[2],FsTicketResponse *out)
{
    if (!FsTicket_Build(&onlinePolicy,now,out)) return 0;
    for (uint32_t i=0;i<5;i++) out->purchaseCounts[i]=-1;
    return 1;
}

int FsTicket_OnlineReply(const FsTicketResponse *ticket,uint32_t operation,void *body,
    uint32_t capacity,uint32_t *size,uint32_t textEncoding)
{
    if (operation!=1 && operation!=5)
        return FsTicket_Reply(ticket,operation,body,capacity,size,textEncoding);
    if (!size) return FS_INVALID;
    *size=0;
    if (!ticket || !body || textEncoding>FS_TICKET_TEXT_UTF8) return FS_INVALID;
    if (capacity<4) return FS_LIMIT;
    /* Native empty vectors, not invented product IDs or purchase counts. */
    /* 原生空列表，不夹带自制商品标识或购买次数。 */
    for (uint32_t i=0;i<4;i++) ((uint8_t *)body)[i]=0;
    *size=4;
    return FS_OK;
}

void FsTicket_Shop(const FsTicketPolicy *policy,FsTicketShop *out)
{
    out->available[FS_TICKET_TRIAL]=policy->freeMode==FS_TICKET_FREE_TRIAL && !policy->trialCount;
    out->available[FS_TICKET_ANNUAL]=policy->annualCount<0x7fffffff;
    static const char code[]="FAKE_BANK_PASS_1";
    for (uint32_t product=0;product<FS_TICKET_PRODUCT_COUNT;product++) {
        for (uint32_t i=0;i<sizeof(code);i++)
            out->itemCode[product][i]=(char)(i==15?'1'+product:(uint8_t)code[i]);
    }
    out->market=FS_SHOP_UNKNOWN; out->price[0]=0; out->price[1]=0; out->balance=0;
}

typedef struct ShopProfile { const char *key,*currency; uint32_t price,decimals; } ShopProfile;
/* Historical base fees. This local shop does not model regional sales taxes. */
/* 历史基础年费；本地商店不模拟各地附加税费。 */
static const ShopProfile shopProfiles[FS_SHOP_MARKET_COUNT]={
    {"shop.$JPY","JPY",500,0}, {"shop.$USD","USD",499,2},
    {"shop.$GBP","GBP",449,2}, {"shop.$EUR","EUR",499,2},
    {"shop.$KRW","KRW",5000,0}, {"shop.$TWD","TWD",150,0},
    {"shop.$HKD","HKD",4000,2}, {"shop.$AUD","AUD",650,2},
    {"shop.$NZD","NZD",850,2}, {"shop.$CAD","CAD",699,2},
    {"shop.$CHF","CHF",700,2}, {"shop.$ZAR","ZAR",6000,2}
};

const char *FsTicket_WalletKey(uint32_t market)
{
    return market<FS_SHOP_MARKET_COUNT?shopProfiles[market].key:market==FS_SHOP_UNKNOWN?"shop.$NONE":0;
}

uint32_t FsTicket_AnnualPrice(uint32_t market)
{
    return market<FS_SHOP_MARKET_COUNT?shopProfiles[market].price:
        market==FS_SHOP_UNKNOWN?FS_SHOP_NONE_DEFAULT_PRICE:0;
}

#define SHOP_COUNTRY(a,b) ((uint32_t)(a)|((uint32_t)(b)<<8))
static int marketRegion(uint32_t market,uint32_t region)
{
    if (region==0xffffffffu) return 1;
    if (market==FS_SHOP_JP) return region==0;
    if (market==FS_SHOP_US || market==FS_SHOP_CA) return region==1;
    if (market==FS_SHOP_KR) return region==5;
    if (market==FS_SHOP_TW || market==FS_SHOP_HK) return region==6;
    return region==2 || region==3;
}
uint32_t FsTicket_Market(uint32_t region,uint32_t country)
{
    static const uint16_t countries[]={SHOP_COUNTRY('J','P'),SHOP_COUNTRY('U','S'),
        SHOP_COUNTRY('G','B'),0,SHOP_COUNTRY('K','R'),SHOP_COUNTRY('T','W'),
        SHOP_COUNTRY('H','K'),SHOP_COUNTRY('A','U'),SHOP_COUNTRY('N','Z'),
        SHOP_COUNTRY('C','A'),SHOP_COUNTRY('C','H'),SHOP_COUNTRY('Z','A')};
    for (uint32_t i=0;i<FS_SHOP_MARKET_COUNT;i++) if (country && country==countries[i])
        return marketRegion(i,region)?i:FS_SHOP_UNKNOWN;
    /* Country, not UI language or merely the European hardware region. */
    /* 按国家区分，不能用界面语言或整个欧版大区代替。 */
    static const uint16_t euro[]={SHOP_COUNTRY('A','T'),SHOP_COUNTRY('B','E'),
        SHOP_COUNTRY('C','Y'),SHOP_COUNTRY('D','E'),SHOP_COUNTRY('E','E'),
        SHOP_COUNTRY('E','S'),SHOP_COUNTRY('F','I'),SHOP_COUNTRY('F','R'),
        SHOP_COUNTRY('G','R'),SHOP_COUNTRY('I','E'),SHOP_COUNTRY('I','T'),
        SHOP_COUNTRY('L','T'),SHOP_COUNTRY('L','U'),SHOP_COUNTRY('L','V'),
        SHOP_COUNTRY('M','T'),SHOP_COUNTRY('N','L'),SHOP_COUNTRY('P','T'),
        SHOP_COUNTRY('S','I'),SHOP_COUNTRY('S','K')};
    for (uint32_t i=0;i<sizeof(euro)/sizeof(*euro);i++) if (country==euro[i])
        return marketRegion(FS_SHOP_EU,region)?FS_SHOP_EU:FS_SHOP_UNKNOWN;
    if (!country) {
        if (region==0) return FS_SHOP_JP;
        if (region==3) return FS_SHOP_AU;
        if (region==5) return FS_SHOP_KR;
    }
    return FS_SHOP_UNKNOWN;
}
#undef SHOP_COUNTRY

void FsTicket_Amount(FsTicketAmount *out,uint32_t market,uint32_t units)
{
    for (uint32_t i=0;i<sizeof(*out);i++) ((uint8_t *)out)[i]=0;
    if ((market>=FS_SHOP_MARKET_COUNT && market!=FS_SHOP_UNKNOWN) || units>FS_SHOP_MONEY_LIMIT) return;
    char *text=out->text;
    const ShopProfile *profile=market<FS_SHOP_MARKET_COUNT?shopProfiles+market:0;
    uint32_t cents=0;
    if (profile && profile->decimals) units=FsDivideU32(units,100,&cents);
    uint32_t size=0,count=0; char reverse[10];
    if (profile) {
        for (uint32_t i=0;i<3;i++) text[size++]=profile->currency[i];
        text[size++]=' ';
    }
    do { uint32_t digit; units=FsDivideU32(units,10,&digit); reverse[count++]=(char)('0'+digit); } while (units);
    while (count) text[size++]=reverse[--count];
    if (profile && profile->decimals) {
        uint32_t digit; text[size++]='.';
        text[size++]=(char)('0'+FsDivideU32(cents,10,&digit)); text[size++]=(char)('0'+digit);
    }
    text[size]=0;
}

int FsTicket_FormatAmount(const FsTicketAmount *amount,char *text,uint32_t capacity)
{
    if (!amount || !text) return FS_INVALID;
    uint32_t size=0;
    while (size<sizeof(amount->text) && amount->text[size]) {
        if ((uint8_t)amount->text[size]<32 || (uint8_t)amount->text[size]>126) return FS_INVALID;
        size++;
    }
    if (!size || size==sizeof(amount->text)) return FS_INVALID;
    if (size>=capacity) return FS_LIMIT;
    for (uint32_t i=0;i<=size;i++) text[i]=amount->text[i];
    return FS_OK;
}

static uint32_t afterDays(uint32_t date,uint32_t amount)
{
    uint32_t year,month,day;
    splitDate(date,&year,&month,&day);
    while (amount) {
        uint32_t left=monthDays(year,month)-day;
        if (amount<=left) { day+=amount; break; }
        amount-=left+1; day=1;
        if (++month==13) { month=1; if (++year>9999) return 0; }
    }
    return year*10000+month*100+day;
}

int FsTicket_Purchase(const FsTicketPolicy *policy,const uint32_t now[2],
    uint32_t product,FsTicketPolicy *out)
{
    FsTicketResponse ticket;
    if (!out || !FsTicket_Build(policy,now,&ticket) ||
        product>=FS_TICKET_PRODUCT_COUNT) return FS_INVALID;
    if (product==FS_TICKET_TRIAL && policy->freeMode!=FS_TICKET_FREE_TRIAL) return FS_UNSUPPORTED;
    if ((product==FS_TICKET_TRIAL && policy->trialCount) ||
        (product==FS_TICKET_ANNUAL && policy->annualCount==0x7fffffff))
        return FS_ALREADY_CLAIMED;
    uint32_t today=FsDate_FromPacked(now);
    uint32_t active=policy->purchased && policy->paidStarts<=today && today<=policy->paidExpires;
    uint32_t expiry=afterDays(active?policy->paidExpires:today,
        product==FS_TICKET_TRIAL?FS_TICKET_TRIAL_DAYS:FS_TICKET_ANNUAL_DAYS);
    if (!expiry) return FS_LIMIT;
    *out=*policy;
    out->paidStarts=active?policy->paidStarts:today;
    out->paidExpires=expiry; out->purchased=1;
    if (product==FS_TICKET_TRIAL) out->trialCount++;
    else out->annualCount++;
    return FS_OK;
}

static uint16_t campaignCrc(const FsCampaignRecord *body)
{
    const uint8_t *bytes=(const uint8_t *)body;
    uint16_t crc=0xffff;
    for (uint32_t i=0;i<18;i++) {
        crc^=(uint16_t)(bytes[i]<<8);
        for (uint32_t bit=0;bit<8;bit++)
            crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}

static void encodeCalendar(FsCampaignDate *out,const uint32_t packed[2])
{
    uint32_t value=packed[0],year=(value>>26)|(packed[1]<<6);
    out->year[0]=(uint8_t)year; out->year[1]=(uint8_t)(year>>8);
    out->month=(uint8_t)((value>>22)&15);
    out->day=(uint8_t)((value>>17)&31);
    out->hour=(uint8_t)((value>>12)&31);
    out->minute=(uint8_t)((value>>6)&63);
    out->second=(uint8_t)(value&63);
}

void FsCampaign_Encode(const FsTicketPolicy *policy,const FsTicketResponse *ticket,
    FsCampaignRecord *out)
{
    (void)ticket;
    uint32_t start[2],end[2],startDate,endDate;
    freeWindow(policy,&startDate,&endDate);
    packDate(startDate,0,start);
    packDate(endDate,(23u<<12)|(59u<<6)|59u,end);
    out->magic[0]='F'; out->magic[1]='R'; out->magic[2]='E'; out->magic[3]='E';
    encodeCalendar(&out->start,start);
    encodeCalendar(&out->end,end);
    uint16_t crc=campaignCrc(out);
    out->checksum[0]=(uint8_t)crc; out->checksum[1]=(uint8_t)(crc>>8);
}

void FsCampaign_OnlineFallback(FsCampaignRecord *out)
{
    FsCampaign_Encode(&onlinePolicy,0,out);
}
