#include "fakeServer/account_configuration.h"
#include "fs_system/protected_file.h"
#include "fakeServer/numbers.h"
#include "fakeServer/conf_text.h"
#include <string.h>

enum { DOC_SHARED,DOC_USER };
static const char *const paths[]={"server.conf","user.conf"};
static const char *const temps[]={"server.tmp","user.tmp"};

static int document(uint32_t field)
{
    if(field>=FS_TICKET_FIELD_PAID && field<=FS_TICKET_FIELD_ANNUAL_COUNT)return DOC_USER;
    if(field==FS_TICKET_FIELD_AUTOCURRENCY || field==FS_TICKET_FIELD_CURRENT_CURRENCY)return DOC_USER;
    return DOC_SHARED;
}
static char *trim(char *text)
{
    while(*text==' ' || *text=='\t')text++;
    char *end=text+strlen(text);
    while(end>text && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r'))*--end=0;
    return text;
}
static int shopField(uint32_t field)
{ return (field>=FS_TICKET_SETTING_MONEY && field<FS_TICKET_FIELD_FREE_ENABLE) || field==FS_TICKET_FIELD_NONE_SHOP; }
static uint32_t marketOf(uint32_t field)
{ return field==FS_TICKET_FIELD_NONE_SHOP?FS_SHOP_UNKNOWN:field-FS_TICKET_SETTING_MONEY; }
static void defaults(FsConfiguration *config)
{
    config->ticket=(FsTicketPolicy){0};config->shop=(FsShopPolicy){0};
    memset(&config->ticketSettings,0,sizeof(config->ticketSettings));
    config->mover.customOptions=0;
    for(uint32_t i=0;i<FS_TICKET_SETTING_FIELDS;i++)if(FsConfiguration_TicketRequired(i))
        (void)FsConfiguration_ParseTicket(config,i,FsConfiguration_TicketValue(i));
}
static void parseDocument(FsConfiguration *config,uint32_t doc,char *text,uint32_t size)
{
    uint32_t seen[2]={0,0},section=0;char *cursor=text;
    if(size>=3 && (uint8_t)cursor[0]==0xef && (uint8_t)cursor[1]==0xbb && (uint8_t)cursor[2]==0xbf)cursor+=3;
    while(cursor<text+size) {
        char *line=cursor;int damaged=0;
        while(cursor<text+size && *cursor!='\n'){if(!*cursor)damaged=1;cursor++;}
        if(cursor<text+size)*cursor++=0;
        if(damaged)continue;
        char *comment=strchr(line,'#');if(comment)*comment=0;line=trim(line);
        if(*line=='[') {
            /* 只读取旧余额分段以保留已有金额；写回统一合入 [ticket]。 */
            /* Read older balance sections without emitting them on publication. */
            section=!strcmp(line,"[ticket]")?1:doc==DOC_USER && !strcmp(line,"[wallet]")?2:
                doc==DOC_USER && !strcmp(line,"[console]")?3:
                doc==DOC_SHARED && !strcmp(line,"[custom]")?4:0;
            continue;
        }
        if(!section)continue;
        char *value=strchr(line,'=');if(!value)continue;*value++=0;line=trim(line);value=trim(value);
        FsConf_Value(value);
        if(section==4){(void)FsCustom_Parse(&config->mover.customOptions,line,value);continue;}
        if(section==3) {
            if(!strcmp(line,"autoRegion") && (!strcmp(value,"true") || !strcmp(value,"false")))
                config->console.autoRegion=!strcmp(value,"true");
            for(uint32_t i=0;i<FS_CONSOLE_FIELDS;i++)if(!strcmp(line,FsConsole_Key(i)))
                (void)FsConsole_Parse(&config->console,i,value);
            continue;
        }
        int field=!strcmp(line,"autoCurrency")?FS_TICKET_FIELD_AUTOCURRENCY:FsConfiguration_TicketField(line);
        if(field==FS_TICKET_FIELD_AUTOCURRENCY && strcmp(line,"autoCurrency"))continue;
        if(doc==DOC_USER && !strncmp(line,"balance.",8)) {
            if(strlen(line+8)>8)continue;
            char key[24];strcpy(key,"shop.");strcat(key,line+8);field=FsConfiguration_TicketField(key);
            if(field<0 || !shopField((uint32_t)field))continue;
            uint32_t balance;
            if(FsParseU32(value,FS_SHOP_MONEY_LIMIT,&balance))
                *FsConfiguration_ShopValue(config,marketOf((uint32_t)field),2)=balance;
            continue;
        }
        if(section!=1)continue;
        if(field<0 || document((uint32_t)field)!=(int)doc || strlen(value)>=FS_CONFIG_LINE_SIZE)continue;
        int valid;
        if(shopField((uint32_t)field)) {
            /* Repair each component independently; a bad duplicate must not
               erase the last valid price/recharge. Server text has no balance. */
            char *component=value;
            for(uint32_t kind=0;kind<2;kind++) {
                char *comma=strchr(component,',');if(comma)*comma=0;
                uint32_t amount;
                if(FsParseU32(trim(component),FS_SHOP_MONEY_LIMIT,&amount))
                    *FsConfiguration_ShopValue(config,marketOf((uint32_t)field),kind)=amount;
                if(!comma)break;
                component=comma+1;
            }
            valid=1;
        } else valid=FsConfiguration_ParseTicket(config,(uint32_t)field,value)==FS_OK;
        if(valid)seen[(uint32_t)field>>5]|=1u<<((uint32_t)field&31);
        else if(!(seen[(uint32_t)field>>5]&(1u<<((uint32_t)field&31))) && FsConfiguration_TicketRequired((uint32_t)field))
            (void)FsConfiguration_ParseTicket(config,(uint32_t)field,FsConfiguration_TicketValue((uint32_t)field));
    }
}
static char *number(char *out,uint32_t value)
{
    char reverse[10];uint32_t count=0;
    do{uint32_t digit;value=FsDivideU32(value,10,&digit);reverse[count++]=(char)('0'+digit);}while(value);
    while(count)*out++=reverse[--count];
    *out=0;return out;
}
static int render(const FsStorageIo *io,const FsConfiguration *config,uint32_t doc,char **data,uint32_t *size)
{
    /* Fixed policy keys have validated date text; users/word pools are dynamic. */
    uint32_t capacity=FS_TICKET_SETTING_FIELDS*(FS_CONFIG_LINE_SIZE+48u)+1024u;
    char *text=io->allocate(capacity);if(!text)return FS_NO_MEMORY;
    char *out=text;
    if(doc==DOC_USER) {
        strcpy(out,config->console.autoRegion?"[console]\nautoRegion=true;\n":"[console]\nautoRegion=false;\n");out+=strlen(out);
        for(uint32_t i=0;i<FS_CONSOLE_FIELDS;i++) {
            strcpy(out,FsConsole_Key(i));out+=strlen(out);*out++='=';
            const char *value=config->console.values[i]==FS_CONSOLE_UNKNOWN?"none":0;
            if(value){strcpy(out,value);out+=strlen(out);}
            else out=number(out,config->console.values[i]);
            *out++=';';*out++='\n';
        }
        *out++='\n';
    }
    strcpy(out,"[ticket]\n");out+=strlen(out);
    for(uint32_t position=0;position<FS_TICKET_SETTING_FIELDS;position++) {
        uint32_t field=FsConfiguration_TicketOrder(position);
        if(field==FS_TICKET_FIELD_AUTOCURRENCY || field==FS_TICKET_FIELD_CURRENT_CURRENCY)continue;
        if(document(field)!=(int)doc || (!FsConfiguration_TicketRequired(field) &&
            !(config->ticketSettings.present&(1u<<field))))continue;
        char value[FS_CONFIG_LINE_SIZE];int result=FsConfiguration_FormatTicket(config,field,value);
        if(result!=FS_OK){io->release(text);return result;}
        if(shopField(field)) {
            char *last=0;for(char *p=value;*p;p++)if(*p==',')last=p;
            if(last)*last=0;
        }
        if(doc==DOC_SHARED && field==FS_TICKET_FIELD_TRIAL_ENABLE)*out++='\n';
        if(doc==DOC_SHARED && field==FS_TICKET_FIELD_NONE_SHOP) {
            strcpy(out,"\n# shop.$<币种>=<年券价格>,<单次充值额>；两项均为非负整数，余额在个人 user.conf 中。\n"
                "# $NONE 的 807 为自定数值，代表 Bank 支持的最高图鉴编号。\n"
                "# shop.$<currency>=<annual_price>,<recharge>; non-negative integers. Balances are in user.conf.\n"
                "# $NONE uses the custom value 807, the highest Pokedex number supported by Bank.\n");out+=strlen(out);
        }
        strcpy(out,FsConfiguration_TicketKey(field));out+=strlen(out);*out++='=';
        strcpy(out,value);out+=strlen(out);*out++=';';*out++='\n';
    }
    if(doc==DOC_USER) {
        *out++='\n';
        char value[FS_CONFIG_LINE_SIZE];
        const uint32_t fields[]={FS_TICKET_FIELD_AUTOCURRENCY,FS_TICKET_FIELD_CURRENT_CURRENCY};
        for(uint32_t i=0;i<2;i++) {
            int result=FsConfiguration_FormatTicket(config,fields[i],value);
            if(result!=FS_OK){io->release(text);return result;}
            strcpy(out,i?"currentCurrency=":"autoCurrency=");out+=strlen(out);
            strcpy(out,value);out+=strlen(out);*out++=';';*out++='\n';
        }
        for(uint32_t position=0;position<FS_TICKET_SETTING_FIELDS;position++) {
            uint32_t field=FsConfiguration_TicketOrder(position);if(!shopField(field))continue;
            strcpy(out,"balance.");out+=8;strcpy(out,FsConfiguration_TicketKey(field)+5);out+=strlen(out);*out++='=';
            out=number(out,*FsConfiguration_ShopValue((FsConfiguration *)config,marketOf(field),2));*out++=';';*out++='\n';
        }
    } else {
        strcpy(out,"\n[custom]\n");out+=strlen(out);
        for(uint32_t i=0;i<FS_CUSTOM_COUNT;i++) {
            strcpy(out,FsCustom_Key(i));out+=strlen(out);*out++='=';
            strcpy(out,FsCustom_Value(config->mover.customOptions,i));out+=strlen(out);
            *out++=';';*out++='\n';
        }
    }
    *out=0;*data=text;*size=(uint32_t)(out-text);return FS_OK;
}
static int saveDocuments(const FsStorageIo *io,FsConfiguration *config,uint32_t mask)
{
    for(uint32_t doc=0;doc<2;doc++)if(mask&(1u<<doc)) {
        char *text=0;uint32_t size=0;int result=render(io,config,doc,&text,&size);
        if(result==FS_OK)result=FsText_Publish(io,paths[doc],temps[doc],text,size);
        if(text)io->release(text);
        if(result!=FS_OK)return result;
    }
    return FS_OK;
}
static void complete(FsConfiguration *config)
{
    if(config->ticketSettings.autocurrency) {
        uint32_t region,country;
        FsConsole_Location(&config->console,&region,&country);
        config->ticketSettings.currentMarket=FsTicket_Market(region,country);
    }
    FsConfiguration_CompleteTicket(config);
}
int FsAccountConfiguration_Load(const FsStorageIo *io,FsConfiguration *config,FsConfigError *error)
{
    if(!io || !config || !error)return FS_INVALID;
    *error=(FsConfigError){0};
    defaults(config);int result=FS_OK;
    for(uint32_t doc=0;doc<2;doc++) {
        char *text=0;uint32_t size=0;error->path=paths[doc];
        result=FsText_Read(io,paths[doc],&text,&size);
        if(doc==DOC_USER)FsConsole_Default(&config->console);
        if(result==FS_OK){parseDocument(config,doc,text,size);io->release(text);}
        else if(result==FS_MISSING)result=(io->capabilities&FS_STORAGE_READ_ONLY) ||
            io->remove(temps[doc])?FS_OK:FS_IO_ERROR;
        if(result!=FS_OK)return error->result=result;
    }
    FsConsole_RefreshStartup(&config->console);
    /* A pre-Bank Mover session reads policy/defaults without publishing user
       files or altering the directory-based first-migration precondition. */
    /* Bank 尚未初始化时，Mover 仅读取策略／默认值，不落盘、不改变按目录判断的首次迁移条件。 */
    if(io->capabilities&FS_STORAGE_READ_ONLY) {
        complete(config);*error=(FsConfigError){0};return FS_OK;
    }
    complete(config);result=saveDocuments(io,config,(1u<<DOC_SHARED)|(1u<<DOC_USER));
    if(result==FS_OK){FsConsole_CommitStartup();*error=(FsConfigError){0};}else error->result=result;
    return result;
}
int FsAccountConfiguration_Edit(const FsStorageIo *io,FsConfiguration *config,const FsConfigEdit *edits,
    uint32_t count,FsConfigError *error)
{
    if(!io || !config || !edits || !count || count>FS_CONFIG_EDIT_LIMIT || !error)return FS_INVALID;
    if(io->capabilities&FS_STORAGE_READ_ONLY)return error->result=FS_MISSING;
    for(uint32_t i=0;i<count;i++)
        if((edits[i].action!=FS_CONFIG_SET && edits[i].action!=FS_CONFIG_REMOVE_KEY) ||
            !edits[i].section || (strcmp(edits[i].section,"ticket") && strcmp(edits[i].section,"console")) || !edits[i].key ||
            (edits[i].action==FS_CONFIG_SET && !edits[i].value))return FS_INVALID;
    int result=FsAccountConfiguration_Load(io,config,error);if(result!=FS_OK)return result;
    uint32_t mask=0;
    for(uint32_t i=0;i<count;i++) {
        if(!strcmp(edits[i].section,"console")) {
            int valid=0;
            const char *value=edits[i].value;
            if(!strcmp(edits[i].key,"autoRegion")) {
                if(edits[i].action==FS_CONFIG_REMOVE_KEY){config->console.autoRegion=0;valid=1;}
                else if(!strcmp(value,"true") || !strcmp(value,"false")) {
                    config->console.autoRegion=!strcmp(value,"true");valid=1;
                }
            }
            for(uint32_t field=0;field<FS_CONSOLE_FIELDS;field++)if(!strcmp(edits[i].key,FsConsole_Key(field))) {
                if(edits[i].action==FS_CONFIG_REMOVE_KEY) {
                    FsConsoleProfile factory;FsConsole_Default(&factory);
                    config->console.values[field]=factory.values[field];valid=1;
                } else valid=FsConsole_Parse(&config->console,field,value);
            }
            if(!valid)return FS_INVALID;
            mask|=1u<<DOC_USER;continue;
        }
        int field=!strcmp(edits[i].key,"autoCurrency")?FS_TICKET_FIELD_AUTOCURRENCY:FsConfiguration_TicketField(edits[i].key);
        if(field==FS_TICKET_FIELD_AUTOCURRENCY && strcmp(edits[i].key,"autoCurrency"))return FS_INVALID;
        if(field<0 || edits[i].action==FS_CONFIG_REMOVE_SECTION)return FS_INVALID;
        const char *value=edits[i].action==FS_CONFIG_REMOVE_KEY?FsConfiguration_TicketValue((uint32_t)field):edits[i].value;
        result=FsConfiguration_ParseTicket(config,(uint32_t)field,value);if(result!=FS_OK)return result;
        mask|=1u<<document((uint32_t)field);if(shopField((uint32_t)field))mask|=1u<<DOC_USER;
    }
    complete(config);return error->result=saveDocuments(io,config,mask);
}
int FsAccountConfiguration_Purchase(const FsStorageIo *io,FsConfiguration *config,const FsTicketPolicy *updated,
    uint32_t market,uint32_t balance,uint32_t paidField,FsConfigError *error)
{
    if(!io || !config || !error || (market>=FS_SHOP_MARKET_COUNT && market!=FS_SHOP_UNKNOWN) || balance>FS_SHOP_MONEY_LIMIT)return FS_INVALID;
    if(io->capabilities&FS_STORAGE_READ_ONLY)return error->result=FS_MISSING;
    FsTicketPolicy previous=config->ticket;FsShopPolicy shop=config->shop;FsTicketSettings settings=config->ticketSettings;
    FsConsoleProfile console=config->console;
    int result=FsAccountConfiguration_Load(io,config,error);if(result!=FS_OK)return result;
    if(memcmp(&previous,&config->ticket,sizeof(previous)) || memcmp(&shop,&config->shop,sizeof(shop)) ||
        memcmp(&settings,&config->ticketSettings,sizeof(settings)) ||
        memcmp(&console,&config->console,sizeof(console)))return error->result=FS_LOCKED;
    if(updated) {result=FsConfiguration_RecordPurchase(config,paidField,updated);if(result!=FS_OK)return result;}
    *FsConfiguration_ShopValue(config,market,2)=balance;
    /* Pass and wallet are published together in ONE user.conf image. */
    return error->result=saveDocuments(io,config,1u<<DOC_USER);
}
