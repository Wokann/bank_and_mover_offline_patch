#include "fs_system/fs_helpers.h"
#include "fs_system/patch_paths.h"

/* Checked SD operations using the calling title's SDK address map. */
/* 带检查的 SD 操作，使用调用软件自己的 SDK 地址表。 */
volatile u32 *fsCommandBuffer(void)
{
    u32 tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));
    return (volatile u32 *)(tls + 0x80u);
}

s32 fsSync(u32 handle)
{
    register u32 r0 __asm__("r0") = handle;
    __asm__ volatile("svc 0x32" : "+r"(r0) : : "r1", "r2", "r3", "r12", "memory", "cc");
    return (s32)r0;
}

s32 fsCloseHandle(u32 handle)
{
    register u32 r0 __asm__("r0")=handle;
    __asm__ volatile("svc 0x23":"+r"(r0)::
        "r1","r2","r3","r12","memory","cc");
    return (s32)r0;
}

s32 closeFile(u32 handle)
{
    u32 localHandle=handle;
    s32 result,closeResult;
    if (!handle) return 0;
    result=FSFILE_Close(&localHandle);
    closeResult=fsCloseHandle(handle);
    return result?result:closeResult;
}

s32 openArchive(u64 *archive)
{
    volatile u32 *c = fsCommandBuffer();
    s32 r;
    c[0]=FSUSER_CMD_OPEN_ARCHIVE; c[1]=FS_ARCHIVE_ID_SDMC; c[2]=FS_PATH_TYPE_EMPTY; c[3]=1;
    c[4]=(1u<<14)|2u; c[5]=(u32)emptyPath;
    r=fsSync(*FSUSER_HandleSlot); if (r) return r; r=(s32)c[1];
    if (!r) *archive=(u64)c[2]|((u64)c[3]<<32);
    return r;
}

s32 closeArchive(u64 archive)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSUSER_CMD_CLOSE_ARCHIVE; c[1]=(u32)archive; c[2]=(u32)(archive>>32);
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

s32 pathCommand(u32 command,u64 archive,const char *path,u32 pathSize)
{
    volatile u32 *c=fsCommandBuffer(); s32 result;
    c[0]=command; c[1]=0; c[2]=(u32)archive; c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=pathSize; c[6]=(pathSize<<14)|2u; c[7]=(u32)path;
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize)
{
    volatile u32 *c=fsCommandBuffer(); s32 result;
    c[0]=FSUSER_CMD_RENAME_FILE; c[1]=0; c[2]=(u32)archive; c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII; c[5]=fromSize; c[6]=(u32)archive; c[7]=(u32)(archive>>32);
    c[8]=FS_PATH_TYPE_ASCII; c[9]=toSize; c[10]=(fromSize<<14)|0x402u; c[11]=(u32)from;
    c[12]=(toSize<<14)|0x802u; c[13]=(u32)to;
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

s32 setSize(u32 handle,u64 size)
{
    volatile u32 *c=fsCommandBuffer(); s32 result;
    c[0]=FSFILE_CMD_SET_SIZE; c[1]=(u32)size; c[2]=(u32)(size>>32);
    result=fsSync(handle);
    return result?result:(s32)c[1];
}

s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle)
{
    return FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,emptyPath,1,FS_PATH_TYPE_ASCII,path,pathSize,flags,0);
}

int resultIsNotFound(s32 result)
{
    return (((u32)result>>RESULT_SUMMARY_SHIFT)&RESULT_SUMMARY_MASK)==
        RESULT_SUMMARY_NOT_FOUND;
}
