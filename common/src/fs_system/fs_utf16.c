#include "fs_system/fs_utf16.h"
#include "fs_system/patch_paths.h"

/* UTF-16 directory enumeration and ranged SD file operations. */
/* UTF-16 目录枚举及 SD 文件分段读写。 */
s32 openFileUtf16(const u16 *path,u32 pathSize,u32 flags,u32 *handle)
{
    return FSUSER_OpenFileDirectly(FSUSER_HandleSlot,handle,0,FS_ARCHIVE_ID_SDMC,
        FS_PATH_TYPE_EMPTY,emptyPath,1,FS_PATH_TYPE_UTF16,path,pathSize,flags,0);
}

s32 openDirectoryUtf16(const u16 *path,u32 pathSize,u64 *archive,u32 *handle)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result=openArchive(archive);
    if (result) return result;
    c[0]=FSUSER_CMD_OPEN_DIRECTORY;
    c[1]=(u32)*archive;
    c[2]=(u32)(*archive>>32);
    c[3]=FS_PATH_TYPE_UTF16;
    c[4]=pathSize;
    c[5]=(pathSize<<14)|2u;
    c[6]=(u32)path;
    result=fsSync(*FSUSER_HandleSlot);
    if (!result) result=(s32)c[1];
    if (!result) *handle=c[3];
    return result;
}

s32 readDirectoryEntry(u32 handle,FsDirectoryEntry *entry,u32 *entriesRead)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result;
    c[0]=FSDIR_CMD_READ;
    c[1]=1;
    c[2]=(sizeof(*entry)<<4)|0x0Cu;
    c[3]=(u32)entry;
    result=fsSync(handle);
    if (!result) result=(s32)c[1];
    if (!result) *entriesRead=c[2];
    return result;
}

s32 closeDirectory(u32 handle)
{
    volatile u32 *c=fsCommandBuffer();
    s32 result,closeResult;
    if (!handle) return 0;
    c[0]=FSDIR_CMD_CLOSE;
    result=fsSync(handle);
    if (!result) result=(s32)c[1];
    closeResult=fsCloseHandle(handle);
    return result?result:closeResult;
}

s32 readFileRangeUtf16(const u16 *path,u32 pathSize,u64 offset,void *data,u32 dataSize)
{
    u32 handle=0,bytesRead=0;
    s32 result=openFileUtf16(path,pathSize,OPEN_READ,&handle);
    s32 closeResult=0;
    if (!result) result=FSFILE_Read(&handle,&bytesRead,offset,data,dataSize);
    if (!result && bytesRead!=dataSize) result=-1;
    if (handle) closeResult=closeFile(handle);
    return result?result:closeResult;
}

int fileIsReadWriteUtf16(const u16 *path,u32 pathSize)
{
    u32 handle=0;
    s32 result=openFileUtf16(path,pathSize,OPEN_READ|OPEN_WRITE,&handle);
    if (handle) {
        s32 closeResult=closeFile(handle);
        if (!result) result=closeResult;
    }
    return result==0;
}
