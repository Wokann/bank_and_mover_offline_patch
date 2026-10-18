#ifndef COMMON_FS_BANK_NATIVE_SETTINGS_H
#define COMMON_FS_BANK_NATIVE_SETTINGS_H

#include <stdint.h>

/* Mover-only worker backend for the existing Bank /turtle record. No format,
   creation, identity changes or raw SD/NAND path access is permitted here. */
/* Mover 专用后台后端，只访问现有 Bank /turtle；不格式化、创建或修改身份，
   不通过 SD／NAND 的物理路径绕过主机存档服务。 */
int FsBankNative_ReadSettings(uint8_t record[512]);
int FsBankNative_UpdateLanguage(uint32_t language,uint32_t kanji,uint8_t record[512]);

#endif
