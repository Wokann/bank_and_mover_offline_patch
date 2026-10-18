#ifndef COMMON_FAKE_SERVER_TYPES_H
#define COMMON_FAKE_SERVER_TYPES_H

#include <stdint.h>

enum FsResult {
    FS_OK = 0, FS_MISSING = 1, FS_NO_GIFT = 2, FS_ALREADY_CLAIMED = 3,
    FS_BOX_OCCUPIED = 4,
    FS_IO_ERROR = -1, FS_INVALID = -2, FS_LOCKED = -3,
    FS_UNSUPPORTED = -4, FS_LIMIT = -5,
    /* Business replies, distinct from an unreadable local journal/file. */
    /* 业务响应与本地日志／文件不可读分开，不能把 I/O 错误当作过期事务。 */
    FS_TRANSACTION_MISMATCH = -6, FS_REMOTE_NOT_FOUND = -7,
    FS_INSUFFICIENT_FUNDS = -8, FS_NO_MEMORY = -9
};

enum FsSourceConstant {
    /* CRC-32/ISO-HDLC, four bytes in most-significant-byte-first order. */
    /* CRC-32/ISO-HDLC，四字节按高位在前存放；不是密码学认证。 */
    FS_TRANSACTION_HASH_SIZE=4,
    FS_TRANSACTION_PATH_CAPACITY=0x110,
    FS_TRANSACTION_OWNER_BANK=1,
    FS_TRANSACTION_OWNER_MOVER=2
};
enum FsTransactionSourceKind {
    FS_TRANSACTION_NO_SOURCE,
    FS_TRANSACTION_NATIVE_SOURCE,
    FS_TRANSACTION_NDS_FILE
};
/* An observed file identity is an API input; journal layout stays private. */
/* 文件身份摘要是 API 输入，日志布局仅在服务内部使用。 */
typedef struct FsTransactionFingerprint {
    uint32_t present;
    uint32_t reserved;
    uint64_t size;
    uint8_t hash[FS_TRANSACTION_HASH_SIZE];
    /* Make ABI tail padding explicit: bytewise snapshots must be deterministic. */
    /* 显式保留 ABI 尾部，避免整块比较受到未初始化填充字节影响。 */
    uint32_t reserved14;
} FsTransactionFingerprint;

/* Original game-save ABI. Matching and state 1/2 decisions stay in the client. */
/* 原版游戏存档 ABI；匹配及状态 1/2 的分支判断仍由原版客户端执行。 */
typedef struct FsNativeTransaction {
    uint64_t dataId;
    uint64_t token;
    uint32_t currentVersion;
    uint32_t updateVersion;
    uint32_t size;
    uint8_t state;
    uint8_t reserved1D[3];
} FsNativeTransaction;

/* Native staging reply: token follows the version/size fields, unlike the
   transaction view inside a game save. Do not copy one layout into the other. */
/* 原版暂存响应的令牌位于版本／大小之后，与游戏存档事务视图不同，不能直接整块互拷。 */
typedef struct FsTransactionParameters {
    uint64_t dataId;
    uint32_t currentVersion;
    uint32_t updateVersion;
    uint32_t size;
    uint32_t reserved14;
    uint64_t token;
} FsTransactionParameters;

/* Native recovery display metadata, not an additional source-authentication key. */
/* 原版恢复界面的显示元数据，不作为额外的来源认证条件。 */
typedef struct FsSourceMetadata {
    uint8_t formatVersion;
    uint8_t nameFormat;
    uint16_t trainerName[13];
    uint32_t trainerId;
    uint8_t gameId;
    uint8_t reserved21[3];
} FsSourceMetadata;

void FsTransaction_Hash(const void *,uint32_t,uint8_t hash[FS_TRANSACTION_HASH_SIZE]);

/* Raw storage only. The library decides what to keep, publish or roll back.
   openRead: 1 present, 0 missing, -1 error. Other callbacks return boolean.
   write must flush; openWrite creates/truncates to the requested length. */
/* 仅底层存储操作。保留、发布和回滚由库决定。openRead 返回存在 1/缺失 0/错误 -1，
   其余回调为布尔值；写入须刷新，openWrite 按指定长度创建/截断文件。 */
typedef struct FsStorageIo {
    int (*openRead)(const char *path,uint32_t *handle,uint32_t *size);
    int (*openWrite)(const char *path,uint32_t size,uint32_t *handle);
    int (*read)(uint32_t handle,uint32_t offset,void *data,uint32_t size);
    int (*write)(uint32_t handle,uint32_t offset,const void *data,uint32_t size);
    int (*close)(uint32_t handle);
    int (*remove)(const char *path);
    int (*rename)(const char *from,const char *to);
    /* Transient server allocations, never serialized. The adapter supplies a
       paired allocator; failure returns NULL instead of terminating the game. */
    /* 服务端临时内存，不落盘；适配层提供成对分配／释放，失败返回空指针。 */
    void *(*allocate)(uint32_t size);
    void (*release)(void *);
    /* Native SD policy is split into shared/client/user documents. */
    uint32_t capabilities;
} FsStorageIo;
enum { FS_STORAGE_ACCOUNT_LAYOUT=1u, FS_STORAGE_READ_ONLY=2u };

#endif
