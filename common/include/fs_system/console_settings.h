#ifndef COMMON_CONSOLE_SETTINGS_H
#define COMMON_CONSOLE_SETTINGS_H

#include "patch_types.h"
#include "fakeServer/console_profile.h"

/* Read only public CFG region/country settings; no serial/account information. */
/* 仅读取 CFG 的地区和国家设置，不读取序列号或账户信息。 */
void ConsoleSettings_GetLocation(u32 *region,u32 *country);
void ConsoleSettings_GetProfile(FsConsoleProfile *);
/* CFG profile name, not NNID, PNID, Mii nickname or a network request. */
int ConsoleSettings_GetUserName(char *utf8,u32 capacity);

#endif
