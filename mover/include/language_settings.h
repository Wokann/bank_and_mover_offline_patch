#ifndef MOVER_OFFLINE_PATCH_LANGUAGE_SETTINGS_H
#define MOVER_OFFLINE_PATCH_LANGUAGE_SETTINGS_H

#include "patch_types.h"

enum BankLanguageSettingsConstant {
    BANK_LOCAL_RECORD_FILE_SIZE = 0x200,
    BANK_LANGUAGE_ID_MAX = 10,
    BANK_LANGUAGE_ID_UNUSED = 6
};

typedef struct BankLanguageSettings {
    u16 languageId;
    u16 useJapaneseKanji;
} BankLanguageSettings;

/* Language settings are outside the Turtle transaction checksum. Keep every
   other serialized byte, including the file's unused tail, when updating them. */
/* 语言设置不在 Turtle 事务校验范围内。修改时保留其他所有序列化字节，包含
   文件末尾的未使用区域。 */
typedef struct BankLocalSettingsFileView {
    u8 reserved00[0x28];
    BankLanguageSettings language;
    u8 reserved2C[0x1D4];
} BankLocalSettingsFileView;

typedef struct MoverGameDataLanguageView {
    u8 reserved00[0xD0];
    u32 languageId;
    u8 useJapaneseKanji;
} MoverGameDataLanguageView;

typedef void (*LanguageSelection_SetSettingsFn)(void *,u32,u8);
#define LanguageSelection_SetSettings ((LanguageSelection_SetSettingsFn)0x0022C06Cu)

u32 LanguageSettings_Load(MoverGameDataLanguageView *gameData);
void LanguageSettings_StoreSelection(void *selection,u32 languageId,u32 useJapaneseKanji);

#endif
