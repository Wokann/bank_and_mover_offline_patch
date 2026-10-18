#ifndef COMMON_CONF_TEXT_H
#define COMMON_CONF_TEXT_H

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
