#include "fakeServer/custom_settings.h"
#include "fakeServer/configuration.h"
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

void FsCustom_ApplyMover(FsMoverPolicy *policy)
{
    if(!policy || !(policy->customOptions&FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN))return;
    for(uint32_t i=0;i<policy->ruleCount;i++)
        if(!strcmp(policy->rules[i].name,"BanUnicodeChinese"))policy->rules[i].enabled=0;
}
