#include "fakeServer/configuration.h"
#include "fakeServer/numbers.h"
#include "fakeServer/transaction.h"
#include "fakeServer/mover_expression.h"
#include "fakeServer/account_configuration.h"
#include "fs_system/protected_file.h"
#include "fakeServer/conf_text.h"

static const char configurationSeal[]="# fakeServer-complete=";
static const char currencyNoteZh[]="# shop.$<币种>=<年券价格>,<单次充值额>,<余额>；三项均为非负整数。";
static const char currencyNoteEn[]="# shop.$<currency>=<annual_price>,<recharge>,<balance>; all three are non-negative integers.";

typedef struct ConfigParser {
    const FsStorageIo *storage;
    const char *backup;
    FsConfiguration *configuration;
    FsGift *gift;
    FsMoverRule *rule;
    uint32_t section,seen,allowedSection,source;
    uint32_t ticketSeen[2],legacySeen[2],legacyMode,allowRepair,needsRepair,ticketHeader;
    uint32_t inputSize,lastTicketOrder;
    int fromBackup;
    uint32_t globalSeen,switchSeen,disabled,shortCircuit,wordTag,customSeen;
    FsMoverWordIndex wordIndex;
    uint32_t wordIndexReady;
    char *conditionText;
    uint32_t conditionUsed,conditionCapacity,conditionOpen;
    uint8_t inputHash[FS_TRANSACTION_HASH_SIZE];
} ConfigParser;

enum { CONFIG_GLOBAL=0xfffffffeu, CONFIG_CUSTOM=0xfffffffdu };

const char *FsConfiguration_Path(uint32_t section,int temporary)
{
    static const char *const paths[]={"server.conf","mover/nds/checkPokemon.conf","gifts/gift.conf",
        "mover/nds/checkName.conf","mover/vc/checkPokemon.conf","mover/vc/checkName.conf"};
    if (section<FS_CONFIG_TICKET || section>FS_CONFIG_WORDS_VC) return 0;
    if(!temporary)return paths[section-1];
    return section==FS_CONFIG_TICKET?"server.tmp":section==FS_CONFIG_MOVER_NDS?
        "mover/nds/checkPokemon.tmp":section==FS_CONFIG_MOVER_VC?"mover/vc/checkPokemon.tmp":0;
}

static uint32_t configKind(uint32_t target)
{
    return target==FS_CONFIG_MOVER_VC?FS_CONFIG_MOVER:
        target==FS_CONFIG_WORDS_VC?FS_CONFIG_WORDS:target;
}

static uint32_t configSource(uint32_t target)
{
    return target==FS_CONFIG_MOVER_VC || target==FS_CONFIG_WORDS_VC?FS_MOVER_VC:FS_MOVER_GEN5;
}

static int equal(const char *a,const char *b)
{
    while (*a && *a==*b) { a++; b++; }
    return *a==*b;
}

