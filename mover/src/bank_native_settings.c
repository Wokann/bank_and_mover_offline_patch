#include "fs_system/bank_native_settings.h"
#include "fs_system/fs_helpers.h"
#include "fs_system/settings_store.h"
#include "fakeServer/types.h"

enum { BANK_SAVE_ARCHIVE=0x567890B4u, BANK_SAVE_UNIQUE_ID=0xC9Bu };
static const u32 bankArchivePath[3]={1,BANK_SAVE_UNIQUE_ID,0};
static const char bankRecordPath[]="/turtle";

static int storageError(s32 result)
{
    return resultIsNotFound(result)?FS_MISSING:FS_IO_ERROR;
}

static int openBank(u64 *archive)
{
    volatile u32 *c=fsCommandBuffer();
    c[0]=FSUSER_CMD_OPEN_ARCHIVE;c[1]=BANK_SAVE_ARCHIVE;c[2]=2;c[3]=sizeof(bankArchivePath);
    c[4]=(sizeof(bankArchivePath)<<14)|2u;c[5]=(u32)bankArchivePath;
    s32 result=fsSync(*FSUSER_HandleSlot);
    if (!result) result=(s32)c[1];
    if (result) return storageError(result);
    *archive=(u64)c[2]|((u64)c[3]<<32);
    return FS_OK;
}

static int openRecord(u64 archive,u32 flags,u32 *handle)
{
    volatile u32 *c=fsCommandBuffer();
    c[0]=0x080201C2u;c[1]=0;c[2]=(u32)archive;c[3]=(u32)(archive>>32);
    c[4]=FS_PATH_TYPE_ASCII;c[5]=sizeof(bankRecordPath);c[6]=flags;c[7]=0;
    c[8]=(sizeof(bankRecordPath)<<14)|2u;c[9]=(u32)bankRecordPath;
    s32 result=fsSync(*FSUSER_HandleSlot);
    if (!result) result=(s32)c[1];
    if (result) return storageError(result);
    *handle=c[3];
    return *handle?FS_OK:FS_IO_ERROR;
}

static int commitBank(u64 archive)
{
    volatile u32 *c=fsCommandBuffer();
    c[0]=0x080D0144u;c[1]=(u32)archive;c[2]=(u32)(archive>>32);c[3]=0;
    c[4]=0;c[5]=0;c[6]=0xAu;c[7]=0;c[8]=0xCu;c[9]=0;
    s32 result=fsSync(*FSUSER_HandleSlot);
    return !result && !(s32)c[1]?FS_OK:FS_IO_ERROR;
}

static int readRecord(u32 handle,u8 record[512])
{
    u64 size=0;u32 read=0;
    if (FSFILE_GetSize(&handle,&size)) return FS_IO_ERROR;
    if (size!=512) return FS_INVALID;
    if (FSFILE_Read(&handle,&read,0,record,512) || read!=512) return FS_IO_ERROR;
    return FsSettings_ValidPrefix(record)?FS_OK:FS_INVALID;
}

int FsBankNative_ReadSettings(u8 record[512])
{
    if (!record) return FS_INVALID;
    u64 archive=0;u32 handle=0;
    int result=openBank(&archive);
    if (result!=FS_OK) return result;
    result=openRecord(archive,OPEN_READ,&handle);
    if (result==FS_OK) result=readRecord(handle,record);
    if (handle && closeFile(handle)) result=FS_IO_ERROR;
    if (closeArchive(archive)) result=FS_IO_ERROR;
    return result;
}

int FsBankNative_UpdateLanguage(uint32_t language,uint32_t kanji,u8 record[512])
{
    if (!record || !language || language>10 || language==6 || kanji>1) return FS_INVALID;
    u64 archive=0;u32 handle=0;
    int result=openBank(&archive);
    if (result!=FS_OK) return result;
    result=openRecord(archive,OPEN_READ|OPEN_WRITE,&handle);
    if (result==FS_OK) result=readRecord(handle,record);
    int changed=result==FS_OK && (record[0x28]!=language || record[0x29] ||
        record[0x2a]!=kanji || record[0x2b]);
    if (changed) {
        record[0x28]=(u8)language;record[0x29]=0;record[0x2a]=(u8)kanji;record[0x2b]=0;
        u32 written=0;
        if (FSFILE_Write(&handle,&written,0x28,record+0x28,4,WRITE_FLUSH) || written!=4)
            result=FS_IO_ERROR;
    }
    if (handle && closeFile(handle)) result=FS_IO_ERROR;
    if (result==FS_OK && changed) result=commitBank(archive);
    if (closeArchive(archive)) result=FS_IO_ERROR;
    if (result!=FS_OK || !changed) return result;
    /* Reopen the committed archive, not just the old file handle's cache.
       Compare all bytes so no unexpected transaction/tail change is approved. */
    /* 重新打开已提交档案，不只核对旧文件句柄缓存；比较全部字节，防止放行非语言字段变化。 */
    u8 verified[512];
    result=FsBankNative_ReadSettings(verified);
    if (result==FS_OK)
        for (u32 i=0;i<512;i++) if (record[i]!=verified[i]) return FS_INVALID;
    return result;
}
