#include "fs_system/console_identity.h"
#include "fs_system/fs_helpers.h"
#include "fs_system/console_settings.h"
#include <string.h>

static int accountInfo(u32 handle,u32 slot,u32 field,void *data,u32 size)
{
    memset(data,0xa5,size);
    volatile u32 *c=fsCommandBuffer();
    c[0]=0x000600C2u;c[1]=slot;c[2]=size;c[3]=field;
    c[4]=(size<<4)|0xCu;c[5]=(u32)data;
    return !fsSync(handle) && !c[1];
}
static int networkIdentity(FsUserIdentity *identity)
{
    if(!identity)return 0;
    *identity=(FsUserIdentity){0};u32 frd=0,act=0;
    if(SRV_GetServiceHandle(&frd,"frd:u",5,1) || !frd)return 0;
    volatile u32 *c=fsCommandBuffer();
    c[0]=0x00320042u;c[1]=0x070000C8u;c[2]=0x20u;c[3]=0;
    int ok=!fsSync(frd) && !c[1];
    c[0]=0x00300000u;c[2]=c[3]=c[4]=0xffffffffu;
    ok=ok && !fsSync(frd) && !c[1] && ((c[0]>>6)&63)>=4 && c[2]<=1;
    u32 environment=ok?c[2]:0xffffffffu;
    c[0]=0x000B0000u;c[2]=0xffffffffu;
    ok=ok && !fsSync(frd) && !c[1] && ((c[0]>>6)&63)>=2 && c[2]>0 && c[2]<0xfe;
    u32 friendSlot=ok?c[2]:0xffffffffu;
    if(fsCloseHandle(frd))ok=0;
    if(!ok || SRV_GetServiceHandle(&act,"act:u",5,1) || !act)return 0;
    c[0]=0x00010084u;c[1]=0x0B0502C8u;c[2]=0;c[3]=0x20u;c[4]=0;c[5]=0;c[6]=0;
    ok=!fsSync(act) && !c[1];
    char name[17];u8 linked=0xa5;
    if(ok)ok=accountInfo(act,0xfe,8,name,sizeof(name)) &&
        accountInfo(act,0xfe,0x2b,&linked,sizeof(linked));
    /* A successful HLE stub leaves sentinels unchanged, not an empty account.
       ACT and FRD must refer to the same selected local account. */
    if(ok) {
        ok=FsUser_ValidAccountName(name) && linked==friendSlot;
    }
    if(fsCloseHandle(act))ok=0;
    if(ok) {
        identity->kind=environment?FS_USER_PNID:FS_USER_NNID;
        strcpy(identity->accountName,name);
    }
    return ok;
}

int ConsoleIdentity_Read(FsUserIdentity *identity)
{
    if(!identity)return 0;
    if(networkIdentity(identity))return 1;
    *identity=(FsUserIdentity){0};
    return ConsoleSettings_GetUserName(identity->accountName,sizeof(identity->accountName));
}
