#ifndef COMMON_CONF_TEXT_H
#define COMMON_CONF_TEXT_H

#include <stdint.h>

/* Assign only after recognizing a complete switch value. */
/* 仅在识别到完整的开关值后赋值，坏的重复项不覆盖合法值。 */
static inline int FsConf_Bool(const char *text,uint32_t *value)
{
    if(text[0]=='t' && text[1]=='r' && text[2]=='u' && text[3]=='e' && !text[4]) {
        *value=1;return 1;
    }
    if(text[0]=='f' && text[1]=='a' && text[2]=='l' && text[3]=='s' && text[4]=='e' && !text[5]) {
        *value=0;return 1;
    }
    return 0;
}

/* Call only for a configuration assignment, never for an NGWord/name line.
   仅用于配置赋值；NGWord 和白名单正文不得调用，分号可能是其有效内容。
   Legacy values without a terminator remain readable; writers always emit it. */
static inline void FsConf_Value(char *text)
{
    char *end=text;
    while(*end)end++;
    while(end>text && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r'))end--;
    if(end>text && end[-1]==';')end--;
    while(end>text && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r'))end--;
    *end=0;
}
#endif
