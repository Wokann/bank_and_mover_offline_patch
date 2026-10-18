#ifndef COMMON_FS_SYSTEM_SETTINGS_STORE_H
#define COMMON_FS_SYSTEM_SETTINGS_STORE_H

#include "fakeServer/types.h"

/* Validate the native 0x28-byte prefix, without changing its transaction fields. */
/* 仅校验原生 0x28 字节前缀，不修改其中的事务字段。 */
int FsSettings_ValidPrefix(const uint8_t *record);

#endif
