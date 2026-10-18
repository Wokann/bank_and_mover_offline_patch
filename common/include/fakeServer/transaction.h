#ifndef COMMON_FAKE_SERVER_TRANSACTION_H
#define COMMON_FAKE_SERVER_TRANSACTION_H

#include "types.h"

enum FsTransactionConstant {
    FS_TRANSACTION_MAGIC = 0x3258544C,
    FS_TRANSACTION_VERSION_LEGACY = 4,
    FS_TRANSACTION_VERSION = 5,
    FS_TRANSACTION_BUFFER_SIZE = 0x400,
    FS_TRANSACTION_WORKSPACE_SIZE = 0x100000,
};

enum FsTransactionFile {
    FS_TRANSACTION_BANK,
    FS_TRANSACTION_TEMP,
    FS_TRANSACTION_BACKUP,
    FS_TRANSACTION_JOURNAL,
    FS_TRANSACTION_JOURNAL_TEMP,
    FS_TRANSACTION_TURTLE,
    FS_TRANSACTION_TURTLE_TEMP,
    FS_TRANSACTION_TURTLE_BACKUP,
    FS_TRANSACTION_SOURCE,
    FS_TRANSACTION_SOURCE_TEMP,
    FS_TRANSACTION_SOURCE_BACKUP,
    FS_TRANSACTION_JOURNAL_BACKUP,
    FS_TRANSACTION_FILE_COUNT
};

enum FsTransactionFileResult {
    FS_TRANSACTION_IO_ERROR = -1,
    FS_TRANSACTION_MISSING = 0,
    FS_TRANSACTION_PRESENT = 1
};

enum FsTransactionOperation {
    FS_TRANSACTION_SOURCE_SAVE = 1,
    FS_TRANSACTION_BANK_ONLY,
    FS_TRANSACTION_RESTORE_BACKUP,
    FS_TRANSACTION_SOURCE_ONLY,
    FS_TRANSACTION_SETTINGS_ONLY,
    FS_TRANSACTION_ADOPT_BANK
};

enum FsTransactionParticipant {
    FS_TRANSACTION_HAS_BANK = 1,
    FS_TRANSACTION_HAS_SOURCE = 2,
    FS_TRANSACTION_HAS_TURTLE = 4
};

enum FsTransactionPhase {
    FS_TRANSACTION_PREPARING = 1,
    FS_TRANSACTION_SOURCE_PENDING,
    FS_TRANSACTION_STAGED,
    FS_TRANSACTION_COMMIT_DECIDED,
    FS_TRANSACTION_PUBLISHED,
    FS_TRANSACTION_CLEANUP_PENDING,
    FS_TRANSACTION_CLEANUP_READY,
    FS_TRANSACTION_COMPLETED,
    FS_TRANSACTION_ROLLBACK_DECIDED,
    FS_TRANSACTION_ABORTED,
    FS_TRANSACTION_DETACHED_ROLLBACK_DECIDED,
    FS_TRANSACTION_DETACHED_ROLLED_BACK
};

enum FsTransactionFlag {
    FS_TRANSACTION_SOURCE_ACKNOWLEDGED = 1,
    FS_TRANSACTION_CLEANUP_ACKNOWLEDGED = 2,
    FS_TRANSACTION_TURTLE_WRITE_PENDING = 4,
    FS_TRANSACTION_MANUAL_ROLLBACK = 8,
    FS_TRANSACTION_TURTLE_BASELINE = 16,
    /* Bit 32 in old v4 journals is ignored; it no longer requires any config file. */
    /* 旧 v4 日志的位 32 不再解释为配置绑定，也不要求另一个配置文件。 */
    FS_TRANSACTION_SOURCE_RESTORED = 64
};

typedef struct FsHashState {
    uint32_t crc;
} FsHashState;

/* The journal stores the server descriptor and file publication state.
   Native source matching belongs to the original client, not a save-block hash.
   272 UTF-16 units still fit the fixed save prefix, a 255-unit name and NUL. */
/* 日志保存服务端描述符及文件发布状态；原生来源匹配由原版客户端执行，不增加
   存档事务块摘要。272 个 UTF-16 单元仍容纳固定前缀、255 单元文件名及 NUL。 */
