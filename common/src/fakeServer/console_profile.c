#include "fakeServer/console_profile.h"
#include "fakeServer/numbers.h"
#include <string.h>

static FsConsoleProfile host __attribute__((section(".data.console_host")))={
    {FS_CONSOLE_UNKNOWN,FS_CONSOLE_UNKNOWN,FS_CONSOLE_UNKNOWN,FS_CONSOLE_UNKNOWN},0,1};
static struct {uint32_t *selected,applied;} startup
    __attribute__((section(".data.console_startup")))={0};

static uint32_t maximum(uint32_t field)
{ return field==FS_CONSOLE_REGION?6:field==FS_CONSOLE_LANGUAGE?11:255; }

void FsConsole_SetHost(const FsConsoleProfile *value)
{
    for(uint32_t i=0;i<FS_CONSOLE_FIELDS;i++)
        host.values[i]=value && value->values[i]<=maximum(i)?value->values[i]:FS_CONSOLE_UNKNOWN;
    host.autoRegion=0;host.ready=1;startup.selected=0;startup.applied=0;
}
void FsConsole_SelectStartup(uint32_t *applied) { startup.selected=applied; }
void FsConsole_RefreshStartup(FsConsoleProfile *profile)
{
    uint32_t applied=startup.selected?*startup.selected:startup.applied;
    if(profile->autoRegion && !applied)
        for(uint32_t i=0;i<FS_CONSOLE_FIELDS;i++)
            if(host.values[i]!=FS_CONSOLE_UNKNOWN)profile->values[i]=host.values[i];
}
void FsConsole_CommitStartup(void)
{ if(startup.selected)*startup.selected=1;else startup.applied=1; }
void FsConsole_Default(FsConsoleProfile *out) { *out=host; }
void FsConsole_Resolve(const FsConsoleProfile *profile,FsConsoleProfile *out)
{
    *out=profile && profile->ready?*profile:host;
    out->autoRegion=0;out->ready=1;
}
const char *FsConsole_Key(uint32_t field)
{
    static const char *const keys[]={"region","language","country","state"};
    return field<FS_CONSOLE_FIELDS?keys[field]:0;
}
int FsConsole_Parse(FsConsoleProfile *profile,uint32_t field,const char *text)
{
    if(!profile || field>=FS_CONSOLE_FIELDS || !text)return 0;
    uint32_t value=FS_CONSOLE_UNKNOWN;
    if(strcmp(text,"none")) {
        if(text[0]=='0' && (text[1]=='x' || text[1]=='X')) {
            const char *p=text+2;value=0;if(!*p)return 0;
            while(*p) {
                uint32_t ch=(uint8_t)*p++,digit=ch>='0' && ch<='9'?ch-'0':
                    ch>='a' && ch<='f'?ch-'a'+10:ch>='A' && ch<='F'?ch-'A'+10:16;
                if(digit>=16 || value>(maximum(field)>>4))return 0;
                value=(value<<4)|digit;if(value>maximum(field))return 0;
            }
        } else if(!FsParseU32(text,maximum(field),&value))return 0;
    }
    profile->values[field]=value;return 1;
}
uint32_t FsConsole_CountryCode(uint32_t country)
{
    /* Public CFG country-ID mapping; generated from the documented reference. */
    static const uint16_t codes[187]={
#include "console_countries.inc"
    };
    return country<sizeof(codes)/sizeof(*codes)?codes[country]:0;
}
void FsConsole_Location(const FsConsoleProfile *profile,uint32_t *region,uint32_t *country)
{
    FsConsoleProfile value;FsConsole_Resolve(profile,&value);
    *region=value.values[FS_CONSOLE_REGION];
    *country=FsConsole_CountryCode(value.values[FS_CONSOLE_COUNTRY]);
}
