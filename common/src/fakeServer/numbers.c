#include "fakeServer/numbers.h"

int FsParseU32(const char *text,uint32_t maximum,uint32_t *out)
{
    uint32_t result=0,last,threshold=FsDivideU32(maximum,10,&last);
    if (!text || !out || !*text) return 0;
    while (*text) {
        if (*text<'0' || *text>'9') return 0;
        uint32_t digit=(uint32_t)(*text++-'0');
        if (result>threshold || (result==threshold && digit>last)) return 0;
        result=result*10+digit;
    }
    *out=result;
    return 1;
}

uint32_t FsDivideU32(uint32_t numerator,uint32_t denominator,uint32_t *remainder)
{
    uint32_t quotient=0,rest=0;
    if (!denominator) { if (remainder) *remainder=0; return 0; }
    for (uint32_t bit=0;bit<32;bit++) {
        uint32_t overflow=rest>>31;
        rest=(rest<<1)|(numerator>>31);
        numerator<<=1; quotient<<=1;
        if (overflow || rest>=denominator) { rest-=denominator; quotient|=1; }
    }
    if (remainder) *remainder=rest;
    return quotient;
}