typedef struct FsTransactionRecord {
    uint32_t magic;
    uint32_t version;
    uint32_t recordSize;
    uint32_t sequence;
    uint32_t transaction;
    uint32_t owner;
    uint32_t operation;
    uint32_t phase;
    uint32_t mode;
    uint32_t participants;
    uint32_t flags;
    uint32_t sourceKind;
    uint32_t sourceGameCode;
    uint32_t sourcePathSize;
    uint64_t sourceTitleId;
    uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY];
    FsTransactionParameters serverTransaction;
    FsSourceMetadata metadata;
    FsTransactionFingerprint bankBefore;
    FsTransactionFingerprint bankAfter;
    FsTransactionFingerprint sourceBefore;
    FsTransactionFingerprint sourceAfter;
    FsTransactionFingerprint sourceClean;
    FsTransactionFingerprint turtleBefore;
    FsTransactionFingerprint turtleCurrent;
    FsTransactionFingerprint turtleAfter;
    uint8_t recordHash[FS_TRANSACTION_HASH_SIZE];
} FsTransactionRecord;

/* Volatile loaded-file view, never journal authority. Only Bank/NDS bodies
   are cached; journals, settings and configuration remain independently read. */
/* 内存中的已载入文件视图，不是持久化事务权威；仅缓存银行／NDS 正文，
   日志、设置和配置仍独立读取。重新载入及任何不确定的 I/O 使视图失效。 */
typedef struct FsTransactionView {
    FsTransactionFingerprint files[FS_TRANSACTION_FILE_COUNT];
    uint32_t known,bankStructured,bodyValid;
    FsTransactionFingerprint body;
    uint32_t bodyFile,sourceGameCode,sourcePathSize;
    uint16_t sourcePath[FS_TRANSACTION_PATH_CAPACITY];
} FsTransactionView;

/* The server owns the journal and all commit/recovery decisions. These
   callbacks perform only raw file operations; failure may have side effects. */
/* 服务器拥有事务记录及提交/恢复决策；回调只做底层文件操作，失败仍可能产生副作用。 */
typedef struct FsTransactionIo {
    int (*openRead)(uint32_t file,const FsTransactionRecord *,uint32_t *handle,uint64_t *size);
    int (*openWrite)(uint32_t file,const FsTransactionRecord *,uint32_t *handle);
    int (*setSize)(uint32_t handle,uint64_t size);
    int (*read)(uint32_t handle,uint64_t offset,void *data,uint32_t size);
    int (*write)(uint32_t handle,uint64_t offset,const void *data,uint32_t size);
    int (*close)(uint32_t handle);
    int (*remove)(uint32_t file,const FsTransactionRecord *);
    int (*rename)(uint32_t from,uint32_t to,const FsTransactionRecord *);
    int (*freeBytes)(uint64_t *bytes);
    int (*randomBytes)(void *data,uint32_t size);
    /* Optional service-owned workspace; concurrent/reentrant readers fall
       back to the small stack buffer rather than sharing mutable bytes. */
    /* 可选的服务端缓冲；并发／重入读取回退小栈缓冲，不共享可变正文。 */
    uint8_t *buffer;
    uint32_t bufferSize;
    volatile uint32_t *bufferBusy;
    /* Optional loaded view requires serialized ownership (the native service
       worker already supplies it). Concurrent raw callers leave this NULL. */
    /* 可选载入视图要求串行使用，原生服务工作线程已提供该条件；并发底层调用方
       留空，不把缓冲占用标记误当作整个事务视图的线程锁。 */
    FsTransactionView *view;
    /* Local Gen5 image backend stages the final complete image at SavePoll,
       not an unchanged temporary before every native segmented save. */
    /* Gen5 本地镜像后端在 SavePoll 写齐最终镜像，不预先复制未修改的暂存。 */
    uint32_t deferSourceImage;
} FsTransactionIo;

void FsTransaction_ViewClear(const FsTransactionIo *);
/* x.sav -> x.bak / x.tmp, with a caller-owned UTF-16 buffer. NULL validates.
   Sibling work files survive interruptions, not normal completion. */
