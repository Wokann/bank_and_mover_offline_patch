#ifndef COMMON_FAKE_SERVER_CONFIGURATION_H
#define COMMON_FAKE_SERVER_CONFIGURATION_H

#include "fakeserverapi.h"
#include "ticket.h"
#include "gifts.h"
#include "console_profile.h"

enum FsConfigConstant {
    FS_CONFIG_LINE_SIZE = 192,
    FS_CONFIG_FILE_LIMIT = 32768
};

typedef struct FsConfiguration {
    FsTicketPolicy ticket;
    FsMoverPolicy mover;
    FsGift gifts[FS_GIFT_LIMIT];
    uint32_t giftCount;
    FsGiftWindow giftWindows[FS_GIFT_WINDOW_LIMIT];
    uint32_t giftWindowCount;
    FsShopPolicy shop;
    FsConfigError ticketError;
    FsConfigError moverError;
    FsConfigError giftError;
    FsTicketSettings ticketSettings;
    FsConsoleProfile console;
} FsConfiguration;

/* Initialize to zero before first use. Loads/validation replace prior RAM
   state; release it before discarding the workspace or disconnecting. */
/* 首次使用须清零；加载／验证替换旧内存状态，丢弃工作区或断开前须释放。 */
void FsConfiguration_Release(const FsStorageIo *,FsConfiguration *);
int FsConfiguration_Load(const FsStorageIo *,FsConfiguration *,FsConfigError *);
/* Runtime ticket/gift loads must not retain Mover's dynamic rule/word pools
   in the same native heap needed by SDK response objects. */
/* 票务／礼物常规加载不常驻 Mover 动态词池和规则，避免挤占原 SDK 响应对象所用堆。 */
int FsConfiguration_LoadService(const FsStorageIo *,FsConfiguration *,FsConfigError *);
int FsConfiguration_LoadMover(const FsStorageIo *,FsConfiguration *,uint32_t source,FsConfigError *);
int FsConfiguration_Purchase(const FsStorageIo *,FsConfiguration *,
    const FsTicketPolicy *,uint32_t market,uint32_t balance,uint32_t paidField,FsConfigError *);
int FsConfiguration_ValidateFile(const FsStorageIo *,FsConfiguration *,uint32_t section,
    const char *path,FsConfigError *);
/* Gift/name inputs have no staging path. Check tables expose staging only
   for the explicitly authorized BanUnicodeChinese update, not generic Edit. */
/* 礼物／姓名表无暂存路径；检查表仅为指定规则字段写回提供暂存，通用 Edit 仍拒绝。 */
const char *FsConfiguration_Path(uint32_t section,int temporary);
int FsConfiguration_Edit(const FsStorageIo *,FsConfiguration *workspace,uint32_t section,
    const FsConfigEdit *,uint32_t count,FsConfigError *);
const char *FsConfiguration_TicketDefault(const char *key);
const char *FsConfiguration_TicketKey(uint32_t field);
uint32_t FsConfiguration_TicketOrder(uint32_t position);
const char *FsConfiguration_TicketValue(uint32_t field);
int FsConfiguration_TicketField(const char *key);
int FsConfiguration_ParseTicket(FsConfiguration *,uint32_t field,const char *value);
int FsConfiguration_RepairTicket(FsConfiguration *,uint32_t field,const char *value);
int FsConfiguration_TicketRequired(uint32_t field);
int FsConfiguration_TicketLegacyField(const char *key);
int FsConfiguration_RecordPurchase(FsConfiguration *,uint32_t field,const FsTicketPolicy *);
void FsConfiguration_CompleteTicket(FsConfiguration *);
int FsConfiguration_FormatTicket(const FsConfiguration *,uint32_t field,char *value);
uint32_t *FsConfiguration_ShopValue(FsConfiguration *,uint32_t market,uint32_t kind);
int FsConfiguration_VerifyTemporary(const FsStorageIo *,const char *path);
#endif
