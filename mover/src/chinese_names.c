#include "chinese_names.h"
#include "fakeServer/fake_server_client.h"

#include "vc_chinese_us.inc"
#include "vc_chinese_ko.inc"

static int custom(void)
{
    return !CombinePatch_ModeStorage[0] &&
        (FakeServerClient_CustomOptions()&FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN);
}

/* Western alphabets share a byte table, but not every byte has the same
   language-specific meaning. Keep the complete native v1.5 adjustments.
   西文字母表共用字节表，但部分字符随语言变化，保留 v1.5 的全部修正。 */
static u16 single(u8 value,u32 alphabet)
{
    if(alphabet>=1 && alphabet<=5) {
        u16 point=((const u16 *)0x002c2e54u)[value];
        if(alphabet==1 && value==0xd6)return 'v';
        if(alphabet==4 && point==0xc0)return 0xdf;
        if(alphabet==2) {
            switch(point) {
            case 0xc1:return 0xe7;
            case 0xcc:return 0xe2;
            case 0xd2:return 0xea;
            case 0xd3:return 0xee;
            case 0xcd:return 0xf4;
            case 0xd1:return 0xfb;
            }
        }
        if(alphabet==3 || alphabet==5) {
            switch(value) {
            case 0xd0:return 0xec;
            case 0xd1:return 0xed;
            case 0xd2:return 0xf1;
            case 0xd3:return 0xf2;
            case 0xd4:return 0xf3;
            case 0xd5:return 0xfa;
            }
        }
        return point;
    }
    const u16 *pairs=(const u16 *)0x002c23d0u;
    for(u32 i=0;i<256 && pairs[2*i+1];i++)
        if(pairs[2*i+1]==value)return pairs[2*i];
    return 0;
}

int MoverNames_DecodeVc(const u8 *raw,u32 bytes,u32 alphabet,u16 out[13])
{
    if(!raw || !out || alphabet<1 || alphabet>6)return 0;
    const u16 *table=alphabet==6?chinese_ko:chinese_us;
    u32 size=alphabet==6?sizeof(chinese_ko)/sizeof(*chinese_ko):sizeof(chinese_us)/sizeof(*chinese_us);
    u32 length=0,changed=0;
    for(u32 i=0;i<bytes && raw[i]!=0x50;i++) {
        u16 value=0;
        /* A mapped lead consumes its trail, including 0x50. Only an unpaired
           0x50 is the terminator. Do not inspect bytes outside the raw field. */
        /* 有效双字节先消费尾字节（含 0x50）；单独的 0x50 才是结束符。 */
        if(raw[i] && raw[i]<=((size-1)>>8)) {
            if(i+1>=bytes)return -1;
            u32 key=((u32)raw[i]<<8)|raw[i+1];
            if(key>=size || !(value=table[key]))return -1;
            i++;changed=1;
        } else value=single(raw[i],alphabet);
        if(!value || value==0xffff || length>=12)return -1;
        out[length++]=value;
    }
    out[length]=0;
    return changed && length?1:0;
}

static void copyName(u16 *out,const u16 *raw,u32 units)
{
    u32 i=0;
    for(;i<units && i<12 && raw[i] && raw[i]!=0xffff;i++)out[i]=raw[i];
    for(;i<13;i++)out[i]=0;
}

/* Select text before the native setter runs. Never reopen a converted record
   to undo an earlier default-name write or change its nickname flag.
   在原版写入前选定字符串，不重开已转换对象补写，也不修改昵称标记。 */
int MoverNames_Gen5Default(void *out,const u16 *nickname,u32 decision)
{
    if(!out || !nickname || !custom() || decision==FS_NAME_FIX_NICKNAME ||
        decision==FS_NAME_FIX_BOTH)return 0;
    u16 text[13];
    copyName(text,nickname,10);
    /* Keep the native Gen5 glyph/control normalization when selecting source
       text instead of a database name; Han characters themselves are unchanged.
       选择原文仍沿用 Gen5 的特殊字形／控制符处理，汉字本身不改。 */
    for(u32 i=0;i<10 && text[i];i++) {
        if(text[i]<0x20) { text[i]=0;break; }
        for(u32 j=0;j<41;j++)if(text[i]==MoverGen5_NameCharacters[j]) {
            text[i]=MoverGen5_NameCharacters[41+j];break;
        }
    }
    if(!FsMover_HasHan(text,13))return 0;
    MoverNames_AssignText(out,text);
    return 1;
}

/* The fifth argument remains on this call's stack throughout the intact
   native body. Its name-choice bridge reads only this slot's decision.
   第五参数随本次调用保留在栈中，命名桥只读取本槽判定，不使用全局槽位状态。 */
MoverVcCandidateView *MoverNames_ConvertGen5(void *context,MoverGen5RecordView *raw)
{ return MoverNames_OriginalGen5(context,raw,0,0,FS_NAME_UNCHANGED); }

MoverVcCandidateView *MoverNames_ConvertGen5Checked(void *context,MoverGen5RecordView *raw,u32 decision)
{ return MoverNames_OriginalGen5(context,raw,0,0,decision); }

/* The native comparison still decides whether the source is nicknamed. Its
   string temporary is changed only at the subsequent write call, including
   the special Mew branch. Source bytes and comparison inputs stay untouched.
   保留原版昵称比较；只在随后的写入调用选择文本，亦覆盖梦幻专用分支，
   不改来源字节或此前的比较输入。 */
static void selectVcText(void *out,const u8 *raw,u32 alphabet)
{
    if(!custom() || !out || !raw || alphabet<1 || alphabet>6)return;
    u32 stride=((const u32 *)0x002be948u)[alphabet]-1;
    if(!stride || stride>11)return;
    u16 text[13];
    if(MoverNames_DecodeVc(raw,stride,alphabet,text)==1)MoverNames_AssignText(out,text);
}

void MoverNames_SetVcNickname(void *candidate,void *text,const u8 *raw,u32 alphabet)
{
    selectVcText(text,raw,alphabet);
    MoverNames_OriginalSetNickname(candidate,text);
}
void MoverNames_SetVcTrainer(void *candidate,void *text,const u8 *raw,u32 alphabet)
{
    selectVcText(text,raw,alphabet);
    MoverNames_OriginalSetTrainer(candidate,text);
}
