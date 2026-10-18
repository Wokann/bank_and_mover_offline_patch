#include "fs_system/fs_helpers.h"

enum { SUPPORT_NATIVE_LINE=43, SUPPORT_INVALID_LINE=130 };
static u32 supportLine __attribute__((section(".data.support_code")))=SUPPORT_NATIVE_LINE;

static u32 observe(u32 principal,u32 command,u32 normalWords,int friendKey)
{
    /* Observe the original call's immediate IPC reply. No extra service read,
       login, account-name guess, replacement ID or network request is made. */
    /* 只观察原调用刚完成的 IPC 响应，不再次读取、不登录、不猜账号名或伪造 ID。 */
    volatile u32 *c=fsCommandBuffer();
    int valid=principal && principal!=0xffffffffu && (c[0]>>16)==command &&
        ((c[0]>>6)&63)>=normalWords && !(c[1]&0x80000000u);
    if(friendKey)valid=valid && c[2]==principal;
    supportLine=valid?SUPPORT_NATIVE_LINE:SUPPORT_INVALID_LINE;
    return principal;
}
u32 SupportCode_ReadActPrincipal(void)
{
    u32 value=((u32 (*)(void))0x0021E5BCu)();
    return observe(value,6,1,0);
}
u32 SupportCode_ReadFriendPrincipal(void)
{
    u32 value=((u32 (*)(void))0x00229D48u)();
    return observe(value,5,5,1);
}
u32 SupportCode_MenuMessage(void) { return supportLine; }
