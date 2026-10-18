#ifndef COMMON_FAKE_SERVER_API_H
#define COMMON_FAKE_SERVER_API_H

#include "types.h"
#include "ticket.h"
#include "gifts.h"
#include "mover_checks.h"

/* Opaque service context. Clients submit native observations, not journal decisions. */
/* 服务上下文不透明；客户端提交原生观察结果，不解释日志或决定文件版本。 */
typedef struct FsServer FsServer;
typedef struct FsTransactionIo FsTransactionIo;
typedef struct FsConfigError {
    uint32_t line;
    int32_t result;
    const char *path;
} FsConfigError;
enum FsConfigSection {
    FS_CONFIG_TICKET = 1,
    FS_CONFIG_MOVER,
    FS_CONFIG_GIFT,
    FS_CONFIG_WORDS,
    FS_CONFIG_MOVER_VC,
    FS_CONFIG_WORDS_VC,
    FS_CONFIG_MOVER_NDS = FS_CONFIG_MOVER,
    FS_CONFIG_WORDS_NDS = FS_CONFIG_WORDS
};
enum FsConfigEditAction { FS_CONFIG_SET, FS_CONFIG_REMOVE_KEY, FS_CONFIG_REMOVE_SECTION };
enum FsConfigEditConstant { FS_CONFIG_EDIT_LIMIT=64, FS_CONFIG_SECTION_SIZE=64 };
typedef struct FsConfigEdit {
    uint32_t action;
    const char *section;
    const char *key;
    const char *value;
} FsConfigEdit;
/* Extension switches require section "global": enableGift / enableCheckPokemon /
   enableCheckName. NDS and VC targets select independent files; no root keys. */
/* 扩展开关必须位于 global 段：enableGift／enableCheckPokemon／enableCheckName；
   NDS 与 VC 目标分别编辑独立文件，不支持无段名键。 */

typedef struct FsSource {
    uint32_t kind;
    uint32_t gameCode;
    const uint16_t *path;
    uint32_t pathSize;
    uint32_t mode;
    const FsTransactionFingerprint *expected;
    const FsSourceMetadata *metadata;
} FsSource;

enum FsTransactionAction {
    FS_TRANSACTION_RECOVER,
    FS_TRANSACTION_SOURCE_SAVED,
    FS_TRANSACTION_COMMIT,
    FS_TRANSACTION_BEGIN_CLEANUP,
    FS_TRANSACTION_CLEANUP_SAVED,
    FS_TRANSACTION_FINISH,
    FS_TRANSACTION_ROLLBACK,
    FS_TRANSACTION_FORCE_ROLLBACK,
    FS_TRANSACTION_FORCE_BANK_ONLY_ROLLBACK
};
enum FsSlotResult { FS_SLOT_ACCEPTED=0, FS_SLOT_REJECTED=1, FS_SLOT_EMPTY=20 };
typedef struct FsMoverCandidate {
    uint16_t species;
    uint8_t structurallyValid;
    uint8_t excluded;
} FsMoverCandidate;
typedef int (*FsMoverExtraCheck)(const FsMoverCandidate *,uint32_t slot,void *user);
typedef int (*FsMoverObserveSlot)(uint32_t slot,FsMoverCandidate *,FsMoverDetails *,void *user);

typedef struct FsAccountResponse {
    uint32_t accountId[2];
    uint32_t authenticated;
    uint32_t maintenance;
} FsAccountResponse;

typedef struct FsConnectionResponse {
    uint32_t result;    /* Native connection poll: 0 waiting, 1 accepted, 2 failed. */
    uint32_t errorCode;
} FsConnectionResponse;
typedef struct FsSupportResponse {
    uint32_t challenge;
    uint32_t candidates[3];
    uint32_t candidateCount;
} FsSupportResponse;
typedef struct FsRemoteStatus {
    uint64_t dataId;
    uint32_t currentVersion;
    uint32_t updateVersion;
    uint32_t size;
    uint64_t transactionPassword;
    uint32_t bankReady;
    uint32_t replyMode;
    uint32_t transactionOwner; /* Native tag: Bank 0, Mover 1. */
} FsRemoteStatus;
typedef struct FsRecordStatus {
    uint32_t fileStatus;
    uint32_t supportCode;
} FsRecordStatus;
enum FsNameResult {
    FS_NAME_UNCHANGED=0, FS_NAME_FIX_NICKNAME=250,
    FS_NAME_FIX_TRAINER=251, FS_NAME_FIX_BOTH=252,
    FS_NAME_FIX_VC_NICKNAME=253, FS_NAME_FIX_VC_TRAINER=254, FS_NAME_FIX_VC_BOTH=255
};

