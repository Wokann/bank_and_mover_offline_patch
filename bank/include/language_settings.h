#ifndef BANK_PATCH_LANGUAGE_SETTINGS_H
#define BANK_PATCH_LANGUAGE_SETTINGS_H

#include "turtle_redirect.h"

/* Native language authority. Existing SD records receive only language fields;
   only a missing SD file receives the complete validated native snapshot. */
/* 语言以主机原档为准：已有 SD 仅同步语言，确实缺失才完整复制已校验原档；不按模式分流。 */
int LanguageSettings_StartupSync(uintptr_t peer,TurtleGameDataView *);
int LanguageSettings_SyncPending(uintptr_t peer);
int LanguageSettings_BindRecord(TurtleGameDataView *);
int LanguageSettings_Save(void *state);
void LanguageSettings_Cancel(void *state);

#endif
