#ifndef MOVER_OFFLINE_PATCH_CODE_EXPANSION_H
#define MOVER_OFFLINE_PATCH_CODE_EXPANSION_H

#include "patch_types.h"

enum CodeExpansionConstant {
    CODE_EXPANSION_PROCESS_HANDLE = 0xFFFF8001u,
    CODE_EXPANSION_EMULATOR_INFO = 0x20000,
    CODE_EXPANSION_AZAHAR_ID = 2,
    CODE_EXPANSION_PROTECT = 6,
    CODE_EXPANSION_READ_WRITE_EXECUTE = 7
};

/* Enable the added pages; only identified Azahar may retain an unhandled SVC. */
/* 启用新增页；仅已识别的 Azahar 可兼容未处理的 SVC。 */
s32 CodeExpansion_Enable(u32 address,u32 size);

#endif
