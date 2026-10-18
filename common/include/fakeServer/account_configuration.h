#ifndef COMMON_ACCOUNT_CONFIGURATION_H
#define COMMON_ACCOUNT_CONFIGURATION_H
#include "configuration.h"
int FsAccountConfiguration_Load(const FsStorageIo *,FsConfiguration *,FsConfigError *);
int FsAccountConfiguration_Edit(const FsStorageIo *,FsConfiguration *,const FsConfigEdit *,uint32_t,FsConfigError *);
int FsAccountConfiguration_Purchase(const FsStorageIo *,FsConfiguration *,const FsTicketPolicy *,uint32_t,uint32_t,uint32_t,FsConfigError *);
#endif
