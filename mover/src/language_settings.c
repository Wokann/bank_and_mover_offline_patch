#include <stddef.h>
#include "language_settings.h"
#include "fs_helpers.h"
#include "patch_paths.h"

static int languageIsSupported(u32 languageId)
{
    return languageId!=0 && languageId<=BANK_LANGUAGE_ID_MAX &&
        languageId!=BANK_LANGUAGE_ID_UNUSED;
}

/* Let the native startup apply both the message archive and the font before
   constructing its views. Missing/unreadable settings keep the native fallback. */
/* 在原版启动流程创建界面前，让其统一应用消息资源与字库。设置缺失或不可读时
   保持原来的回退逻辑。 */
u32 LanguageSettings_Load(MoverGameDataLanguageView *gameData)
{
    BankLanguageSettings settings;

    if (readCompleteFile(turtleSavePath,sizeof(turtleSavePath),
        BANK_LOCAL_RECORD_FILE_SIZE,offsetof(BankLocalSettingsFileView,language),
        &settings,sizeof(settings))>=0 && languageIsSupported(settings.languageId)) {
        gameData->languageId=settings.languageId;
        gameData->useJapaneseKanji=settings.useJapaneseKanji!=0;
    }
    return gameData->languageId;
}

/* Only a confirmed native selection writes back, and only to an existing
   complete Bank record. Stage and close the whole file before replacing it;
   leave a completed tmp available to Bank's recovery if the rename fails. */
/* 仅在原版确认语言选择后写回已有的完整 Bank 记录。先暂存并关闭整个文件，再
   替换原文件；重命名失败时保留已完成的 tmp，供 Bank 恢复。 */
void LanguageSettings_StoreSelection(void *selection,u32 languageId,u32 useJapaneseKanji)
{
    BankLocalSettingsFileView record;
    u64 archive;
    s32 result;

    LanguageSelection_SetSettings(selection,languageId,(u8)useJapaneseKanji);
    if (!languageIsSupported(languageId) ||
        readCompleteFile(turtleSavePath,sizeof(turtleSavePath),
            sizeof(record),0,&record,sizeof(record))<0) return;
    useJapaneseKanji=useJapaneseKanji!=0;
    if (record.language.languageId==languageId &&
        record.language.useJapaneseKanji==useJapaneseKanji) return;
    record.language.languageId=(u16)languageId;
    record.language.useJapaneseKanji=(u16)useJapaneseKanji;
    if (openArchive(&archive)<0) return;
    result=pathCommand(FSUSER_CMD_DELETE_FILE,archive,turtleTempPath,sizeof(turtleTempPath));
    if ((result>=0 || resultIsNotFound(result)) &&
        writeCompleteFile(turtleTempPath,sizeof(turtleTempPath),
            &record,sizeof(record),sizeof(record)) &&
        pathCommand(FSUSER_CMD_DELETE_FILE,archive,turtleSavePath,sizeof(turtleSavePath))>=0)
        renamePath(archive,turtleTempPath,sizeof(turtleTempPath),
            turtleSavePath,sizeof(turtleSavePath));
    closeArchive(archive);
}