static char *trim(char *value)
{
    while (*value==' ' || *value=='\t') value++;
    char *end=value;
    while (*end) end++;
    while (end>value && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r')) end--;
    *end=0;
    return value;
}

#define number FsParseU32

static int copyText(char *target,uint32_t capacity,const char *value)
{
    uint32_t length=0;
    while (value[length]) {
        if (length+1>=capacity) return 0;
        target[length]=value[length]; length++;
    }
    target[length]=0;
    return 1;
}

static int relativePath(const char *value)
{
    const char *part=value;
    if (!*value || *value=='/') return 0;
    for (;;) {
        uint8_t byte=(uint8_t)*value;
        if (!byte || byte=='/') {
            uint32_t size=(uint32_t)(value-part);
            if (!size || (size==1 && part[0]=='.') ||
                (size==2 && part[0]=='.' && part[1]=='.')) return 0;
            if (!byte) return 1;
            part=value+1;
        } else if (byte<32 || byte>126 || byte==':' || byte=='\\') return 0;
        value++;
    }
}

static void releaseRules(const FsStorageIo *io,FsMoverPolicy *policy,uint32_t first)
{
    if (io && io->release) for (uint32_t i=first;i<policy->ruleCount;i++)
        if (policy->rules[i].condition) {
            io->release(policy->rules[i].condition);policy->rules[i].condition=0;
        }
    policy->ruleCount=first;
}
void FsConfiguration_Release(const FsStorageIo *io,FsConfiguration *config)
{
    if (!config) return;
    FsMover_ReleaseWords(&config->mover);
    releaseRules(io,&config->mover,0);
    if (config->mover.rules && io && io->release) io->release(config->mover.rules);
    config->mover.rules=0;config->mover.ruleCapacity=0;
}

static int reserveRule(ConfigParser *parser)
{
    FsMoverPolicy *policy=&parser->configuration->mover;
    if (policy->ruleCount<policy->ruleCapacity) return FS_OK;
    const FsStorageIo *io=parser->storage;
    if (!io || !io->allocate || !io->release) return FS_NO_MEMORY;
    uint32_t capacity=policy->ruleCapacity;
    uint32_t extra=capacity?(capacity>>1)+1u:8u;
    if (capacity>UINT32_MAX-extra || capacity+extra>UINT32_MAX/sizeof(FsMoverRule)) return FS_NO_MEMORY;
    capacity+=extra;
    FsMoverRule *rules=io->allocate(capacity*sizeof(*rules));
    if (!rules) return FS_NO_MEMORY;
    for (uint32_t i=0;i<policy->ruleCount;i++) rules[i]=policy->rules[i];
    if (policy->rules) io->release(policy->rules);
    policy->rules=rules; policy->ruleCapacity=capacity;
    return FS_OK;
}

static int selectSection(ConfigParser *parser,char *name)
{
    parser->gift=0;
    parser->rule=0;
    if(parser->allowedSection==FS_CONFIG_TICKET && equal(name,"custom")) {
        parser->section=CONFIG_CUSTOM;return FS_OK;
    }
    if (parser->allowedSection==FS_CONFIG_TICKET && equal(name,"ticket")) {
        if (parser->ticketHeader) {
            if (!parser->allowRepair) return FS_INVALID;
            parser->needsRepair=1;
        }
        parser->ticketHeader=1; parser->section=FS_CONFIG_TICKET; return FS_OK;
    }
    if (parser->allowedSection!=FS_CONFIG_TICKET) {
        if (equal(name,"global")) {
            if (parser->globalSeen || parser->section) return FS_INVALID;
            parser->globalSeen=1; parser->section=CONFIG_GLOBAL; return FS_OK;
        }
        if (!parser->switchSeen) return FS_INVALID;
    }
    if (parser->allowedSection==FS_CONFIG_WORDS) {
        if (!FsMover_WordSection(name,&parser->wordTag)) return FS_INVALID;
        parser->section=FS_CONFIG_WORDS; return FS_OK;
    }
    if (parser->allowedSection==FS_CONFIG_MOVER) {
        static const char rulePrefix[]="rule.";
        for (uint32_t i=0;i<sizeof(rulePrefix)-1;i++)
            if (!name[i] || name[i]!=rulePrefix[i]) return FS_INVALID;
        name+=sizeof(rulePrefix)-1;
        if (!*name) return FS_INVALID;
        for (const char *p=name;*p;p++)
            if (!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_')) return FS_INVALID;
        FsMoverPolicy *policy=&parser->configuration->mover;
        for (uint32_t i=0;i<policy->ruleCount;i++)
            if (policy->rules[i].sources==parser->source && equal(name,policy->rules[i].name)) return FS_INVALID;
        int reserved=reserveRule(parser);
        if (reserved!=FS_OK) return reserved;
        FsMoverRule *rule=policy->rules+policy->ruleCount;
        for (uint32_t i=0;i<sizeof(*rule);i++) ((uint8_t *)rule)[i]=0;
        if (!copyText(rule->name,sizeof(rule->name),name)) return FS_LIMIT;
        policy->ruleCount++;
        rule->sources=(uint8_t)parser->source;
        parser->rule=rule; parser->section=FS_CONFIG_MOVER; parser->seen=0;
        return FS_OK;
    }
    if (parser->allowedSection!=FS_CONFIG_GIFT) return FS_INVALID;
    static const char prefix[]="gift.";
    for (uint32_t i=0;i<sizeof(prefix)-1;i++)
        if (!name[i] || name[i]!=prefix[i]) return FS_INVALID;
    name+=sizeof(prefix)-1;
    if (!*name) return FS_INVALID;
    for (const char *p=name;*p;p++)
        if (!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')) return FS_INVALID;
    FsConfiguration *config=parser->configuration;
    if (config->giftCount>=FS_GIFT_LIMIT) return FS_LIMIT;
    for (uint32_t i=0;i<config->giftCount;i++)
        if (equal(name,config->gifts[i].name)) return FS_INVALID;
    FsGift *gift=&config->gifts[config->giftCount];
    if (!copyText(gift->name,sizeof(gift->name),name)) return FS_LIMIT;
    config->giftCount++;
    parser->gift=gift; parser->section=FS_CONFIG_GIFT; parser->seen=0;
    return FS_OK;
}

/* A semicolon closes a field, not its rule section. Accumulate only the
   expression being read; rule/word files are never loaded as a second image. */
/* 分号结束字段而非规则段；只积累当前表达式，不复制整份配置镜像。 */
static int conditionLine(ConfigParser *parser,char *value)
{
    if (*trim(value)=='[') return FS_INVALID;
    char quote=0,*end=value;
    for (;*end;end++) {
        if (quote) {
            if (*end=='\\' && end[1]) end++;
            else if (*end==quote) quote=0;
        } else if (*end=='"') quote=*end;
        else if (*end==';') break;
    }
    if (quote || (*end && *trim(end+1))) return FS_INVALID;
    uint32_t amount=(uint32_t)(end-value),used=parser->conditionUsed;
    if (used>UINT32_MAX-2 || amount>UINT32_MAX-used-2) return FS_NO_MEMORY;
    FsMemory memory={parser->storage->allocate,parser->storage->release};
    int result=FsMemory_Reserve(&memory,(void **)&parser->conditionText,&parser->conditionCapacity,
        used+amount+2,used,1);
    if (result!=FS_OK) return result;
    for (uint32_t i=0;i<amount;i++) parser->conditionText[used++]=value[i];
    parser->conditionText[used++]=' ';parser->conditionText[used]=0;parser->conditionUsed=used;
    if (!*end) return FS_OK;
    result=FsMoverExpression_Parse(parser->storage,parser->conditionText,&parser->rule->condition);
    parser->storage->release(parser->conditionText);parser->conditionText=0;
    parser->conditionUsed=0;parser->conditionCapacity=0;parser->conditionOpen=0;
    return result;
}
static int ruleEntry(ConfigParser *parser,char *key,char *value)
{
    FsMoverRule *rule=parser->rule;uint32_t bit=0,parsed=0;
    if (equal(key,"enabled")) bit=1;
    else if (equal(key,"action")) bit=2;
    else if (equal(key,"condition")) bit=4;
    if (!bit || (rule->defined&bit)) return FS_INVALID;
    if (bit==1) {
        if (!number(value,1,&parsed)) return FS_INVALID;
        rule->enabled=(uint8_t)parsed;
    } else if (bit==2) {
        if (!equal(value,"reject") && !equal(value,"require")) return FS_INVALID;
        rule->action=equal(value,"reject")?FS_MOVER_REJECT:FS_MOVER_REQUIRE;
    } else {
        rule->defined|=(uint8_t)bit;parser->conditionOpen=1;
        return conditionLine(parser,value);
    }
    rule->defined|=(uint8_t)bit;
    return FS_OK;
}

static int gameList(char *value,uint32_t *games)
{
    static const char *const names[]={"X","Y","OR","AS","S","M","US","UM","XY","ORAS","SM","USUM"};
    static const uint8_t masks[]={1,2,4,8,16,32,64,128,3,12,48,192};
    uint32_t result=0;
    if (!*value) return 0;
    do {
        char *end=value;
        while (*end && *end!=',') end++;
        int more=*end!=0;
        if (more) *end=0;
        char *part=trim(value);
        uint32_t mask=0;
        for (uint32_t i=0;i<sizeof(masks);i++) if (equal(part,names[i])) { mask=masks[i]; break; }
        if (!mask || (result&mask)) return 0;
        result|=mask;
        if (!more) break;
        value=end+1;
    } while (1);
    *games=result;
    return 1;
}

static int giftFiles(char *value,FsGift *gift)
{
    char list[FS_GIFT_PATH_SIZE]; uint32_t used=0,count=0;
    do {
        char *end=value;
        while (*end && *end!=',') end++;
        int more=*end!=0;
        if (more) *end=0;
        char *name=trim(value);
        if (++count>6 || !relativePath(name)) return 0;
        for (const char *p=name;*p;p++)
            if (*p=='/' || *p=='*' || *p=='?' || *p=='"' || *p=='<' || *p=='>' || *p=='|') return 0;
        if (used) list[used++]=',';
        for (;*name;name++) {
            if (used+1>=sizeof(list)) return 0;
            list[used++]=*name;
        }
        if (!more) break;
        value=end+1;
    } while (1);
    list[used]=0;
    return copyText(gift->file,sizeof(gift->file),list);
}

static int giftDateEntry(ConfigParser *parser,char *key,char *value)
{
    uint32_t field=0,scope=FS_GIFT_GLOBAL,selector=0;
    char *label=key;
    while (*label && *label!='.') label++;
    int qualified=*label!=0;
    if (qualified) *label++=0;
    if (qualified && !*label) return FS_INVALID;
    if (equal(key,"start")) field=FS_GIFT_WINDOW_START;
    else if (equal(key,"end")) field=FS_GIFT_WINDOW_END;
    else if (equal(key,"periods")) field=FS_GIFT_WINDOW_RANGE;
    else if (equal(key,"exclude")) field=FS_GIFT_WINDOW_RANGE|FS_GIFT_WINDOW_EXCLUDE;
    else return FS_INVALID;
    if (*label) {
        static const char *const regions[]={"JPN","USA","EUR","AUS","CHN","KOR","TWN"};
        scope=FS_GIFT_REGION; selector=7;
        for (uint32_t i=0;i<7;i++) if (equal(label,regions[i])) selector=i;
        if (selector==7) {
            if (label[0]<'A' || label[0]>'Z' || !label[1] ||
                label[1]<'A' || label[1]>'Z' || label[2]) return FS_INVALID;
            scope=FS_GIFT_COUNTRY;
            selector=(uint8_t)label[0]|((uint32_t)(uint8_t)label[1]<<8);
        }
    } else if (field!=FS_GIFT_WINDOW_RANGE &&
        field!=(FS_GIFT_WINDOW_RANGE|FS_GIFT_WINDOW_EXCLUDE)) return FS_INVALID;
    FsConfiguration *config=parser->configuration;
    uint32_t event=(uint32_t)(parser->gift-config->gifts);
    FsGiftWindow *existing=0;
    for (uint32_t i=0;i<config->giftWindowCount;i++) {
        FsGiftWindow *window=config->giftWindows+i;
        if (window->event==event && (window->flags>>6)==scope && window->selector==selector &&
            (window->flags&FS_GIFT_WINDOW_EXCLUDE)==(field&FS_GIFT_WINDOW_EXCLUDE)) {
            if ((window->flags&FS_GIFT_WINDOW_RANGE) || (field&FS_GIFT_WINDOW_RANGE) ||
                (window->flags&field)) return FS_INVALID;
            existing=window;
        }
    }
    do {
        if (!existing && config->giftWindowCount==FS_GIFT_WINDOW_LIMIT) return FS_LIMIT;
        FsGiftWindow *window=existing?existing:config->giftWindows+config->giftWindowCount++;
        window->event=(uint8_t)event; window->selector=(uint16_t)selector;
        window->flags|=(uint8_t)((scope<<6)|field);
        if (field&FS_GIFT_WINDOW_RANGE) {
            if (equal(value,"disable")) {
                window->flags|=FS_GIFT_WINDOW_DISABLED;
                window->starts=window->ends=0; break;
            }
            char *end=value;
            while (*end && *end!=',') end++;
            int more=*end!=0;
            if (more) *end=0;
            char *range=trim(value); uint32_t length=0;
            while (range[length]) length++;
            if (!FsDate_ParsePeriod(range,length,&window->starts,&window->ends))
                return FS_INVALID;
            if (!more) break;
            value=end+1;
        } else {
            if (!FsDate_Parse(value,field==FS_GIFT_WINDOW_START?&window->starts:&window->ends))
                return FS_INVALID;
            break;
        }
    } while (1);
    return FS_OK;
}


static int ticketSeen(const ConfigParser *parser,uint32_t field)
{ return (parser->ticketSeen[field>>5]>>(field&31))&1u; }
static void ticketMark(ConfigParser *parser,uint32_t field)
{ parser->ticketSeen[field>>5]|=1u<<(field&31); }

static int legacySeen(const ConfigParser *parser,uint32_t field)
{ return (parser->legacySeen[field>>5]>>(field&31))&1u; }

static int ticketEntry(ConfigParser *parser,const char *key,const char *value)
{
    int field=FsConfiguration_TicketField(key);
    if (field>=0) {
        int duplicate=ticketSeen(parser,(uint32_t)field);
        if (duplicate) {
            if (!parser->allowRepair) return FS_INVALID;
            parser->needsRepair=1;
        }
        int result=FsConfiguration_ParseTicket(parser->configuration,(uint32_t)field,value);
        if ((result==FS_INVALID || result==FS_LIMIT) && (parser->allowRepair ||
            (field==FS_TICKET_FIELD_PURCHASED && (equal(value,"0") || equal(value,"1"))))) {
            parser->needsRepair=1;
            if (duplicate) return FS_OK;
            result=FsConfiguration_RepairTicket(parser->configuration,(uint32_t)field,value);
        }
        if (result!=FS_OK) return result;
        if (parser->allowRepair && (field==FS_TICKET_FIELD_FREE || field==FS_TICKET_FIELD_TRIAL) &&
            (equal(value,"enable") || equal(value,"disable") || equal(value,"none"))) {
            uint32_t enable=field==FS_TICKET_FIELD_FREE?FS_TICKET_FIELD_FREE_ENABLE:FS_TICKET_FIELD_TRIAL_ENABLE;
            if (!ticketSeen(parser,enable)) {
                (void)FsConfiguration_ParseTicket(parser->configuration,enable,equal(value,"enable")?"true":"false");
                ticketMark(parser,enable);
            }
        }
        if (parser->allowRepair) {
            uint32_t position=0;
            while (position<FS_TICKET_SETTING_FIELDS && FsConfiguration_TicketOrder(position)!=(uint32_t)field) position++;
            if (position+1<parser->lastTicketOrder) parser->needsRepair=1;
            parser->lastTicketOrder=position+1;
        }
        ticketMark(parser,(uint32_t)field);
        return FS_OK;
    }
    int legacy=FsConfiguration_TicketLegacyField(key);
    if (legacy<0) return FS_INVALID;
    int duplicate=legacySeen(parser,(uint32_t)legacy);
    if (duplicate && !parser->allowRepair) return FS_INVALID;
    FsTicketPolicy *ticket=&parser->configuration->ticket;
    uint32_t parsed=0,maximum=legacy==0 || legacy==3?3:FS_SHOP_MONEY_LIMIT;
    int dated=legacy==1 || legacy==2 || legacy==4 || legacy==5;
    int valid=dated?((legacy>=4 && equal(value,"0000-00-00")) || FsDate_Parse(value,&parsed)):
        number(value,maximum,&parsed);
    if (!valid && !parser->allowRepair) return FS_INVALID;
    if (!valid && duplicate) return FS_OK;
    if (!valid) {
        if (!legacy) parsed=FS_TICKET_FREE_OFFICIAL;
        else if (legacy==1) parsed=FS_TICKET_OFFICIAL_START;
        else if (legacy==2) parsed=FS_TICKET_OFFICIAL_END;
        else if (legacy==3) parsed=3;
        else if (legacy>=7) {
            uint32_t kind,market=FsDivideU32((uint32_t)legacy-7,3,&kind);
            if (kind<2) parsed=FsTicket_AnnualPrice(market);
        }
    }
    if (legacy==0) ticket->freeMode=parsed;
    else if (legacy==1) ticket->starts=parsed;
    else if (legacy==2) ticket->expires=parsed;
    else if (legacy==3) parser->legacyMode=parsed;
    else if (legacy==4) ticket->paidStarts=parsed;
    else if (legacy==5) ticket->paidExpires=parsed;
    else if (legacy==6) {
        if (!ticketSeen(parser,FS_TICKET_FIELD_TRIAL_CLAIMED)) ticket->trialCount=parsed!=0;
    } else {
        uint32_t kind,market=FsDivideU32((uint32_t)legacy-7,3,&kind);
        if (!ticketSeen(parser,FS_TICKET_SETTING_MONEY+market)) {
            uint32_t *target=kind==0?&parser->configuration->ticketSettings.price[market]:
                kind==1?&parser->configuration->ticketSettings.recharge[market]:&parser->configuration->shop.balance[market];
            *target=parsed;
        }
    }
    parser->legacySeen[(uint32_t)legacy>>5]|=1u<<((uint32_t)legacy&31); parser->needsRepair=1;
    return FS_OK;
}

static int entry(ConfigParser *parser,char *key,char *value)
{
    uint32_t bit=0,*seen=0,valid=0;
    if (parser->section==CONFIG_GLOBAL) {
        const char *name=parser->allowedSection==FS_CONFIG_GIFT?"enableGift":
            parser->allowedSection==FS_CONFIG_MOVER?"enableCheckPokemon":"enableCheckName";
        if (!equal(key,name) || parser->switchSeen ||
            (!equal(value,"true") && !equal(value,"false"))) return FS_INVALID;
        parser->switchSeen=1; parser->disabled=equal(value,"false");
        return FS_OK;
    }
    if(parser->section==CONFIG_CUSTOM) {
        if(!FsCustom_Parse(&parser->configuration->mover.customOptions,key,value))return FS_INVALID;
        for(uint32_t i=0;i<FS_CUSTOM_COUNT;i++)if(equal(key,FsCustom_Key(i)))parser->customSeen|=1u<<i;
        return FS_OK;
    }
    if (parser->section==FS_CONFIG_TICKET) return ticketEntry(parser,key,value);
    else if (parser->section==FS_CONFIG_MOVER) {
        if (parser->rule) return ruleEntry(parser,key,value);
    } else if (parser->section==FS_CONFIG_GIFT && parser->gift) {
        FsGift *gift=parser->gift;
        seen=&parser->seen;
        if (equal(key,"enabled")) { bit=1; valid=number(value,1,&gift->enabled); }
        else if (equal(key,"generation")) {
            bit=2; valid=number(value,7,&gift->generation) &&
                (gift->generation==6 || gift->generation==7);
        } else if (equal(key,"games")) { bit=4; valid=gameList(value,&gift->games); }
        else if (equal(key,"start")) { bit=8; valid=FsDate_Parse(value,&gift->starts); }
        else if (equal(key,"end")) { bit=16; valid=FsDate_Parse(value,&gift->ends); }
        else if (equal(key,"file")) {
            if (parser->seen&128) return FS_INVALID;
            bit=32; valid=relativePath(value) && copyText(gift->file,sizeof(gift->file),value);
        }
        else if (equal(key,"folder")) {
            bit=256; valid=relativePath(value) && copyText(gift->folder,sizeof(gift->folder),value);
        }
        else if (equal(key,"files")) {
            if (parser->seen&32) return FS_INVALID;
            bit=128; valid=giftFiles(value,gift);
            gift->fileList=1;
        }
        else if (equal(key,"id")) { bit=64; valid=number(value,65535,&gift->nativeId) && gift->nativeId; }
        else return giftDateEntry(parser,key,value);
    }
    if (!seen || !bit || (*seen&bit) || !valid) return FS_INVALID;
    *seen|=bit;
    return FS_OK;
}

static int line(ConfigParser *parser,char *text)
{
    int word=parser->allowedSection==FS_CONFIG_WORDS && parser->section==FS_CONFIG_WORDS;
    if (word) {
        /* A word expression is data, not an indented key/value. Strip only the
           CR of CRLF; significant leading/trailing spaces must survive edits
           and reloads, including an escaped final space. */
        /* 词条正文不是可缩进的键值；只去除 CRLF 的 CR，保留首尾有效空格，
           包括转义的末尾空格；编辑和重载不得改变表达式含义。 */
        char *end=text,*control=text;
        while (*end) end++;
        if (end>text && end[-1]=='\r') end[-1]=0;
        while (*control==' ' || *control=='\t') control++;
        if (*control && *control!='#' && *control!=';' && !FsMover_WordHeader(control)) {
            if (!parser->wordIndexReady) {
                int result=FsMover_IndexWords(&parser->configuration->mover,&parser->wordIndex);
                if (result!=FS_OK) return result;
            }
            parser->wordIndexReady=1;
            return FsMover_AddIndexedNameRule(&parser->configuration->mover,&parser->wordIndex,
                text,parser->source,parser->wordTag);
        }
    }
    text=trim(text);
    if (!*text) return FS_OK;
    if (*text=='#' || *text==';') {
        /* Human documentation is external; only tuple labels and the seal remain. */
        /* 人工说明外置；规范化配置只保留货币三元组注释和完整性标记。 */
        if (parser->allowRepair && parser->allowedSection==FS_CONFIG_TICKET &&
            !equal(text,currencyNoteZh) && !equal(text,currencyNoteEn)) {
            const char *prefix=configurationSeal,*cursor=text;
            while (*prefix && *cursor==*prefix) { cursor++; prefix++; }
            if (*prefix) parser->needsRepair=1;
            else {
                for (uint32_t i=0;i<2*FS_TRANSACTION_HASH_SIZE;i++) {
                    if (!((*cursor>='0' && *cursor<='9') || (*cursor>='a' && *cursor<='f'))) {
                        parser->needsRepair=1; break;
                    }
                    cursor++;
                }
                if (*cursor) parser->needsRepair=1;
            }
        }
        return FS_OK;
    }
    if (*text=='[' && (!word || FsMover_WordHeader(text))) {
        char *end=text+1;
        while (*end && *end!=']') end++;
        /* Unknown sections are quarantined, not parsed as ticket settings. */
        /* 未知段中的内容不按票务字段解释。 */
        if (parser->allowedSection==FS_CONFIG_TICKET) parser->section=0xffffffffu;
        if (!*end || *trim(end+1)) {
            if (parser->allowedSection==FS_CONFIG_WORDS) parser->section=0xffffffffu;
            return FS_INVALID;
        }
        *end=0;
        int selected=selectSection(parser,trim(text+1));
        if (selected!=FS_OK && parser->allowedSection==FS_CONFIG_WORDS) parser->section=0xffffffffu;
        return selected;
    }
    char *separator=text;
    while (*separator && *separator!='=') separator++;
    int separated=*separator!=0;
    *separator=0;
    char *key=trim(text),*value=separated?trim(separator+1):"";
    if(separated && parser->allowRepair && parser->allowedSection==FS_CONFIG_TICKET) {
        char *end=value;while(*end)end++;
        if(end==value || end[-1]!=';')parser->needsRepair=1;
    }
    if(separated && parser->allowedSection!=FS_CONFIG_MOVER)FsConf_Value(value);
    int field=FsConfiguration_TicketField(key),legacy=FsConfiguration_TicketLegacyField(key);
    if (parser->allowRepair && parser->allowedSection==FS_CONFIG_TICKET &&
        !parser->section && (field>=0 || legacy>=0)) {
        parser->section=FS_CONFIG_TICKET; parser->ticketHeader=1; parser->needsRepair=1;
    }
    if (!separated && !(parser->allowRepair && parser->section==FS_CONFIG_TICKET &&
        (field>=0 || legacy>=0))) return FS_INVALID;
    return entry(parser,key,value);
}

/* # comments end at the physical newline; quoted OT text is data. */
/* # 注释仅到本物理行末；OT 引号内的 # 不当作注释。 */
static int moverLine(ConfigParser *parser,char *text)
{
    char quote=0;
    for (char *p=text;*p;p++) {
        if (quote) {
            if (*p=='\\' && p[1]) p++;
            else if (*p==quote) quote=0;
        } else if (*p=='"') quote=*p;
        else if (*p=='#') { *p=0;break; }
    }
    text=trim(text);
    if (!*text) return FS_OK;
    if (parser->conditionOpen) return conditionLine(parser,text);
    if (*text=='[') return line(parser,text);
    char *separator=text;
    while (*separator && *separator!='=') separator++;
    if (!*separator) return FS_INVALID;
    char *keyEnd=separator;
    while (keyEnd>text && (keyEnd[-1]==' ' || keyEnd[-1]=='\t')) keyEnd--;
    static const char key[]="condition";
    int condition=(uint32_t)(keyEnd-text)==sizeof(key)-1;
    for (uint32_t i=0;condition && i<sizeof(key)-1;i++) if (text[i]!=key[i]) condition=0;
    if (!condition) {
        char *end=separator+1;
        while (*end && *end!=';') end++;
        if (!*end || *trim(end+1)) return FS_INVALID;
        *end=0;
    }
    return line(parser,text);
}

static int configurationLine(ConfigParser *parser,char *text,int damaged)
{
    if (parser->allowedSection==FS_CONFIG_WORDS) {
        if (damaged) {
            char *control=text;
            while (*control==' ' || *control=='\t') control++;
            if (FsMover_WordHeader(control)) parser->section=0xffffffffu;
        }
        int result=damaged?damaged:line(parser,text);
        if (result==FS_INVALID || result==FS_UNSUPPORTED || result==FS_LIMIT) {
            uint32_t source=parser->source==FS_MOVER_VC?1:0;
            uint32_t *skipped=parser->configuration->mover.wordSkipped+source;
            if (*skipped<UINT32_MAX) (*skipped)++;
            return FS_OK;
        }
        return result;
    }
    if (parser->allowedSection==FS_CONFIG_MOVER)
        return damaged?damaged:moverLine(parser,text);
    int repair=parser->allowRepair && parser->allowedSection==FS_CONFIG_TICKET;
    if (damaged) {
        if (!repair) return damaged;
        parser->needsRepair=1;
        /* Keep only a recognizable key, never a truncated/binary value. */
        /* 只恢复可识别的键，不采用截断或含二进制字节的值。 */
        char *separator=text;
        while (*separator && *separator!='=') separator++;
        if (*separator) separator[1]=0;
        else if (*trim(text)!='[') return FS_OK;
    }
    int result=line(parser,text);
    if (repair && (result==FS_INVALID || result==FS_LIMIT)) {
        parser->needsRepair=1; return FS_OK;
    }
    return result;
}

static int readConfigurationBody(const FsStorageIo *io,ConfigParser *parser,
    const char *path,int required,FsConfigError *error)
{
    uint32_t handle=0,size=0;
    error->path=path;
    int status=parser->backup?FsFile_OpenPublished(io,path,parser->backup,&handle,&size,&parser->fromBackup):
        io->openRead(path,&handle,&size);
    if (!status) return required?FS_MISSING:FS_OK;
    if (status<0) return FS_IO_ERROR;
    parser->inputSize=size;
    FsHashState digest; FsHash_Initialize(&digest);
    int words=parser->allowedSection==FS_CONFIG_WORDS;
    uint32_t limit=parser->allowedSection==FS_CONFIG_MOVER || words?UINT32_MAX:FS_CONFIG_FILE_LIMIT;
    int oversized=size>limit;
    int discard=oversized && parser->allowRepair && parser->allowedSection==FS_CONFIG_TICKET;
    int result=oversized && !discard && !parser->shortCircuit?FS_LIMIT:FS_OK;
    if (discard) parser->needsRepair=1;
    uint8_t chunk[4096]; char initial[FS_CONFIG_LINE_SIZE],*text=initial;
    uint32_t textCapacity=sizeof(initial),used=0,lineNumber=1;
    FsMemory memory={io->allocate,io->release};
    int damaged=0,stopped=0;
    for (uint32_t offset=0;result==FS_OK && !stopped && offset<size;) {
        if (oversized && !discard && offset>=limit) { result=FS_LIMIT; break; }
        uint32_t amount=size-offset;
        if (oversized && !discard && amount>limit-offset) amount=limit-offset;
        uint32_t capacity=parser->shortCircuit && !parser->switchSeen?64:
            parser->allowedSection==FS_CONFIG_WORDS || parser->allowedSection==FS_CONFIG_MOVER?sizeof(chunk):256;
        if (amount>capacity) amount=capacity;
        if (!io->read(handle,offset,chunk,amount)) { result=FS_IO_ERROR; break; }
        FsHash_Update(&digest,chunk,amount);
        offset+=amount;
        if (discard) continue;
        for (uint32_t i=0;result==FS_OK && i<amount;i++) {
            if (chunk[i]=='\n') {
                text[used]=0;
                char *content=text;
                if (lineNumber==1 && used>=3 && (uint8_t)text[0]==0xef &&
                    (uint8_t)text[1]==0xbb && (uint8_t)text[2]==0xbf) content+=3;
                uint32_t skipped=words?parser->configuration->mover.wordSkipped[parser->source==FS_MOVER_VC]:0;
                result=configurationLine(parser,content,damaged);
                if (words && !parser->configuration->mover.wordSkippedLine[parser->source==FS_MOVER_VC] &&
                    skipped!=parser->configuration->mover.wordSkipped[parser->source==FS_MOVER_VC])
                    parser->configuration->mover.wordSkippedLine[parser->source==FS_MOVER_VC]=lineNumber;
                used=0; damaged=0;
                if (result==FS_OK && lineNumber<UINT32_MAX) lineNumber++;
                if (result==FS_OK && parser->shortCircuit && parser->disabled) { stopped=1; break; }
                if (result==FS_OK && oversized && !discard && parser->switchSeen)
                    result=FS_LIMIT;
            } else if (!chunk[i]) damaged=FS_INVALID;
            else if (damaged) continue;
            else {
                if (used>=UINT32_MAX-1u) { result=FS_NO_MEMORY;break; }
                if (used+1u>=textCapacity) {
                    if (!words && parser->allowedSection!=FS_CONFIG_MOVER) { damaged=FS_LIMIT;continue; }
                    result=FsMemory_Text(&memory,&text,&textCapacity,initial,used,used+2u);
                    if (result!=FS_OK) break;
                }
                text[used++]=(char)chunk[i];
            }
        }
    }
    if (result==FS_OK && !stopped && (used || damaged)) {
        text[used]=0;
        char *content=text;
        if (lineNumber==1 && used>=3 && (uint8_t)text[0]==0xef &&
            (uint8_t)text[1]==0xbb && (uint8_t)text[2]==0xbf) content+=3;
        uint32_t skipped=words?parser->configuration->mover.wordSkipped[parser->source==FS_MOVER_VC]:0;
        result=configurationLine(parser,content,damaged);
        if (words && !parser->configuration->mover.wordSkippedLine[parser->source==FS_MOVER_VC] &&
            skipped!=parser->configuration->mover.wordSkipped[parser->source==FS_MOVER_VC])
            parser->configuration->mover.wordSkippedLine[parser->source==FS_MOVER_VC]=lineNumber;
    }
    if (result==FS_OK && parser->conditionOpen) result=FS_INVALID;
    if (parser->conditionText) memory.release(parser->conditionText);
    parser->conditionText=0;parser->conditionUsed=0;parser->conditionCapacity=0;parser->conditionOpen=0;
    if (text!=initial) memory.release(text);
    if (result==FS_OK && parser->allowedSection!=FS_CONFIG_TICKET &&
        (!parser->globalSeen || !parser->switchSeen)) result=FS_INVALID;
    if (!io->close(handle)) result=FS_IO_ERROR;
    if (result==FS_OK) FsHash_Finish(&digest,parser->inputHash);
    error->line=result==FS_OK?0:lineNumber;
    return result;
}

/* All word buffers/index/AST frames grow from actual content. Loader scratch
   is released on every exit; matching reuses its own on-demand workspace. */
/* 词池、索引及语法帧按实际内容增长；加载临时数据在所有退出路径释放。 */
__attribute__((noinline))
static int readWordConfiguration(const FsStorageIo *io,ConfigParser *parser,
    const char *path,int required,FsConfigError *error)
{
    FsMoverPolicy *policy=&parser->configuration->mover;
    policy->wordWork.memory=(FsMemory){io->allocate,io->release};
    parser->wordIndex=(FsMoverWordIndex){0}; parser->wordIndexReady=0;
    int result=readConfigurationBody(io,parser,path,required,error);
    FsMover_ReleaseWordIndex(policy,&parser->wordIndex);
    FsWordPattern_Release(&policy->wordWork);
    parser->wordIndexReady=0;
    return result;
}
static int readConfiguration(const FsStorageIo *io,ConfigParser *parser,
    const char *path,int required,FsConfigError *error)
{
    return parser->allowedSection==FS_CONFIG_WORDS?
        readWordConfiguration(io,parser,path,required,error):
        readConfigurationBody(io,parser,path,required,error);
}

static int paidComplete(const FsConfiguration *config)
{
    if (!config->ticket.purchased) return 1;
    for (uint32_t i=0;i<config->ticketSettings.periodCount;i++)
        if (config->ticketSettings.periods[i].field>=FS_TICKET_FIELD_PAID) return 1;
    return 0;
}
static int ticketComplete(const ConfigParser *parser)
{
    for (uint32_t i=0;i<FS_TICKET_SETTING_FIELDS;i++)
        if (FsConfiguration_TicketRequired(i) && !ticketSeen(parser,i)) return 0;
    return paidComplete(parser->configuration);
}
static int completeTicket(ConfigParser *parser)
{
    if(parser->customSeen!=((1u<<FS_CUSTOM_COUNT)-1u))parser->needsRepair=1;
    FsConfiguration *config=parser->configuration;
    if (legacySeen(parser,0)) {
        uint32_t mode=config->ticket.freeMode;
        if (!ticketSeen(parser,FS_TICKET_FIELD_FREE_ENABLE)) {
            config->ticketSettings.freeEnable=mode==FS_TICKET_FREE_OFFICIAL || mode==FS_TICKET_FREE_CUSTOM;
            ticketMark(parser,FS_TICKET_FIELD_FREE_ENABLE);
        }
        if (!ticketSeen(parser,FS_TICKET_FIELD_TRIAL_ENABLE)) {
            config->ticketSettings.trialEnable=mode==FS_TICKET_FREE_TRIAL;
            ticketMark(parser,FS_TICKET_FIELD_TRIAL_ENABLE);
        }
        if (!ticketSeen(parser,FS_TICKET_FIELD_FREE) && mode==FS_TICKET_FREE_CUSTOM &&
            legacySeen(parser,1) && legacySeen(parser,2) && FsDate_Valid(config->ticket.starts) &&
            FsDate_Valid(config->ticket.expires) && config->ticket.starts<=config->ticket.expires) {
            (void)FsConfiguration_ParseTicket(config,FS_TICKET_FIELD_FREE,"0000-00-00..0000-00-00");
            config->ticketSettings.periods[config->ticketSettings.periodCount++]=
                (FsTicketPeriod){config->ticket.starts,config->ticket.expires,FS_TICKET_FIELD_FREE};
            ticketMark(parser,FS_TICKET_FIELD_FREE);
        }
    }
    if (legacySeen(parser,3)) for (uint32_t kind=0;kind<2;kind++) {
        uint32_t field=kind?FS_TICKET_FIELD_TRIAL_ENABLE:FS_TICKET_FIELD_FREE_ENABLE;
        if (!ticketSeen(parser,field)) {
            (void)FsConfiguration_ParseTicket(config,field,parser->legacyMode&(1u<<kind)?"true":"false");
            ticketMark(parser,field);
        }
    }
    if (!ticketSeen(parser,FS_TICKET_FIELD_PAID) && legacySeen(parser,4) && legacySeen(parser,5) &&
        FsDate_Valid(config->ticket.paidStarts) && FsDate_Valid(config->ticket.paidExpires) &&
        config->ticket.paidStarts<=config->ticket.paidExpires &&
        config->ticketSettings.periodCount<FS_TICKET_PERIOD_LIMIT) {
        config->ticketSettings.periods[config->ticketSettings.periodCount++]=
            (FsTicketPeriod){config->ticket.paidStarts,config->ticket.paidExpires,FS_TICKET_FIELD_PAID};
        config->ticketSettings.present|=1u<<FS_TICKET_FIELD_PAID;
        ticketMark(parser,FS_TICKET_FIELD_PAID);
    }
    if (!ticketSeen(parser,FS_TICKET_FIELD_TRIAL_CLAIMED) && legacySeen(parser,6))
        ticketMark(parser,FS_TICKET_FIELD_TRIAL_CLAIMED);
    for (uint32_t market=0;market<FS_SHOP_MARKET_COUNT;market++)
        if (!ticketSeen(parser,FS_TICKET_SETTING_MONEY+market)) {
            uint32_t legacy=7+market*3;
            int provided=0;
            for (uint32_t kind=0;kind<3;kind++) {
                uint32_t *value=kind==0?&config->ticketSettings.price[market]:
                    kind==1?&config->ticketSettings.recharge[market]:&config->shop.balance[market];
                if (legacySeen(parser,legacy+kind)) provided=1;
                else *value=kind<2?FsTicket_AnnualPrice(market):0;
            }
            if (provided) ticketMark(parser,FS_TICKET_SETTING_MONEY+market);
        }
    for (uint32_t i=0;i<FS_TICKET_SETTING_FIELDS;i++) if (FsConfiguration_TicketRequired(i) && !ticketSeen(parser,i)) {
        parser->needsRepair=1;
        int result=FsConfiguration_ParseTicket(config,i,FsConfiguration_TicketValue(i));
        if (result!=FS_OK) return result;
        ticketMark(parser,i);
    }
    if (!paidComplete(config)) {
        /* Incomplete entitlement dates never create an invented paid pass. */
        /* 不完整的券期限清零为未购券，不猜测日期；余额和合法历史计数保留。 */
        config->ticket.purchased=0;
        parser->needsRepair=1;
    }
    FsConfiguration_CompleteTicket(config);
    return FS_OK;
}

static int validatePolicy(FsConfiguration *config,uint32_t section)
{
    if (section==FS_CONFIG_WORDS && !FsMover_ValidateWords(&config->mover)) return FS_INVALID;
    if (section==FS_CONFIG_MOVER) for (uint32_t i=0;i<config->mover.ruleCount;i++) {
        const FsMoverRule *rule=config->mover.rules+i;
        if (rule->defined!=7 || !rule->condition ||
            (rule->action!=FS_MOVER_REJECT && rule->action!=FS_MOVER_REQUIRE)) return FS_INVALID;
    }
    if (section==FS_CONFIG_GIFT) for (uint32_t i=0;i<config->giftCount;i++) {
        const FsGift *gift=config->gifts+i;
        int dated=FsDate_Valid(gift->starts) && FsDate_Valid(gift->ends) && gift->starts<=gift->ends;
        if ((gift->starts && !FsDate_Valid(gift->starts)) || (gift->ends && !FsDate_Valid(gift->ends)) ||
            (gift->starts && gift->ends && gift->starts>gift->ends)) return FS_INVALID;
        for (uint32_t j=0;j<config->giftWindowCount;j++) {
            FsGiftWindow *window=config->giftWindows+j;
            if (window->event!=i) continue;
            if (window->flags&FS_GIFT_WINDOW_DISABLED) {
                if (!(window->flags&FS_GIFT_WINDOW_EXCLUDE)) dated=1;
                continue;
            }
            if (!window->starts) window->starts=gift->starts;
            if (!window->ends) window->ends=gift->ends;
            if (!FsDate_Valid(window->starts) || !FsDate_Valid(window->ends) ||
                window->starts>window->ends) return FS_INVALID;
            if (!(window->flags&FS_GIFT_WINDOW_EXCLUDE)) dated=1;
        }
        if (gift->enabled && (!gift->file[0] || !gift->games || !dated ||
            (gift->generation!=6 && gift->generation!=7) ||
            (gift->games&~(gift->generation==6?15u:240u)))) return FS_INVALID;
        if ((gift->fileList && !gift->folder[0]) || (!gift->fileList && gift->folder[0])) return FS_INVALID;
        if (gift->fileList && gift->file[0]) {
            char path[FS_GIFT_FILE_PATH_SIZE],previous[FS_GIFT_FILE_PATH_SIZE]; uint32_t count=1;
            for (const char *p=gift->file;*p;p++) if (*p==',') count++;
            if (count>6) return FS_INVALID;
            for (uint32_t j=0;j<count;j++) {
                if (!FsGift_FilePath(gift,j,path)) return FS_LIMIT;
                for (uint32_t k=0;k<j;k++) {
                    if (!FsGift_FilePath(gift,k,previous)) return FS_LIMIT;
                    uint32_t n=0;
                    while (path[n] && previous[n]) {
                        char a=path[n],b=previous[n];
                        if (a>='A' && a<='Z') a=(char)(a+32);
                        if (b>='A' && b<='Z') b=(char)(b+32);
                        if (a!=b) break;
                        n++;
                    }
                    if (!path[n] && !previous[n]) return FS_INVALID;
                }
            }
        }
        if (gift->nativeId) for (uint32_t j=0;j<i;j++)
            if (config->gifts[j].nativeId==gift->nativeId) return FS_INVALID;
    }
    return FS_OK;
}

static int completeGifts(ConfigParser *parser)
{
    int status=validatePolicy(parser->configuration,FS_CONFIG_GIFT);
    if (status==FS_OK && parser->disabled)
        for (uint32_t i=0;i<parser->configuration->giftCount;i++)
            parser->configuration->gifts[i].enabled=0;
    return status;
}

static void discardExtension(ConfigParser *parser,uint32_t rules)
{
    FsConfiguration *config=parser->configuration;
    if (parser->allowedSection==FS_CONFIG_MOVER) {
        releaseRules(parser->storage,&config->mover,rules);
    } else if (parser->allowedSection==FS_CONFIG_GIFT) {
        config->giftCount=0; config->giftWindowCount=0;
    } else if (parser->allowedSection==FS_CONFIG_WORDS) {
        /* Remove only this file's source bit, including duplicate words merged
           into a previously loaded source. No second policy copy is needed. */
        /* 只撤销本文件来源位，包括合并到另一来源的同词；不复制整份策略到栈。 */
        FsMoverPolicy *policy=&config->mover;
        uint32_t read=0,write=0,count=0;
        for (uint32_t i=0;i<policy->wordCount;i++) {
            FsMoverWord *word=FsMover_WordAt(policy,read);
            uint32_t tag=word->tag,bytes=FsMover_WordBytes(word->length);
            uint32_t sources=(tag&FS_MOVER_ALL)&~parser->source;
            if (sources) {
                uint8_t *pool=policy->words;
                for (uint32_t j=0;j<bytes;j++) pool[write+j]=pool[read+j];
                FsMover_WordAt(policy,write)->tag=(tag&~FS_MOVER_ALL)|sources;
                write+=bytes;count++;
            }
            read+=bytes;
        }
        policy->wordCount=count;policy->wordBytes=write;
        for (;write<read;write++) ((uint8_t *)policy->words)[write]=0;
    }
}

static int loadExtension(const FsStorageIo *io,FsConfiguration *config,uint32_t target,
    FsConfigError *error)
{
    ConfigParser parser={.storage=io,.configuration=config,.allowedSection=configKind(target),
        .source=configSource(target),.shortCircuit=1};
    uint32_t rules=config->mover.ruleCount;
    error->line=0;
    /* Only the explicitly permitted BanUnicodeChinese edit can publish a
       screening file. Its previous published image is a valid fallback.
       Gift and name inputs remain read-only. */
    /* 仅汉化许可可发布指定规则字段；筛选表可读已发布备份，礼物／姓名表仍只读。 */
    if(parser.allowedSection==FS_CONFIG_MOVER)parser.backup=parser.source==FS_MOVER_GEN5?
        "mover/nds/checkPokemon.bak":"mover/vc/checkPokemon.bak";
    int adjusted=parser.allowedSection==FS_CONFIG_MOVER?
        FsCustom_ApplyMover(io,config->mover.customOptions,parser.source):FS_OK;
    if(adjusted!=FS_OK && adjusted!=FS_INVALID){error->path=FsConfiguration_Path(target,0);return error->result=adjusted;}
    int result=readConfiguration(io,&parser,FsConfiguration_Path(target,0),1,error);
    if (result==FS_OK) result=parser.allowedSection==FS_CONFIG_GIFT?
        completeGifts(&parser):validatePolicy(config,parser.allowedSection);
    if (result!=FS_OK || parser.disabled) discardExtension(&parser,rules);
    if (result==FS_INVALID || result==FS_LIMIT || result==FS_MISSING) result=FS_OK;
    error->result=result;
    if (result==FS_OK) { error->line=0; error->path=0; }
    return result;
}

/* Writing and verification share one transformation. Human documentation is external;
   valid values survive and invalid entries are removed rather than reactivated. */
/* 写入与验证使用同一转换；说明外置，保留合法设置，移除无效条目。 */
typedef struct TicketOutput {
    const FsStorageIo *io;
    uint32_t handle,offset,verify;
    FsHashState digest;
    uint32_t used;
    uint8_t buffer[512];
} TicketOutput;

static int sameBytes(const void *a,const void *b,uint32_t size)
{
    for (uint32_t i=0;i<size;i++) if (((const uint8_t *)a)[i]!=((const uint8_t *)b)[i]) return 0;
    return 1;
}
static int ticketFlush(TicketOutput *output)
{
    if (!output->used) return 1;
    uint32_t offset=output->offset-output->used;
    if (output->verify) {
        uint8_t actual[sizeof(output->buffer)];
        if (!output->io->read(output->handle,offset,actual,output->used) ||
            !sameBytes(output->buffer,actual,output->used)) return 0;
    } else if (!output->io->write(output->handle,offset,output->buffer,output->used)) return 0;
    output->used=0;
    return 1;
}

static int ticketEmit(TicketOutput *output,const void *data,uint32_t size)
{
    if (size>32768-output->offset) return 0;
    FsHash_Update(&output->digest,data,size);
    if (!output->handle) { output->offset+=size; return 1; }
    const uint8_t *in=data;
    while (size) {
        uint32_t amount=sizeof(output->buffer)-output->used;
        if (amount>size) amount=size;
        for (uint32_t i=0;i<amount;i++) output->buffer[output->used+i]=in[i];
        output->used+=amount; output->offset+=amount; in+=amount; size-=amount;
        if (output->used==sizeof(output->buffer) && !ticketFlush(output)) return 0;
    }
    return 1;
}
static int ticketText(TicketOutput *output,const char *text)
{
    uint32_t size=0; while (text[size]) size++;
    return ticketEmit(output,text,size);
}
static int renderTicket(TicketOutput *output,const ConfigParser *parser)
{
    FsHash_Initialize(&output->digest);
    if (!ticketText(output,"[ticket]\n")) return 0;
    for (uint32_t position=0;position<FS_TICKET_SETTING_FIELDS;position++) {
        uint32_t i=FsConfiguration_TicketOrder(position);
        if (!FsConfiguration_TicketRequired(i) && !ticketSeen(parser,i)) continue;
        if (i==FS_TICKET_FIELD_TRIAL_ENABLE ||
            i==FS_TICKET_FIELD_AUTOCURRENCY || i==FS_TICKET_FIELD_PURCHASED) {
            if (!ticketText(output,"\n")) return 0;
            if (i==FS_TICKET_FIELD_AUTOCURRENCY &&
                (!ticketText(output,currencyNoteZh) || !ticketText(output,"\n") ||
                 !ticketText(output,currencyNoteEn) || !ticketText(output,"\n"))) return 0;
        }
        char value[FS_CONFIG_LINE_SIZE];
        if (FsConfiguration_FormatTicket(parser->configuration,i,value)!=FS_OK ||
            !ticketText(output,FsConfiguration_TicketKey(i)) || !ticketText(output,"=") ||
            !ticketText(output,value) || !ticketText(output,";\n")) return 0;
    }
    if(!ticketText(output,"\n[custom]\n"))return 0;
    for(uint32_t i=0;i<FS_CUSTOM_COUNT;i++)
        if(!ticketText(output,FsCustom_Key(i)) || !ticketText(output,"=") ||
            !ticketText(output,FsCustom_Value(parser->configuration->mover.customOptions,i)) ||
            !ticketText(output,";\n"))return 0;
    uint8_t hash[FS_TRANSACTION_HASH_SIZE]; char seal[sizeof(configurationSeal)+2*FS_TRANSACTION_HASH_SIZE];
    FsHash_Finish(&output->digest,hash);
    for (uint32_t i=0;i<sizeof(configurationSeal)-1;i++) seal[i]=configurationSeal[i];
    static const char hex[]="0123456789abcdef";
    for (uint32_t i=0;i<FS_TRANSACTION_HASH_SIZE;i++) {
        seal[sizeof(configurationSeal)-1+i*2]=hex[hash[i]>>4];
        seal[sizeof(configurationSeal)+i*2]=hex[hash[i]&15];
    }
    seal[sizeof(seal)-1]='\n';
    return ticketEmit(output,seal,sizeof(seal)) && ticketFlush(output);
}
static int inputMatches(const FsStorageIo *io,const ConfigParser *parser,int existing)
{
    uint32_t handle=0,size=0;
    int fromBackup=0;
    int status=FsFile_OpenPublished(io,"server.conf",parser->backup,&handle,&size,&fromBackup);
    if (status<0) return 0;
    if (!status) return !existing && !parser->fromBackup;
    int ok=(existing || parser->fromBackup) && fromBackup==parser->fromBackup && size==parser->inputSize;
    FsHashState digest; FsHash_Initialize(&digest); uint8_t chunk[256],hash[FS_TRANSACTION_HASH_SIZE];
    for (uint32_t offset=0;ok && offset<size;) {
        uint32_t amount=size-offset; if (amount>sizeof(chunk)) amount=sizeof(chunk);
        ok=io->read(handle,offset,chunk,amount);
        if (ok) FsHash_Update(&digest,chunk,amount);
        offset+=amount;
    }
    if (!io->close(handle)) ok=0;
    if (!ok) return 0;
    FsHash_Finish(&digest,hash);
    return sameBytes(hash,parser->inputHash,FS_TRANSACTION_HASH_SIZE);
}
static int normalizeTicket(const FsStorageIo *io,ConfigParser *parser,int existing,FsConfigError *error)
{
    uint32_t source=0,size=0,staged=0;
    int fromBackup=0;
    int status=FsFile_OpenPublished(io,"server.conf",parser->backup,&source,&size,&fromBackup);
    if (status<0) return FS_IO_ERROR;
    if (status!=((existing || parser->fromBackup)?1:0) || fromBackup!=parser->fromBackup || size!=parser->inputSize) {
        if (status==1 && !io->close(source)) return FS_IO_ERROR;
        return FS_LOCKED;
    }
    TicketOutput output={.io=io};
    int result=renderTicket(&output,parser)?FS_OK:FS_IO_ERROR;
    uint32_t expected=output.offset;
    if (result==FS_OK && (!io->remove("server.tmp") || !io->openWrite("server.tmp",expected,&staged))) result=FS_IO_ERROR;
    if (result==FS_OK) {
        output=(TicketOutput){.io=io,.handle=staged};
        if (!renderTicket(&output,parser)) result=FS_IO_ERROR;
    }
    if (staged && !io->close(staged)) result=FS_IO_ERROR;
    if (result==FS_OK) {
        if (io->openRead("server.tmp",&staged,&size)!=1) result=FS_IO_ERROR;
        else {
            output=(TicketOutput){.io=io,.handle=staged,.verify=1};
            if (size!=expected || !renderTicket(&output,parser)) result=FS_IO_ERROR;
            if (!io->close(staged)) result=FS_IO_ERROR;
        }
    }
    if (status==1 && !io->close(source)) result=FS_IO_ERROR;
    if (result!=FS_OK) return result;
    if (!inputMatches(io,parser,existing)) return FS_LOCKED;
    result=FsConfiguration_ValidateFile(io,parser->configuration,FS_CONFIG_TICKET,"server.tmp",error);
    if (result!=FS_OK) return result;
    if (FsConfiguration_VerifyTemporary(io,"server.tmp")!=FS_OK) return FS_IO_ERROR;
    error->path="server.conf";
    return FsFile_PublishStaged(io,"server.conf","server.tmp","server.bak",existing);
}

/* Purchases and repairs use the same checked temporary-file publisher. */
/* 购买与配置修复均使用校验后的中转文件发布。 */
int FsConfiguration_Purchase(const FsStorageIo *io,FsConfiguration *config,
    const FsTicketPolicy *updated,uint32_t market,uint32_t balance,uint32_t paidField,FsConfigError *error)
{
    if(io && (io->capabilities&FS_STORAGE_ACCOUNT_LAYOUT))
        return FsAccountConfiguration_Purchase(io,config,updated,market,balance,paidField,error);
    if (!io || !config || !error || (market>=FS_SHOP_MARKET_COUNT && market!=FS_SHOP_UNKNOWN) ||
        balance>FS_SHOP_MONEY_LIMIT || (updated && !updated->purchased)) return FS_INVALID;
    FsTicketPolicy previous=config->ticket;
    FsShopPolicy previousShop=config->shop;
    uint8_t before[FS_TRANSACTION_HASH_SIZE],after[FS_TRANSACTION_HASH_SIZE];
    FsTransaction_Hash(&config->ticketSettings,sizeof(config->ticketSettings),before);
    ConfigParser parser={.configuration=config,.allowedSection=FS_CONFIG_TICKET};
    config->ticket=(FsTicketPolicy){0}; config->shop=(FsShopPolicy){0};
    config->mover.customOptions=0;
    for (uint32_t i=0;i<sizeof(config->ticketSettings);i++) ((uint8_t *)&config->ticketSettings)[i]=0;
    int result=readConfiguration(io,&parser,"server.conf",1,error);
    if (result==FS_OK && !ticketComplete(&parser)) result=FS_INVALID;
    if (result==FS_OK) FsConfiguration_CompleteTicket(config);
    FsTransaction_Hash(&config->ticketSettings,sizeof(config->ticketSettings),after);
    if (result==FS_OK && (!sameBytes(&previous,&config->ticket,sizeof(previous)) ||
        !sameBytes(&previousShop,&config->shop,sizeof(previousShop)) || !sameBytes(before,after,FS_TRANSACTION_HASH_SIZE))) result=FS_LOCKED;
    if (result!=FS_OK) return error->result=result;
    static const uint8_t fields[]={FS_TICKET_FIELD_PURCHASED,FS_TICKET_FIELD_TRIAL_CLAIMED,FS_TICKET_FIELD_ANNUAL_COUNT};
    FsConfigEdit edits[5]; char values[4][33],paid[FS_CONFIG_LINE_SIZE];
    if (updated) {
        result=FsConfiguration_RecordPurchase(config,paidField,updated);
        if (result!=FS_OK) return error->result=result;
        if (FsConfiguration_FormatTicket(config,paidField,paid)!=FS_OK) return error->result=FS_LIMIT;
        edits[4]=(FsConfigEdit){FS_CONFIG_SET,"ticket",FsConfiguration_TicketKey(paidField),paid};
    }
    *FsConfiguration_ShopValue(config,market,2)=balance;
    for (uint32_t i=0;i<4;i++) {
        uint32_t field=i<3?fields[i]:market==FS_SHOP_UNKNOWN?FS_TICKET_FIELD_NONE_SHOP:FS_TICKET_SETTING_MONEY+market;
        if (FsConfiguration_FormatTicket(config,field,values[i])!=FS_OK) return FS_INVALID;
        edits[i]=(FsConfigEdit){FS_CONFIG_SET,"ticket",FsConfiguration_TicketKey(field),values[i]};
    }
    return FsConfiguration_Edit(io,config,FS_CONFIG_TICKET,edits+(updated?0:3),updated?5:1,error);
}

static int loadConfiguration(const FsStorageIo *io,FsConfiguration *config,FsConfigError *error,int mover)
{
    if (!io || !config || !error || !io->openRead || !io->read || !io->close ||
        !io->openWrite || !io->write || !io->remove || !io->rename) return FS_INVALID;
    FsConfiguration_Release(io,config);
    for (uint32_t i=0;i<sizeof(*config);i++) ((uint8_t *)config)[i]=0;
    /* Reserve factory schedules before parsing so malformed/excessive entries
       cannot consume the space needed to repair missing required dates. */
    /* 先预填基础日期，避免过量或损坏条目耗尽缺项修复所需的空间。 */
    for (uint32_t i=0;i<FS_TICKET_SETTING_FIELDS;i++) if (FsConfiguration_TicketRequired(i))
        (void)FsConfiguration_ParseTicket(config,i,FsConfiguration_TicketValue(i));
    error->line=0; error->result=FS_OK; error->path=0;
    ConfigParser parser={.storage=io,.backup="server.bak",.configuration=config,.allowedSection=FS_CONFIG_TICKET,.allowRepair=1};
    int result;
    if(io->capabilities&FS_STORAGE_ACCOUNT_LAYOUT) {
        result=FsAccountConfiguration_Load(io,config,error);
    } else {
    result=readConfiguration(io,&parser,"server.conf",1,error);
    if (result==FS_MISSING) {
        /* Explicit pure-tmp policy: no published configuration means defaults,
           not an uncommitted wallet/date change recovered from server.tmp. */
        /* 纯 tmp 策略：无正式配置时重建默认值，不接纳未提交的钱包／日期改动。 */
        if (!(io->capabilities&FS_STORAGE_READ_ONLY) && !io->remove("server.tmp")) result=FS_IO_ERROR;
    }
    int existing=result==FS_OK && !parser.fromBackup;
    if (result==FS_OK || result==FS_MISSING) {
        if (result==FS_MISSING) {
            for (uint32_t i=0;i<sizeof(*config);i++) ((uint8_t *)config)[i]=0;
            parser=(ConfigParser){.storage=io,.backup="server.bak",.configuration=config,.allowedSection=FS_CONFIG_TICKET,.allowRepair=1};
        }
        result=completeTicket(&parser);
        if (result==FS_OK && (parser.needsRepair || parser.fromBackup) &&
            !(io->capabilities&FS_STORAGE_READ_ONLY)) result=normalizeTicket(io,&parser,existing,error);
    }
    }
    error->result=result;
    if (result==FS_OK) { error->line=0; error->path=0; }
    config->ticketError=*error;
    config->moverError=*error; config->giftError=*error;
    if (result!=FS_OK) return result;
    /* Missing or malformed extensions mean no extension, not a failed service.
       I/O failures and pending transaction/history evidence remain errors. */
    /* 缺少或损坏的扩展视为未配置；真实 I/O 失败及在途／历史记录不按缺项清零。 */
    static const uint32_t extensions[]={FS_CONFIG_MOVER_NDS,FS_CONFIG_MOVER_VC,
        FS_CONFIG_WORDS_NDS,FS_CONFIG_WORDS_VC,FS_CONFIG_GIFT};
    for (uint32_t i=0;i<sizeof(extensions)/sizeof(*extensions);i++) {
        if (!mover && extensions[i]!=FS_CONFIG_GIFT) continue;
        result=loadExtension(io,config,extensions[i],error);
        if (extensions[i]==FS_CONFIG_GIFT) config->giftError=*error;
        else if (result!=FS_OK && config->moverError.result==FS_OK) config->moverError=*error;
    }
    if (config->moverError.result!=FS_OK) *error=config->moverError;
    return error->result;
}

int FsConfiguration_Load(const FsStorageIo *io,FsConfiguration *config,FsConfigError *error)
{
    return loadConfiguration(io,config,error,1);
}

int FsConfiguration_LoadService(const FsStorageIo *io,FsConfiguration *config,FsConfigError *error)
{
    return loadConfiguration(io,config,error,0);
}

int FsConfiguration_LoadMover(const FsStorageIo *io,FsConfiguration *config,uint32_t source,
    FsConfigError *error)
{
    if (!io || !config || !error || (source!=FS_MOVER_GEN5 && source!=FS_MOVER_VC)) return FS_INVALID;
    FsConfiguration_Release(io,config);
    uint32_t custom=config->mover.customOptions;
    for (uint32_t i=0;i<sizeof(config->mover);i++) ((uint8_t *)&config->mover)[i]=0;
    config->mover.customOptions=custom;
    error->line=0;error->result=FS_OK;error->path=0;
    config->moverError=*error;
    uint32_t checks=source==FS_MOVER_GEN5?FS_CONFIG_MOVER_NDS:FS_CONFIG_MOVER_VC;
    uint32_t words=source==FS_MOVER_GEN5?FS_CONFIG_WORDS_NDS:FS_CONFIG_WORDS_VC;
    int result=loadExtension(io,config,checks,error);
    config->moverError=*error;
    int loaded=loadExtension(io,config,words,error);
    if (result==FS_OK) { result=loaded;config->moverError=*error; }
    *error=config->moverError;
    return result;
}

int FsConfiguration_ValidateFile(const FsStorageIo *io,FsConfiguration *config,
    uint32_t section,const char *path,FsConfigError *error)
{
    if (!io || !config || !path || !error || !FsConfiguration_Path(section,0)) return FS_INVALID;
    FsConfiguration_Release(io,config);
    for (uint32_t i=0;i<sizeof(*config);i++) ((uint8_t *)config)[i]=0;
    ConfigParser parser;
    for (uint32_t i=0;i<sizeof(parser);i++) ((uint8_t *)&parser)[i]=0;
    parser.storage=io; parser.configuration=config; parser.allowedSection=configKind(section); parser.source=configSource(section);
    error->line=0;
    int result=readConfiguration(io,&parser,path,1,error);
    if (result==FS_OK && section==FS_CONFIG_TICKET) {
        if (!ticketComplete(&parser) || parser.legacySeen[0] || parser.legacySeen[1] || parser.needsRepair) result=FS_INVALID;
        else FsConfiguration_CompleteTicket(config);
    }
    if (result==FS_OK) result=section==FS_CONFIG_GIFT?completeGifts(&parser):validatePolicy(config,parser.allowedSection);
    if (result==FS_OK && parser.disabled) discardExtension(&parser,0);
    error->result=result;
    if (result==FS_OK) { error->line=0; error->path=0; }
    return result;
}
