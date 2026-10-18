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
/* The only automatic edit to player-supplied screening rules. Missing optional
   input/rule is not created. Turning the permission off never re-enables it.
   唯一自动修改的筛选字段；缺少文件／规则不新建，关闭许可不重新启用规则。 */
int FsCustom_ApplyMover(const FsStorageIo *,uint32_t options,uint32_t source);
#endif
