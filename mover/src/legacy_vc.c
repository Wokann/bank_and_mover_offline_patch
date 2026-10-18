#include "legacy_vc.h"
#include "fakeServer/fake_server_client.h"

static int legacy(void)
{
    return !CombinePatch_ModeStorage[0] &&
        (FakeServerClient_CustomOptions()&FS_CUSTOM_MOVER_V12);
}
/* The RGBY caller provides a native construction template. The native
   constructor still generates PID, random gender, IVs and checksum itself.
   Only its shiny selector changes; Gen2/NDS calls are never redirected here.
   仅更改第一世代原构造参数的闪光选择；第二世代、NDS 完全不进入此包装。 */
void *MoverLegacy_Create(void *object,u32 heap,void *settings,u32 mode,u32 dvs)
{
    if(settings && legacy()) {
        u32 attack=(dvs>>4)&15,defense=dvs&15,speed=(dvs>>12)&15,special=(dvs>>8)&15;
        ((u32 *)settings)[3]=(attack==10 && (defense&2) && speed==10 && special==10)?2:1;
    }
    return MoverLegacy_OriginalCreate(object,heap,settings,mode);
}
/* v1.2 stopped after random construction; v1.5 subsequently overwrites
   mixed-gender species from Attack DV. Suppress just that overwrite when
   explicitly requested, leaving genderless and fixed-gender species native.
   v1.2 不做攻击 DV 性别覆盖；只跳过此覆盖，不把无性别／固定性别变成随机。 */
void MoverLegacy_SetGender(void *object,u32 gender)
{
    if(!legacy())MoverLegacy_OriginalSetGender(object,gender);
}

/* Reconstruct the old table from the current native table plus its complete
   byte-for-byte delta. Do not apply v1.5's added language-specific fixes.
   按原表的完整差异恢复旧表，不使用新版后加的语言专属转码修正。 */
u16 MoverLegacy_Character(u32 byte,u32 alphabet)
{
    if(byte>255 || alphabet>5)return 0;
    if(!alphabet) {
        if(byte==0xe6 || byte==0xe7 || byte>=0xf6)return 0x3000;
        return ((const u16 *)0x002c2c54u)[byte];
    }
    if(byte==0xe8)return '-';
    if(byte==0xba || byte==0xbb || (byte>=0xbd && byte<=0xbf) ||
        (byte>=0xc6 && byte<=0xd6) || byte==0xe0 || byte==0xe4 || byte==0xe5 ||
        byte==0xe9 || byte==0xea || byte>=0xf6)return ' ';
    return ((const u16 *)0x002c2e54u)[byte];
}
u32 MoverLegacy_Decode(void *context,void *out,const u8 *raw,u32 bytes,
    u32 alphabet,u32 mode,u32 species,u32 zeroTerminated)
{
    if(!legacy() || !context || !out || !raw || alphabet>5 || !bytes || bytes>13 ||
        (raw[0]==0x5d && bytes>1 && raw[1]==0x50))
        return MoverLegacy_OriginalDecode(context,out,raw,bytes,alphabet,mode,species,zeroTerminated);
    u16 text[13]={0};u32 spaces=0;
    for(u32 i=0;i+1<bytes;i++) {
        text[i]=zeroTerminated && !raw[i]?0:MoverLegacy_Character(raw[i],alphabet);
        if(text[i]==(alphabet?' ':0x3000))spaces++;
    }
    MoverLegacy_AssignText(out,text);
    /* Native kana disambiguation is unchanged between the compared releases. */
    /* 已对比两版假名消歧逻辑，保留共用的原字符串对象与所有权流程。 */
    MoverLegacy_NormalizeKana(context,out);
    return spaces==bytes-1;
}
