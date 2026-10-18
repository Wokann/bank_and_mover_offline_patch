#ifndef COMMON_FS_SOURCE_IMAGE_H
#define COMMON_FS_SOURCE_IMAGE_H

#include "fakeServer/fakeserverapi.h"
#include "fakeServer/transaction.h"

enum { FS_LOCAL_SOURCE_SIZE=0x80000, FS_LOCAL_SOURCE_MAGIC=0x4943534C };

/* Local save backend, not a remote endpoint. Its persistent checkpoints reuse
   transaction.bin; its independent RAM never aliases the downloaded Bank or
   an async delivery. All access is serialized by the existing service mutex. */
/* 本地存档后端，不是远端接口。沿用 transaction.bin 的持久化检查点；独立内存
   不借用已下载银行或异步投递缓冲。全部访问由既有服务互斥量串行保护。 */
typedef struct FsLocalSource {
    uint32_t magic,loaded,working,error,written;
    uint8_t *image;
    FsTransactionFingerprint selected,validated;
    FsTransactionRecord identity,reservation;
    FsTransactionView view;
    FsTransactionIo io;
    const FsTransactionIo *coordinator;
    volatile uint32_t busy;
} FsLocalSource;

int FsLocalSource_Initialize(FsLocalSource *,const FsTransactionIo *,void *,uint32_t);
int FsLocalSource_Load(FsLocalSource *,const FsSource *,FsTransactionFingerprint *);
int FsLocalSource_Contains(const FsLocalSource *,const uint16_t *,uint32_t,uint32_t);
int FsLocalSource_Prepare(FsLocalSource *,const FsSource *,const FsTransactionIo *coordinator);
int FsLocalSource_Read(FsLocalSource *,uint32_t,void *,uint32_t);
int FsLocalSource_Write(FsLocalSource *,uint32_t,const void *,uint32_t);
void FsLocalSource_Discard(FsLocalSource *);
int FsLocalSource_Flush(FsLocalSource *,void *,uint32_t,FsTransactionFingerprint *);
int FsLocalSource_Saved(FsLocalSource *,const FsSource *,uint32_t,
    const FsTransactionIo *coordinator,FsTransactionFingerprint *);

#endif
