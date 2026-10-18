#include "fakeServer/custom_settings.h"
#include "fakeServer/configuration.h"
#include "fakeServer/numbers.h"
#include "fs_system/protected_file.h"
#include <string.h>

static const uint32_t bits[]={FS_CUSTOM_MOVER_V12,FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN};
const char *FsCustom_Key(uint32_t index)
{ return index==0?"enableMoverv1.2":index==1?"allowFanTranslationCHN":0; }
const char *FsCustom_Value(uint32_t options,uint32_t index)
{ return index<FS_CUSTOM_COUNT && (options&bits[index])?"true":"false"; }
int FsCustom_Parse(uint32_t *options,const char *key,const char *value)
{
    if(strcmp(value,"true") && strcmp(value,"false"))return 0;
    for(uint32_t i=0;i<FS_CUSTOM_COUNT;i++)if(!strcmp(key,FsCustom_Key(i))) {
        if(!strcmp(value,"true"))*options|=bits[i];else *options&=~bits[i];
        return 1;
    }
    return 0;
}

static int blank(char c) { return c==' ' || c=='\t' || c=='\r'; }
static int span(const char *first,const char *end,const char *word)
{ return (uint32_t)(end-first)==strlen(word) && !memcmp(first,word,(uint32_t)(end-first)); }
/* Preserve all unrelated bytes, line endings and comments. Only enabled in
   the named rule is canonicalized, including duplicate/damaged assignments.
   保留无关字节、换行和注释；只规范指定段的 enabled，合并其重复赋值。 */
int FsCustom_ApplyMover(const FsStorageIo *io,uint32_t options,uint32_t source)
{
    if(!(options&FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN))return FS_OK;
    uint32_t target=source==FS_MOVER_GEN5?FS_CONFIG_MOVER_NDS:
        source==FS_MOVER_VC?FS_CONFIG_MOVER_VC:0;
    if(!target || !io || !io->allocate || !io->release)return FS_INVALID;
    const char *path=FsConfiguration_Path(target,0),*temp=FsConfiguration_Path(target,1);
    char *input=0;uint32_t size=0;
    int result=FsText_Read(io,path,&input,&size);
    if(result==FS_MISSING)return FS_OK;
    if(result!=FS_OK)return result;
    uint32_t sections=0;
    for(uint32_t i=0;i<size;i++) {
        if(!input[i]){io->release(input);return FS_INVALID;}
        if(input[i]=='[')sections++;
    }
    if(sections>FsDivideU32(UINT32_MAX-size-1u,24u,0)){io->release(input);return FS_NO_MEMORY;}
    char *output=io->allocate(size+1u+sections*24u);
    if(!output){io->release(input);return FS_NO_MEMORY;}
    char *out=output;const char *cursor=input,*limit=input+size;
    int selected=0,written=0;
    while(cursor<limit) {
        const char *first=cursor;
        while(cursor<limit && *cursor!='\n')cursor++;
        if(cursor<limit)cursor++;
        const char *start=first,*end=cursor;
        if(end>start && end[-1]=='\n')end--;
        if(first==input && end-start>=3 && (uint8_t)start[0]==0xef &&
            (uint8_t)start[1]==0xbb && (uint8_t)start[2]==0xbf)start+=3;
        while(start<end && blank(*start))start++;
        const char *comment=start;while(comment<end && *comment!='#')comment++;
        const char *visible=comment;while(visible>start && blank(visible[-1]))visible--;
        if(start<visible && *start=='[') {
            if(selected && !written) {
                if(out>output && out[-1]!='\n')*out++='\n';
                memcpy(out,"enabled=0;\n",11);out+=11;
            }
            const char *name=start+1,*close=name;
            while(close<visible && *close!=']')close++;
            const char *nameEnd=close;
            while(name<nameEnd && blank(*name))name++;
            while(nameEnd>name && blank(nameEnd[-1]))nameEnd--;
            selected=close<visible && close+1==visible && span(name,nameEnd,"rule.BanUnicodeChinese");written=0;
        } else if(selected && start<visible && *start!='#') {
            const char *keyEnd=start;
            while(keyEnd<visible && *keyEnd!='=' && !blank(*keyEnd))keyEnd++;
            const char *equal=keyEnd;while(equal<visible && blank(*equal))equal++;
            if(span(start,keyEnd,"enabled") && (equal==visible || *equal=='=')) {
                if(!written) {
                    const char *value=equal<visible?equal+1:equal;
                    while(value<visible && blank(*value))value++;
                    const char *after=value<visible && *value=='0'?value+1:value;
                    while(after<visible && blank(*after))after++;
                    if(value<visible && *value=='0' && after<visible && *after==';' && after+1==visible) {
                        memcpy(out,first,(uint32_t)(cursor-first));out+=cursor-first;written=1;continue;
                    }
                    memcpy(out,first,(uint32_t)(start-first));out+=start-first;
                    memcpy(out,"enabled=0;",10);out+=10;
                    const char *commentEnd=end;
                    if(end<cursor && commentEnd>comment && commentEnd[-1]=='\r')commentEnd--;
                    if(comment<commentEnd){*out++=' ';memcpy(out,comment,(uint32_t)(commentEnd-comment));out+=commentEnd-comment;}
                    if(end<cursor && end>first && end[-1]=='\r')*out++='\r';
                    if(end<cursor)*out++='\n';
                    written=1;
                }
                continue;
            }
        }
        memcpy(out,first,(uint32_t)(cursor-first));out+=cursor-first;
    }
    if(selected && !written) {
        if(out>output && out[-1]!='\n')*out++='\n';
        memcpy(out,"enabled=0;\n",11);out+=11;
    }
    uint32_t bytes=(uint32_t)(out-output);
    if(bytes!=size || memcmp(output,input,size))result=FsText_Publish(io,path,temp,output,bytes);
    io->release(output);io->release(input);return result;
}
