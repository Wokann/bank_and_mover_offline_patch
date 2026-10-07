#include "fs_helpers.h"
#include "patch_paths.h"

/* Shared checked SD filesystem implementation used by Mover's local backend. */
/* Mover 本地后端共用的带检查 SD 文件系统实现。 */
volatile u32 *fsCommandBuffer(void)
{
    u32 tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3":"=r"(tls));
    return (volatile u32 *)(tls+0x80u);
}

s32 fsSync(u32 handle)
{
    register u32 r0 __asm__("r0")=handle;
    __asm__ volatile("svc 0x32":"+r"(r0)::"r1","r2","r3","r12","memory","cc");
    return (s32)r0;
}

s32 fsCloseHandle(u32 handle)
{
    register u32 r0 __asm__("r0")=handle;
    __asm__ volatile("svc 0x23":"+r"(r0)::"r1","r2","r3","r12","memory","cc");
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
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSUSER_CMD_OPEN_ARCHIVE;
    c[1]=FS_ARCHIVE_ID_SDMC;
    c[2]=FS_PATH_TYPE_EMPTY;
    c[3]=1;
    c[4]=(1u<<14)|2u;
    c[5]=(u32)emptyPath;
    result=fsSync(*FSUSER_HandleSlot);
    if (result) return result;
    result=(s32)c[1];
    if (!result) *archive=(u64)c[2]|((u64)c[3]<<32);
    return result;
}

s32 closeArchive(u64 archive)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSUSER_CMD_CLOSE_ARCHIVE;
    c[1]=(u32)archive;
    c[2]=(u32)(archive>>32);
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

s32 pathCommand(u32 command,u64 archive,const char *path,u32 pathSize)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=command;
    c[1]=0;
    c[2]=(u32)archive;
    c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII;
    c[5]=pathSize;
    c[6]=(pathSize<<14)|2u;
    c[7]=(u32)path;
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

static void createDirectory(u64 archive,const char *path,u32 pathSize)
{
    volatile u32 *c=fsCommandBuffer();
    c[0]=FSUSER_CMD_CREATE_DIRECTORY;
    c[1]=0;
    c[2]=(u32)archive;
    c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII;
    c[5]=pathSize;
    c[6]=0;
    c[7]=(pathSize<<14)|2u;
    c[8]=(u32)path;
    (void)fsSync(*FSUSER_HandleSlot);
}

s32 renamePath(u64 archive,const char *from,u32 fromSize,const char *to,u32 toSize)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSUSER_CMD_RENAME_FILE;
    c[1]=0;
    c[2]=(u32)archive;
    c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII;
    c[5]=fromSize;
    c[6]=(u32)archive;
    c[7]=(u32)(archive>>32);
    c[8]=FS_PATH_TYPE_ASCII;
    c[9]=toSize;
    c[10]=(fromSize<<14)|0x402u;
    c[11]=(u32)from;
    c[12]=(toSize<<14)|0x802u;
    c[13]=(u32)to;
    result=fsSync(*FSUSER_HandleSlot);
    return result?result:(s32)c[1];
}

s32 openFile(const char *path,u32 pathSize,u32 flags,u32 *handle)
{
    return FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,emptyPath,1,FS_PATH_TYPE_ASCII,path,pathSize,flags,0);
}

static s32 setSize(u32 handle,u64 size)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSFILE_CMD_SET_SIZE;
    c[1]=(u32)size;
    c[2]=(u32)(size>>32);
    result=fsSync(handle);
    return result?result:(s32)c[1];
}

static void ensureDirectories(void)
{
    u64 archive=0;
    if (openArchive(&archive)) return;
    createDirectory(archive,directory3ds,sizeof(directory3ds));
    createDirectory(archive,directoryBank,sizeof(directoryBank));
    closeArchive(archive);
}

int resultIsNotFound(s32 result)
{
    return (((u32)result>>RESULT_SUMMARY_SHIFT)&RESULT_SUMMARY_MASK)==
        RESULT_SUMMARY_NOT_FOUND;
}

s32 readCompleteFile(const char *path,u32 pathSize,u32 expectedFileSize,u64 offset,
    void *data,u32 dataSize)
{
    u32 handle=0,bytesRead=0;
    u64 fileSize=0;
    s32 result=openFile(path,pathSize,OPEN_READ,&handle);
    s32 closeResult=0;

    if (!result) result=FSFILE_GetSize(&handle,&fileSize);
    if (!result && fileSize!=expectedFileSize) result=-1;
    if (!result) result=FSFILE_Read(&handle,&bytesRead,offset,data,dataSize);
    if (!result && bytesRead!=dataSize) result=-1;
    if (handle) closeResult=closeFile(handle);
    return result?result:closeResult;
}

int writeCompleteFile(const char *path,u32 pathSize,const void *data,u32 dataSize,u32 fileSize)
{
    u32 handle=0,bytesWritten=0;
    s32 result,closeResult=0;
    ensureDirectories();
    result=openFile(path,pathSize,OPEN_READ|OPEN_WRITE|OPEN_CREATE,&handle);
    if (!result) result=setSize(handle,fileSize);
    if (!result) result=FSFILE_Write(&handle,&bytesWritten,0,data,dataSize,WRITE_FLUSH);
    if (handle) closeResult=closeFile(handle);
    return !result && !closeResult && bytesWritten==dataSize;
}
