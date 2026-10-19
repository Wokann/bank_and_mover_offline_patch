#ifndef COMMON_ACCOUNT_CONFIGURATION_H
#define COMMON_ACCOUNT_CONFIGURATION_H
#include "configuration.h"
enum FsAccountDocument { FS_ACCOUNT_SHARED,FS_ACCOUNT_USER };
/* Rebuild published configuration without provisioning an account. Valid
   locale/currency values survive; damaged fields use the factory template.
   Automatic startup refresh is separate. Missing files stay missing. */
/* 重建既有配置，合法地区／币种迁移，坏项恢复默认；不创建账户或执行自动启动刷新。 */
int FsAccountConfiguration_RepairPublished(const FsStorageIo *,const char *,const char *,
    uint32_t document,const FsConsoleProfile *factory,FsConfigError *);
int FsAccountConfiguration_ReadLogSetting(const FsStorageIo *,const char *,const char *,uint32_t *);
int FsAccountConfiguration_Load(const FsStorageIo *,FsConfiguration *,FsConfigError *);
int FsAccountConfiguration_Edit(const FsStorageIo *,FsConfiguration *,const FsConfigEdit *,uint32_t,FsConfigError *);
int FsAccountConfiguration_Purchase(const FsStorageIo *,FsConfiguration *,const FsTicketPolicy *,uint32_t,uint32_t,uint32_t,FsConfigError *);
#endif
