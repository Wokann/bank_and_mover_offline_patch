#include "fakeServer/mover_checks.h"

/* Unicode 17 CJK ideograph blocks, not kana, Hangul, punctuation or PUA.
   Unicode 17 汉字区块，不把假名、韩文、标点或私用区误当作汉字。 */
static int han(uint32_t value)
{
    static const uint32_t ranges[][2]={
        {0x3400,0x4dbf},{0x4e00,0x9fff},{0xf900,0xfaff},
        {0x20000,0x2a6df},{0x2a700,0x2b73f},{0x2b740,0x2b81f},
        {0x2b820,0x2ceaf},{0x2ceb0,0x2ebef},{0x2ebf0,0x2ee5f},
        {0x2f800,0x2fa1f},{0x30000,0x3134f},{0x31350,0x323af},
        {0x323b0,0x3347f}
    };
    for(uint32_t i=0;i<sizeof(ranges)/sizeof(*ranges);i++)
        if(value>=ranges[i][0] && value<=ranges[i][1])return 1;
    return 0;
}
int FsMover_HasHan(const uint16_t *name,uint32_t units)
{
    if(!name)return 0;
    for(uint32_t i=0;i<units && name[i] && name[i]!=0xffff;i++) {
        uint32_t value=name[i];
        if(value>=0xd800 && value<=0xdbff && i+1<units &&
            name[i+1]>=0xdc00 && name[i+1]<=0xdfff)
            value=0x10000+((value-0xd800)<<10)+(name[++i]-0xdc00);
        if(han(value))return 1;
    }
    return 0;
}
