#include "fs_system/utf_path.h"
#include <string.h>

int FsUtf8_Next(const char **cursor,const char *end,uint32_t *scalar)
{
    if(!cursor || !*cursor || *cursor>=end || !scalar)return 0;
    const unsigned char *p=(const unsigned char *)*cursor;
    uint32_t c=*p++,extra=0,minimum=0;
    if(c<0x80) {}
    else if(c>=0xc2 && c<=0xdf){c&=31;extra=1;minimum=0x80;}
    else if(c>=0xe0 && c<=0xef){c&=15;extra=2;minimum=0x800;}
    else if(c>=0xf0 && c<=0xf4){c&=7;extra=3;minimum=0x10000;}
    else return 0;
    if((uint32_t)(end-(const char *)p)<extra)return 0;
    for(uint32_t i=0;i<extra;i++) {
        if((*p&0xc0)!=0x80)return 0;
        c=(c<<6)|(*p++&63);
    }
    if(c<minimum || c>0x10ffff || (c>=0xd800 && c<=0xdfff))return 0;
    *cursor=(const char *)p;*scalar=c;return 1;
}
int FsUtf8_ToUtf16(const char *text,uint16_t *out,uint32_t capacity,uint32_t *units)
{
    if(!text || !out || !capacity)return 0;
    const char *end=text+strlen(text);uint32_t used=0;
    while(text<end) {
        uint32_t c;if(!FsUtf8_Next(&text,end,&c))return 0;
        uint32_t size=c>0xffff?2:1;
        if(size>=capacity-used)return 0;
        if(size==2){c-=0x10000;out[used++]=(uint16_t)(0xd800+(c>>10));out[used++]=(uint16_t)(0xdc00+(c&1023));}
        else out[used++]=(uint16_t)c;
    }
    out[used]=0;if(units)*units=used;return 1;
}
int FsUtf16_ToUtf8(const uint16_t *text,uint32_t units,char *out,uint32_t capacity)
{
    if(!text || !out || !capacity)return 0;
    uint32_t used=0;
    for(uint32_t i=0;i<units && text[i];i++) {
        uint32_t c=text[i];
        if(c>=0xd800 && c<=0xdbff) {
            if(i+1>=units || text[i+1]<0xdc00 || text[i+1]>0xdfff)return 0;
            c=0x10000+((c-0xd800)<<10)+(text[++i]-0xdc00);
        } else if(c>=0xdc00 && c<=0xdfff)return 0;
        uint32_t size=c<0x80?1:c<0x800?2:c<0x10000?3:4;
        if(size>=capacity-used)return 0;
        if(size==1)out[used++]=(char)c;
        else {
            out[used++]=(char)((size==2?0xc0:size==3?0xe0:0xf0)|(c>>(6*(size-1))));
            for(uint32_t n=size-1;n;n--)out[used++]=(char)(0x80|((c>>(6*(n-1)))&63));
        }
    }
    out[used]=0;return 1;
}
