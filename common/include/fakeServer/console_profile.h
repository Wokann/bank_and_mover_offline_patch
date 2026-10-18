#ifndef COMMON_CONSOLE_PROFILE_H
#define COMMON_CONSOLE_PROFILE_H

#include "types.h"

enum { FS_CONSOLE_REGION,FS_CONSOLE_LANGUAGE,FS_CONSOLE_COUNTRY,FS_CONSOLE_STATE,
    FS_CONSOLE_FIELDS,FS_CONSOLE_UNKNOWN=0xffffffffu };
/* Raw CFG numbers, not Pokemon language IDs or geographic-history fields. */
/* CFG 原始编号，不是宝可梦语言编号或经历地区。 */
typedef struct FsConsoleProfile {
    uint32_t values[FS_CONSOLE_FIELDS];
    uint32_t autoRegion,ready;
} FsConsoleProfile;

/* One application-visible CFG snapshot per process; no SD I/O here. */
void FsConsole_SetHost(const FsConsoleProfile *);
/* Adapter-owned per-user boot state: selecting another user never consumes it. */
void FsConsole_SelectStartup(uint32_t *);
void FsConsole_RefreshStartup(FsConsoleProfile *);
void FsConsole_CommitStartup(void);
void FsConsole_Default(FsConsoleProfile *);
void FsConsole_Resolve(const FsConsoleProfile *,FsConsoleProfile *);
const char *FsConsole_Key(uint32_t);
int FsConsole_Parse(FsConsoleProfile *,uint32_t,const char *);
uint32_t FsConsole_CountryCode(uint32_t);
void FsConsole_Location(const FsConsoleProfile *,uint32_t *,uint32_t *);

#endif
