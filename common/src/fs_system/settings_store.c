#include "fakeServer/fakeserverapi.h"
#include "fakeServer/transaction.h"
#include "fs_system/settings_store.h"
#include <string.h>

static uint16_t settingsChecksum(const uint8_t *record)
{
    uint16_t checksum=0xffff;
    for (uint32_t i=0;i<0x20;i++) {
        checksum^=(uint16_t)(record[i]<<8);
        for (uint32_t bit=0;bit<8;bit++)
            checksum=(uint16_t)((checksum<<1)^((checksum&0x8000)?0x1021:0));
    }
    return checksum;
}

int FsSettings_ValidPrefix(const uint8_t *record)
{
    uint16_t checksum=settingsChecksum(record);
    return record[0x20]==0x44 && record[0x21]==0x45 && record[0x22]==0x45 &&
        record[0x23]==0x46 && record[0x24]==(uint8_t)checksum &&
        record[0x25]==(uint8_t)(checksum>>8) && !record[0x26] && !record[0x27];
}

int FsSettings_UpdateRecord(const FsTransactionIo *io,uint32_t owner,
    const void *data,uint32_t size,int activeSave)
{
    if (!data || size!=0x30 || (activeSave!=0 && activeSave!=1)) return FS_INVALID;
    uint8_t record[512];
    int result=FsSettings_ReadRange(io,0,record,sizeof(record));
    if (result!=FS_OK && result!=FS_MISSING) return result;
    if (result==FS_MISSING) memset(record,0,sizeof(record));
    memcpy(record,data,size);
    return FsTransaction_WriteTurtle(io,owner,record,sizeof(record),activeSave)?FS_OK:FS_IO_ERROR;
}

static int updateLanguage(const FsTransactionIo *io,uint32_t owner,
    uint32_t languageId,uint32_t useJapaneseKanji,const uint8_t *nativeRecord)
{
    if (!languageId || languageId>10 || languageId==6 || useJapaneseKanji>1) return FS_INVALID;
    uint8_t record[512];
    int result=FsSettings_ReadRange(io,0,record,sizeof(record));
    if (result==FS_MISSING) {
        if (!nativeRecord) return FS_OK;
        if (!FsSettings_ValidPrefix(nativeRecord) || nativeRecord[0x28]!=languageId ||
            nativeRecord[0x29] || nativeRecord[0x2a]!=useJapaneseKanji || nativeRecord[0x2b])
            return FS_INVALID;
        /* Only genuine absence authorizes a complete native-record import. */
        /* 仅确实缺失时完整复制原记录；已有文件不复制身份、事务或未知尾部。 */
        return FsTransaction_WriteTurtle(io,owner,nativeRecord,sizeof(record),0)?FS_OK:FS_IO_ERROR;
    }
    if (result!=FS_OK) return result;
    if (!FsSettings_ValidPrefix(record)) return FS_INVALID;
    if (record[0x28]==languageId && !record[0x29] &&
        record[0x2a]==useJapaneseKanji && !record[0x2b]) return FS_OK;
    record[0x28]=(uint8_t)languageId;record[0x29]=0;
    record[0x2a]=(uint8_t)useJapaneseKanji;record[0x2b]=0;
    return FsTransaction_WriteTurtle(io,owner,record,sizeof(record),0)?FS_OK:FS_IO_ERROR;
}

int FsSettings_UpdateLanguage(const FsTransactionIo *io,uint32_t owner,
    uint32_t languageId,uint32_t useJapaneseKanji)
{
    return updateLanguage(io,owner,languageId,useJapaneseKanji,0);
}

int FsSettings_SynchronizeLanguage(const FsTransactionIo *io,uint32_t owner,
    uint32_t languageId,uint32_t useJapaneseKanji,const void *nativeRecord,uint32_t size)
{
    if (!nativeRecord || size!=512) return FS_INVALID;
    return updateLanguage(io,owner,languageId,useJapaneseKanji,nativeRecord);
}
