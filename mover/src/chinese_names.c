#include "chinese_names.h"
#include "fakeServer/fake_server_client.h"

#include "vc_chinese_us.inc"
#include "vc_chinese_ko.inc"

static int custom(u32 source)
{
    (void)source;
    return !CombinePatch_ModeStorage[0] &&
        (FakeServerClient_CustomOptions()&FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN);
}

/* The native single-byte alphabets remain authoritative for mixed text.
   混排文本的单字节字符仍使用 v1.5 原始转换表，不复制或替换原字符表。 */
static u16 single(u8 value,u32 alphabet)
{
    if(alphabet==1)return value==0xd6?'v':((const u16 *)0x002c2e54u)[value];
    const u16 *pairs=(const u16 *)0x002c23d0u;
    for(u32 i=0;i<256 && pairs[2*i+1];i++)
        if(pairs[2*i+1]==value)return pairs[2*i];
    return 0;
}

int MoverNames_DecodeVc(const u8 *raw,u32 bytes,u32 alphabet,u16 out[13])
{
    if(!raw || !out || (alphabet!=1 && alphabet!=6))return 0;
    const u16 *table=alphabet==1?chinese_us:chinese_ko;
    u32 size=alphabet==1?sizeof(chinese_us)/sizeof(*chinese_us):sizeof(chinese_ko)/sizeof(*chinese_ko);
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

static void storeNames(MoverVcCandidateView *candidate,const u16 *nickname,const u16 *trainer)
{
    if(!candidate || !candidate->record || (!nickname && !trainer))return;
    void *record=candidate->record;
    Pokemon_BeginAccess(record);
    if(*(const u16 *)Pokemon_GetGrowthBlock(record,0)) {
        /* Writing accessors mark the native record dirty; EndAccess performs
           its original checksum/encryption. Keep the native nickname flag. */
        /* 写访问器登记修改，EndAccess 沿用原校验／加密；不伪造昵称标志。 */
        if(nickname)copyName((u16 *)Pokemon_GetMoveBlock(record,1),nickname,12);
        if(trainer)copyName((u16 *)Pokemon_GetTrainerBlock(record,1),trainer,12);
    }
    Pokemon_EndAccess(record);
}

static MoverVcCandidateView *convertGen5(void *context,MoverGen5RecordView *raw,u32 decision)
{
    MoverVcCandidateView *result=MoverNames_OriginalGen5(context,raw);
    if(!result || !raw || !custom(FS_MOVER_GEN5) || !(raw->control&MOVER_GEN5_BODY_DECODED))return result;
    const u8 *row=MoverGen5_BlockOrderTable+4*((raw->pid>>13)&31);
    const u8 *body=(const u8 *)raw->body;
    u16 nickname[13],trainer[13];
    copyName(nickname,(const u16 *)(body+row[2]),10);
    copyName(trainer,(const u16 *)(body+row[3]),7);
    /* Native screening clears a rejected nickname's flag, not its source
       text. Do not mistake that for an unnicknamed fan-translation name.
       原筛选会清昵称标志但保留旧文本；不得当成无昵称汉化名再次恢复。 */
    storeNames(result,decision!=FS_NAME_FIX_NICKNAME && decision!=FS_NAME_FIX_BOTH &&
        FsMover_HasHan(nickname,13)?nickname:0,
        decision!=FS_NAME_FIX_TRAINER && decision!=FS_NAME_FIX_BOTH &&
        FsMover_HasHan(trainer,13)?trainer:0);
    return result;
}
MoverVcCandidateView *MoverNames_ConvertGen5(void *context,MoverGen5RecordView *raw)
{ return convertGen5(context,raw,FS_NAME_UNCHANGED); }

MoverVcCandidateView *MoverNames_ConvertGen5Checked(void *context,MoverGen5RecordView *raw,u32 decision)
{ return convertGen5(context,raw,decision); }

/* Run only after each complete native bulk conversion. Ownership, rejection,
   metadata and callbacks remain native; no pointer is sent to a worker here. */
/* 仅在原批量转换返回后更新候选名字；对象所有权、筛选、元数据、回调均保留。 */
static void convertVc(void *output,const u8 *trainer,const u8 *nickname,u32 count,u32 alphabet)
{
    if(!custom(FS_MOVER_VC) || !output || !trainer || !nickname ||
        count>30 || (alphabet!=1 && alphabet!=6))return;
    u32 stride=((const u32 *)0x002be948u)[alphabet]-1;
    if(!stride || stride>11)return;
    MoverVcCandidateView *const *candidates=output;
    for(u32 i=0;i<count;i++) {
        u16 nick[13],ot[13];
        int n=MoverNames_DecodeVc(nickname+i*stride,stride,alphabet,nick);
        int t=MoverNames_DecodeVc(trainer+i*stride,stride,alphabet,ot);
        storeNames(candidates[i],n==1?nick:0,t==1?ot:0);
    }
}

void MoverNames_ConvertVcA(void *a,u32 b,void *out,const void *mon,
    const u8 *ot,const u8 *nick,u32 count,u32 alphabet,const void *species)
{
    MoverNames_OriginalVcA(a,b,out,mon,ot,nick,count,alphabet,species);
    convertVc(out,ot,nick,count,alphabet);
}
u32 MoverNames_ConvertVcB(void *a,u32 b,void *out,const void *mon,
    u32 extra,const u8 *ot,const u8 *nick,u32 count,u32 alphabet)
{
    u32 result=MoverNames_OriginalVcB(a,b,out,mon,extra,ot,nick,count,alphabet);
    convertVc(out,ot,nick,count,alphabet);
    return result;
}
void MoverNames_ConvertVcC(void *a,u32 b,void *out,const void *mon,
    const u8 *ot,const u8 *nick,u32 count,u32 alphabet,const void *species)
{
    MoverNames_OriginalVcC(a,b,out,mon,ot,nick,count,alphabet,species);
    convertVc(out,ot,nick,count,alphabet);
}
