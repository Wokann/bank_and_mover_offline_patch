#ifndef COMMON_UTF_PATH_H
#define COMMON_UTF_PATH_H
#include <stdint.h>
/* Strict scalar decoding: reject truncation, overlongs and surrogate scalars. */
int FsUtf8_Next(const char **cursor,const char *end,uint32_t *scalar);
int FsUtf8_ToUtf16(const char *,uint16_t *,uint32_t capacity,uint32_t *units);
int FsUtf16_ToUtf8(const uint16_t *,uint32_t units,char *,uint32_t capacity);
#endif