/* x.sav 替换扩展名为 x.bak / x.tmp；UTF-16 缓冲由调用方持有，NULL 仅验证。
   同名工作文件仅为中断恢复保留，正常完成后移除。 */
int FsTransaction_SourcePath(const FsTransactionRecord *,uint32_t,uint16_t *);
void FsTransaction_ViewForget(const FsTransactionIo *,uint32_t);
int FsTransaction_ViewGet(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    FsTransactionFingerprint *);
void FsTransaction_ViewPut(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    const FsTransactionFingerprint *);
int FsTransaction_ViewBody(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    const FsTransactionFingerprint *);
void FsTransaction_ViewRememberBody(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    const FsTransactionFingerprint *,const void *,uint32_t);
int FsTransaction_OpenWrite(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,uint32_t *);
int FsTransaction_Remove(const FsTransactionIo *,uint32_t,const FsTransactionRecord *);
int FsTransaction_Rename(const FsTransactionIo *,uint32_t,uint32_t,const FsTransactionRecord *);

int FsTransaction_ReadRecord(const FsTransactionIo *,FsTransactionRecord *);
void FsHash_Initialize(FsHashState *);
void FsHash_Update(FsHashState *,const void *,uint32_t);
void FsHash_Finish(FsHashState *,uint8_t hash[FS_TRANSACTION_HASH_SIZE]);
int FsTransaction_UpdateMetadata(const FsTransactionIo *,uint64_t dataId,
    const FsSourceMetadata *);
int FsTransaction_Observe(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    FsTransactionFingerprint *);
/* Always reads the actual file and inspects the same bytes being hashed, then
   updates the optional volatile loaded view. Observe may reuse that view. */
/* 总是读取实际文件并检查正在计算摘要的同份字节，再更新可选的载入视图。
   Observe 可复用该视图；修改、失败、新载入及重启使之失效。 */
typedef int (*FsTransactionInspect)(void *,uint64_t,uint64_t,const uint8_t *,uint32_t);
int FsTransaction_ObserveChecked(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    FsTransactionFingerprint *,FsTransactionInspect,void *);
int FsTransaction_Matches(const FsTransactionIo *,uint32_t,const FsTransactionRecord *,
    const FsTransactionFingerprint *);
int FsTransaction_Copy(const FsTransactionIo *,uint32_t,uint32_t,const FsTransactionRecord *,
    const FsTransactionFingerprint *);
int FsTransaction_Check(const FsTransactionIo *);
int FsTransaction_Guard(const FsTransactionIo *);
int FsTransaction_GuardBank(const FsTransactionIo *,const FsTransactionFingerprint *);
int FsTransaction_TurtleGuard(const FsTransactionIo *);
/* Recover missing settings only from the published baseline. If all settings
   copies were deleted after settlement, record absence without changing Bank ID. */
/* 设置缺失时仅恢复已发布基线；事务结束且设置副本全部删除时，登记缺失而不改变银行编号。 */
int FsTransaction_ReconcileTurtle(const FsTransactionIo *);
/* No published journal: discard pure settings tmp; for NDS, inspect only the
   selected source's siblings through ReconcileOrphanSource. Never invent a save. */
/* 无正式日志时清理纯设置暂存；NDS 仅由 ReconcileOrphanSource 检查所选来源的
   同名工作文件，不遍历其他存档，也不生造游戏存档。 */
int FsTransaction_ReconcileOrphanParticipants(const FsTransactionIo *);
int FsTransaction_ReconcileOrphanSource(const FsTransactionIo *,const FsTransactionRecord *);
int FsTransaction_Stage(const FsTransactionIo *,uint32_t owner,uint32_t operation,const void *,uint32_t,
    const FsSourceMetadata *);
/* Reserve an unchanged Bank download using the service's freshly read, validated
   body and fingerprint. Do not rotate the previous backup or stage an unchanged
   tmp. Actual save restaging backs up the old main and verifies the complete tmp.
   The buffer must remain unchanged until this call returns. */
