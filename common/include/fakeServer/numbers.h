#ifndef COMMON_FAKE_SERVER_NUMBERS_H
#define COMMON_FAKE_SERVER_NUMBERS_H

#include <stdint.h>

/* Freestanding integer division; no ARM compiler-runtime dependency. */
/* 独立整数除法，不依赖 ARM 编译器运行库。 */
uint32_t FsDivideU32(uint32_t numerator,uint32_t denominator,uint32_t *remainder);
int FsParseU32(const char *text,uint32_t maximum,uint32_t *result);

#endif
