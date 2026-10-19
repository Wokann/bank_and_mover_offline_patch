#ifndef COMMON_FAKE_SERVER_CUSTOM_SETTINGS_H
#define COMMON_FAKE_SERVER_CUSTOM_SETTINGS_H
#include "types.h"

enum FsCustomOption {
    FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN=4,
    FS_CUSTOM_MOVER_V12=8,
    FS_CUSTOM_COUNT=2
};
const char *FsCustom_Key(uint32_t index);
const char *FsCustom_Value(uint32_t options,uint32_t index);
int FsCustom_Parse(uint32_t *options,const char *key,const char *value);
struct FsMoverPolicy;
/* Override only this user's loaded rules; shared files are always read-only.
   仅覆盖当前用户已载入的内存规则，共享文件始终只读。 */
void FsCustom_ApplyMover(struct FsMoverPolicy *);
#endif
