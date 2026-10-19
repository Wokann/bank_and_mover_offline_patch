#ifndef COMMON_FAKE_SERVER_SD_ADAPTER_H
#define COMMON_FAKE_SERVER_SD_ADAPTER_H

#include "types.h"
#include "transaction.h"
#include "user_profiles.h"

/* Optional 3DS backend. Compile with the calling title's SDK declarations;
   neither table performs server policy, commit or recovery. */
/* 可选 3DS 后端，使用调用方的 SDK 声明；两张表均不执行策略、提交或恢复决策。 */
/* Capabilities include the selected session's read-only bootstrap state. */
/* 能力标记包含当前会话尚未初始化时的只读状态。 */
extern FsStorageIo FsSdStorage;
extern const FsTransactionIo FsSdTransactions;
int FsSd_SelectUser(void);
/* Shared configuration only; safe before selecting/provisioning any account. */
int FsSd_ReadLogSetting(uint32_t *);
void FsSd_ResetUser(void);
int FsSd_UserMenu(uint32_t action,uint32_t activeUser,void *body,uint32_t bodySize,
    FsUserMenu *,uint32_t *outputSize);

#endif