/* 用本次读取且验证的正文及摘要预留未修改的银行下载，不轮替旧备份或暂存原样正文。
   实际保存才备份旧主文件、写齐并核对 tmp；调用结束前不得修改缓冲。 */
int FsTransaction_StageSnapshot(const FsTransactionIo *,uint32_t owner,const void *,uint32_t,
    const FsSourceMetadata *,const FsTransactionFingerprint *);
int FsTransaction_UpdateBankReservation(const FsTransactionIo *,uint32_t owner,uint64_t token,
    const void *,uint32_t,const FsSourceMetadata *);
int FsTransaction_StageSource(const FsTransactionIo *,uint32_t owner,uint32_t mode,
    const uint16_t *path,uint32_t pathSize,uint32_t gameCode,const void *bank,uint32_t size,
    const FsTransactionFingerprint *expected,const FsSourceMetadata *);
int FsTransaction_EnsureIdentity(const FsTransactionIo *,uint32_t owner);
/* Only a freshly read/checked bank snapshot may initialize a missing journal. */
/* 仅本次载入并检查的银行快照可用于建立缺失日志，不跨重启保存可信状态。 */
int FsTransaction_EnsureIdentitySnapshot(const FsTransactionIo *,uint32_t owner,
    const FsTransactionFingerprint *);
/* Register the freshly validated main as a new Bank, abandoning an unrelated
   reservation without restoring/rewriting its game save. NULL requires both
   published Bank copies to be missing; with published history, tmp must also be
   missing. Persist the decision before retiring old temporaries. */
/* 将本次验证的主文件登记为新银行，放弃无关旧预留，不恢复或改写游戏存档。
   NULL 要求正式正文及备份缺失；有正式历史时暂存也必须缺失。
   先发布决定，再清理旧暂存及银行备份。 */
int FsTransaction_AdoptBankSnapshot(const FsTransactionIo *,uint32_t owner,
    const FsTransactionFingerprint *);
/* Missing history + missing main only: adopt a freshly checked backup as a new
   Bank, not as proof of an old cross-save commit. The decision precedes copying. */
/* 仅在正式记录及主正文缺失时，将本次校验的备份作为新银行接纳；不证明旧跨存档
   提交成功。先登记决定，再复制／发布正文。 */
int FsTransaction_AdoptBankBackupSnapshot(const FsTransactionIo *,uint32_t owner,
    const FsTransactionFingerprint *);
/* Explicit local policy: absent published history and Bank/bin+bak means a sole
   Bank/journal tmp is abandoned, then the native client creates a new Bank. */
/* 本地明确策略：正式历史及银行 bin/bak 均缺失时，放弃孤立银行／日志 tmp，
   随后由原客户端创建新银行；其他参与文件仍独立保护。 */
int FsTransaction_ResetOrphanBank(const FsTransactionIo *,uint32_t owner);
int FsTransaction_Request(const FsTransactionIo *,uint32_t owner,
    const FsTransactionParameters *,int commit,int force);
int FsTransaction_SourceSaved(const FsTransactionIo *,uint32_t owner);
int FsTransaction_BeginCleanup(const FsTransactionIo *,uint32_t owner);
int FsTransaction_CleanupSaved(const FsTransactionIo *,uint32_t owner);
int FsTransaction_SourceRestored(const FsTransactionIo *,uint32_t owner);
int FsTransaction_Commit(const FsTransactionIo *,uint32_t owner);
int FsTransaction_Finish(const FsTransactionIo *,uint32_t owner);
int FsTransaction_Rollback(const FsTransactionIo *,uint32_t owner);
int FsTransaction_CanRollback(const FsTransactionIo *,const FsTransactionRecord *,int withoutSource);
int FsTransaction_ForceRollback(const FsTransactionIo *,uint32_t owner,int withoutSource);
int FsTransaction_Write(const FsTransactionIo *,uint32_t owner,uint32_t operation,const void *,uint32_t);
int FsTransaction_WriteTurtle(const FsTransactionIo *,uint32_t owner,const void *,uint32_t,int activeSave);
int FsTransaction_SourceIsWorking(const FsTransactionRecord *);

#endif
