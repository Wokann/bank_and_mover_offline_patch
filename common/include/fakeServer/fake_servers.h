#ifndef COMMON_FAKE_SERVERS_H
#define COMMON_FAKE_SERVERS_H

#include "fakeserverapi.h"
#include "configuration.h"
#include "transaction.h"

enum FsBankConstant {
    FS_BANK_SIZE=0xBB518, FS_BANK_FORMAT_VERSION=2, FS_BANK_BOX_COUNT=100,
    FS_BANK_GROUP_COUNT=10, FS_BANK_SLOT_DATA_SIZE=0x1B30, FS_BANK_BOX_SIZE=0x1B56
};
typedef struct FsBankHeader {
    uint8_t reserved00[0x15C];
    uint16_t formatVersion;
    uint16_t boxCount;
    uint8_t reserved160[0x1C];
} FsBankHeader;

typedef struct FsBankBoxMetadata {
    uint16_t name[17];
    uint8_t wallpaper;
    uint8_t group;
    uint16_t order;
} FsBankBoxMetadata;

/* Service-private state, never interpreted by the application hooks. */
/* 服务内部状态，应用钩子不解释这些字段。 */
struct FsServer {
    const FsStorageIo *storage;
    const FsTransactionIo *transactions;
    uint32_t owner;
    uint32_t ready;
    uint32_t ticketReload;
    uint32_t ticketSnapshotReady;
    FsConfiguration configuration;
    FsTicketResponse ticket;
    int32_t configurationStatus;
    FsCampaignRecord campaign;
    uint64_t bankDownloadToken;
    FsSupportResponse rollbackCodes;
    FsTransactionParameters rollbackDescriptor;
    uint32_t rollbackTransaction;
    uint32_t rollbackAuthorized;
    FsTransactionIo transactionIo;
    volatile uint32_t bufferBusy;
    uint8_t ioBuffer[FS_TRANSACTION_WORKSPACE_SIZE];
    uint32_t shopMarket,shopRegion,shopCountry;
    uint32_t ticketQuery;
    FsTransactionView loaded;
    uint32_t bankDelivered;
};

int FsServer_Open(FsServer *,uint32_t owner,const FsStorageIo *,const FsTransactionIo *,
    FsConfigError *);
void FsServer_UserLocation(const FsServer *,uint32_t *region,uint32_t *country);
#endif