/* A fresh opaque context must be zero-initialized once. Disconnect releases
   its transient configuration allocations; reopening replaces current values. */
/* 新上下文首次须清零；断开释放临时配置内存，重新连接读取当时的文本。 */
int FakeServerApi_Connect(FsServer *,uint32_t capacity,uint32_t owner,uint32_t backend,
    const FsStorageIo *,const FsTransactionIo *,FsConfigError *);
int FsServer_Connected(const FsServer *);
int FsServer_ConfigurationError(FsServer *,uint32_t section,FsConfigError *);
uint32_t FsServer_ConfigurationWorkspaceSize(void);
/* Ticket/account settings only; player-owned gift/check/word inputs return
   FS_UNSUPPORTED without any file mutation. */
/* 仅允许修改票务／账户设置；玩家维护的礼物／筛选／词表直接返回不支持，不改文件。 */
int FsServer_EditConfiguration(FsServer *,void *workspace,uint32_t capacity,uint32_t section,
    const FsConfigEdit *,uint32_t count,FsConfigError *);
/* Each reload reads the current published text. Already returned responses and
   staged transaction bodies are not reevaluated against later edits. */
/* 每次重载读取当前正式文本；已返回响应及已暂存事务正文不因后续编辑重新计算。 */
int FsServer_ReloadConfiguration(FsServer *,uint32_t *applied);
/* One native ticket job shares one policy/time snapshot. Reload/edit/checkout
   invalidates it; this never caches Bank files across independent requests. */
/* 同一原生票务作业共享策略／时间快照；重载、编辑、结账后失效，不缓存银行正文。 */
void FsServer_BeginTicketQuery(FsServer *);
int FsServer_Ticket(FsServer *,const uint32_t currentDate[2],FsTicketResponse *);
/* Campaign requests precede ticket jobs in Bank; never require a previous job. */
/* Bank 先请求活动、后查询票务；活动不能依赖先前已完成的票务作业。 */
int FsServer_Campaign(FsServer *,const uint32_t currentDate[2],void *body,uint32_t capacity,uint32_t *size);
int FsServer_PurchaseTicket(FsServer *,const uint32_t currentDate[2],uint32_t product);
int FsServer_TicketShop(FsServer *,FsTicketShop *);
int FsServer_SetShopLocation(FsServer *,uint32_t region,uint32_t country);
int FsServer_AddTicketFunds(FsServer *);
int FsServer_Resource(FsServer *,const char *address,void *body,uint32_t capacity,
    uint32_t *size);
int FsServer_ReadBank(FsServer *,void *data,uint32_t capacity);
int FsServer_ReadBankRange(FsServer *,uint32_t offset,void *data,uint32_t size);
int FsServer_DownloadBank(FsServer *,void *data,uint32_t capacity,
    const FsSourceMetadata *,FsTransactionParameters *);
int FsServer_CreateBank(FsServer *,const void *data,uint32_t size);
int FsServer_Stage(FsServer *,const void *bank,uint32_t size,const FsSource *);
int FsServer_DescribeTransaction(FsServer *,FsTransactionParameters *);
int FsServer_QueryMetadata(FsServer *,FsTransactionParameters *,FsSourceMetadata *);
int FsServer_QueryMetadataFor(FsServer *,uint64_t dataId,FsSourceMetadata *);
int FsServer_UpdateMetadata(FsServer *,uint64_t dataId,const FsSourceMetadata *);
int FsServer_RequestTransaction(FsServer *,const FsTransactionParameters *,int commit,int force);
int FsServer_Transaction(FsServer *,enum FsTransactionAction);
int FsServer_ObserveSource(FsServer *,const FsSource *,FsTransactionFingerprint *);
int FsServer_BeginSourceSave(FsServer *,const FsSource *);
int FsServer_SourceSaveResult(FsServer *,const FsSource *,uint32_t nativeResult,
    const FsTransactionFingerprint *validated,FsTransactionFingerprint *settled);
enum FsNativeSaveEvent {
    FS_NATIVE_SOURCE_SAVED,
    FS_NATIVE_SOURCE_RESTORED,
    FS_NATIVE_CLEANUP_STARTING,
    FS_NATIVE_CLEANUP_SAVED
};
/* Called only after the original matching/save state has supplied its evidence. */
/* 仅由原版匹配／保存状态提交实际结果，不代替原版来源匹配。 */
int FsServer_NativeSaveEvent(FsServer *,const FsTransactionParameters *,enum FsNativeSaveEvent);
/* Missing completed history does not invalidate a fresh source observation;
   journal read/validation failures remain errors, not missing-history replies. */
/* 缺少完成历史不作废新来源观察；日志读取／校验失败返回错误，不冒充历史缺失。 */
int FsServer_SettledSource(FsServer *,const FsSource *,FsTransactionFingerprint *);
int FsServer_WriteTurtle(FsServer *,const void *data,uint32_t size,int activeSave);
/* Merge the native serialized record into the existing 512-byte settings file;
   preserve its opaque tail and use the same transaction-aware publication. */
/* 将原生序列化记录并入 512 字节设置文件，保留未知尾部并沿用原事务发布。 */
int FsServer_UpdateTurtleRecord(FsServer *,const void *record,uint32_t size,int activeSave);
/* Compatibility wrapper over local settings storage, not a remote request.
   Missing settings are not created; all other bytes remain authoritative. */
/* 本地设置存储的兼容包装，不是远端请求；缺失设置不创建，其余字节保持。 */
int FsServer_UpdateTurtleLanguage(FsServer *,uint32_t languageId,uint32_t useJapaneseKanji);
/* Local settings backend: these calls never establish a server connection. */
/* 本地设置后端：调用不建立服务器连接。 */
int FsSettings_UpdateRecord(const FsTransactionIo *,uint32_t owner,const void *,uint32_t,int activeSave);
int FsSettings_UpdateLanguage(const FsTransactionIo *,uint32_t owner,uint32_t languageId,uint32_t useJapaneseKanji);
/* Existing SD: language only. Missing SD: exact, validated native file copy. */
/* 已有 SD 记录仅同步语言；确实缺失时完整复制已校验的原存档文件。 */
int FsSettings_SynchronizeLanguage(const FsTransactionIo *,uint32_t owner,uint32_t languageId,
    uint32_t useJapaneseKanji,const void *nativeRecord,uint32_t size);
int FsServer_TransferAllowed(FsServer *,int nativeTransferBoxEmpty);
int FsServer_CheckCandidates(FsServer *,const FsMoverCandidate slots[30],
    uint32_t maximumSpecies,int32_t results[30],FsMoverExtraCheck,void *user);
int FsServer_CheckPokemon(FsServer *,const FsMoverCandidate *,const FsMoverDetails *,
    uint32_t maximumSpecies,int32_t *result);
int FsServer_CheckBoxReply(FsServer *,uint32_t source,uint32_t count,
    FsMoverObserveSlot,void *user,uint8_t *body,uint32_t capacity,uint32_t *size);
int FsServer_Home(FsServer *);
int FsServer_Guard(FsServer *);
int FsServer_PrepareBank(FsServer *);
int FsServer_WriteBank(FsServer *,const void *,uint32_t size);
int FsServer_ReadTurtleRange(FsServer *,uint32_t offset,void *,uint32_t size);
int FsServer_TurtleStatus(FsServer *);
/* Guarded settings access retains the journal/settings identity checks. */
/* 受保护的设置访问保留日志／设置身份检查。 */
int FsSettings_Status(const FsTransactionIo *);
int FsSettings_ReadRange(const FsTransactionIo *,uint32_t offset,void *,uint32_t size);
/* Display-only read of the published settings, independent of journal version.
   The native decoder still validates the record. Never grant initialization,
   import, save, transfer or recovery permission from this preview. */
/* 界面只读正式设置，不依赖日志版本；记录仍由原版解码器校验。
   此预览不能授予初始化、导入、保存、传送或事务恢复许可。 */
int FsSettings_ReadPreviewRange(const FsTransactionIo *,uint32_t offset,void *,uint32_t size);
int FsSettings_LanguageNeedsWrite(const FsTransactionIo *,uint32_t languageId,
    uint32_t useJapaneseKanji,int *needed);
int FsServer_WriteSourceRange(FsServer *,uint32_t offset,const void *,uint32_t size);
int FsServer_ReadSourceRange(FsServer *,uint32_t offset,void *,uint32_t size);
int FsServer_Authenticate(FsServer *,FsAccountResponse *);
void FsServer_Connection(FsServer *,uint32_t disconnect,FsConnectionResponse *);
int FsServer_InitialValues(FsServer *,uint32_t values[2]);
int FsServer_ValidateTicket(FsServer *,uint32_t *accepted);
int FsServer_VerifyTicket(FsServer *,uint32_t minimumDays);
int FsServer_NoSave(FsServer *);
void FsServer_Disconnect(FsServer *);
int FsServer_Support(FsServer *,uint32_t challenge,FsSupportResponse *);
int FsServer_AuthorizeRollback(FsServer *,uint32_t challenge,uint32_t enteredCode);
int FsServer_NameResult(FsServer *,uint32_t *result);
int FsServer_GiftDeliverySupport(FsServer *);
int FsServer_QueryRemoteStatus(FsServer *,FsRemoteStatus *);
int FsServer_QueryRecordStatus(FsServer *,FsRecordStatus *);

#endif
