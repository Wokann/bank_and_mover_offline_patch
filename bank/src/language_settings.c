#include "language_settings.h"
#include "fakeServer/fake_server_client.h"
#include <stddef.h>

enum {
    LANGUAGE_BEGIN=0, LANGUAGE_FORMAT=1, LANGUAGE_NATIVE_SAVE=2,
    LANGUAGE_SD_SYNC=3, LANGUAGE_DONE=4, LANGUAGE_FAILED=5
};

typedef struct LanguageStateView {
    u8 reserved00[8];
    TurtleGameDataView *gameData;
    void *heap;
    u32 phase;
} LanguageStateView;

/* Native FileStream ABI: construct a local read-only stream, close its native
   handle, but never call the deleting destructor on a stack object. */
/* 原版 FileStream ABI：栈上构造只读流，关闭原生句柄；绝不调用会释放对象的析构入口。 */
typedef struct NativeStreamView {
    void *vtable;
    u8 reserved04[0x23];
    u8 result;
} NativeStreamView;
typedef struct NativeStorageFilesView {
    void *archive;
    u32 reserved04,count;
    const u8 *files;
    u8 reserved10[0x10];
    u32 externalMount;
} NativeStorageFilesView;

_Static_assert(sizeof(NativeStreamView)==0x28,"native FileStream size");
_Static_assert(offsetof(NativeStreamView,result)==0x27,"native FileStream result");
_Static_assert(offsetof(NativeStorageFilesView,externalMount)==0x20,"native mount policy");

typedef void (*SetLanguageFn)(void *,u32,u32);
#define SetLanguage ((SetLanguageFn)0x002BAF00u)
#define NativeArchive_Mount ((s32 (*)(void *))0x001D8298u)
#define NativeArchive_Unmount ((s32 (*)(void *))0x001D8238u)
#define NativeStream_Construct ((void (*)(NativeStreamView *))0x001D3AE4u)
#define NativeStream_Open ((s32 (*)(NativeStreamView *,const void *,u32))0x00292808u)
#define NativeStream_Read ((u32 (*)(NativeStreamView *,void *,u32))0x002928C4u)
#define NativeStream_Close ((void (*)(NativeStreamView *))0x00292988u)
#define NativeStream_Size ((s32 (*)(void *,u64 *))0x00222C54u)

static struct {
    u32 languageId,useJapaneseKanji;
} nativeLanguage __attribute__((section(".data.language_settings")))={0,0};
static struct {
    uintptr_t peer;
    u32 languageId,useJapaneseKanji;
    u8 record[TURTLE_RECORD_FILE_SIZE];
} languageSync __attribute__((section(".data.language_sync")))={0};
static struct {
    uintptr_t peer;
    u32 languageId,useJapaneseKanji;
} languageSave __attribute__((section(".data.language_save")))={0,0,0};

static int validLanguage(u32 language)
{
    return language && language<=10 && language!=6;
}

static int rememberNative(TurtleGameDataView *gameData)
{
    if (!gameData || !gameData->record) return 0;
    u32 language=TurtleRecord_GetLanguage(gameData->record);
    u32 kanji=TurtleRecord_GetKanji(gameData->record);
    if (!validLanguage(language) || kanji>1) return 0;
    nativeLanguage.languageId=language;
    nativeLanguage.useJapaneseKanji=kanji;
    return 1;
}

/* Retain all 512 bytes, including unknown padding. Do not synthesize the file
   from the 48-byte logical object, nor read past that object's declared size. */
/* 保留文件全部 512 字节及未知尾部；不由 48 字节逻辑对象臆造文件，也不越界读对象。 */
static int readNativeRecord(TurtleGameDataView *gameData,u8 record[TURTLE_RECORD_FILE_SIZE])
{
    NativeStorageFilesView *storage=(void *)gameData->storage;
    if (!storage || !storage->archive || storage->count!=1 || !storage->files) return 0;
    if (((const u32 *)storage->files)[2]!=TURTLE_RECORD_FILE_SIZE ||
        ((const u32 *)storage->files)[3]) return 0;
    int mounted=!storage->externalMount;
    if (mounted && NativeArchive_Mount(storage->archive)) return 0;
    NativeStreamView stream;
    NativeStream_Construct(&stream);
    int opened=NativeStream_Open(&stream,storage->files+0x10,0);
    int result=0;
    if (opened) {
        u64 size=0;
        if (NativeStream_Size((u8 *)&stream+0x10,&size)>=0 && size==TURTLE_RECORD_FILE_SIZE &&
            NativeStream_Read(&stream,record,TURTLE_RECORD_FILE_SIZE)==TURTLE_RECORD_FILE_SIZE &&
            !stream.result) result=1;
    }
    NativeStream_Close(&stream);
    if (stream.result) result=0;
    if (mounted && NativeArchive_Unmount(storage->archive)) result=0;
    if (!result) return 0;
    TurtleObjectView *object=gameData->record;
    TurtleBufferView *buffer=object->vtable->createBuffer(object,0);
    if (!buffer || buffer->vtable->getSize(buffer)!=TURTLE_RECORD_SERIALIZED_OBJECT_SIZE) return 0;
    const u8 *prefix=buffer->vtable->getData(buffer);
    if (!prefix) return 0;
    for (u32 i=0;i<TURTLE_RECORD_SERIALIZED_OBJECT_SIZE;i++)
        if (record[i]!=prefix[i]) return 0;
    return 1;
}

void LanguageSettings_Cancel(void *state)
{
    uintptr_t peer=(uintptr_t)state;
    if (!peer) return;
    NativeTransport_Cancel(peer);
    if (languageSync.peer==peer) languageSync.peer=0;
    if (languageSave.peer==peer) languageSave.peer=0;
}

int LanguageSettings_SyncPending(uintptr_t peer)
{
    return peer && languageSync.peer==peer;
}

static int beginSync(uintptr_t peer,TurtleGameDataView *gameData)
{
    if (languageSync.peer) LanguageSettings_Cancel((void *)languageSync.peer);
    if (!peer || !readNativeRecord(gameData,languageSync.record)) return 0;
    languageSync.peer=peer;
    languageSync.languageId=nativeLanguage.languageId;
    languageSync.useJapaneseKanji=nativeLanguage.useJapaneseKanji;
    return 1;
}

static int pollSync(uintptr_t peer)
{
    if (!peer || languageSync.peer!=peer) return -1;
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return 0;
    if (scope.status!=FS_ASYNC_ACCEPTED) {
        LanguageSettings_Cancel((void *)peer);
        return -1;
    }
    FsServiceRequest request={.operation=FS_SERVICE_UPDATE_TURTLE_LANGUAGE};
    request.input.language.languageId=languageSync.languageId;
    request.input.language.useJapaneseKanji=languageSync.useJapaneseKanji;
    request.input.language.createIfMissing=1;
    FsAsyncDelivery delivery;
    int status=NativeTransport_PollRequest(peer,FS_SERVICE_UPDATE_TURTLE_LANGUAGE,
        &request,languageSync.record,sizeof(languageSync.record),&delivery);
    if (status==FS_ASYNC_ACCEPTED || status==FS_ASYNC_BUSY) return 0;
    int result=FS_IO_ERROR;
    if (status==FS_ASYNC_READY) {
        result=delivery.result;
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return 0;
        if (released!=FS_ASYNC_ACCEPTED) result=FS_IO_ERROR;
    }
    LanguageSettings_Cancel((void *)peer);
    return status==FS_ASYNC_READY && result==FS_OK?1:-1;
}

/* Startup has already validated/applied native settings. Its UI constructor
   waits here without reallocating its heap or rereading the native snapshot. */
/* 启动已沿原流程校验并应用主机语言；此处等待 SD 同步，不重复建堆或读取原档快照。 */
int LanguageSettings_StartupSync(uintptr_t peer,TurtleGameDataView *gameData)
{
    if (!LanguageSettings_SyncPending(peer)) {
        if (!rememberNative(gameData) || !beginSync(peer,gameData)) return -1;
    }
    return pollSync(peer);
}

/* Mode binding must not load fonts/messages or consult the SD language. Only
   its in-memory record is normalized to the native language already in use. */
/* 模式绑定不重载字体／文本，也不读取 SD 语言；只令内存记录沿用已生效的主机语言。 */
int LanguageSettings_BindRecord(TurtleGameDataView *gameData)
{
    if (!gameData || !gameData->record || !validLanguage(nativeLanguage.languageId))
        return TURTLE_PREPARE_FAILED;
    SetLanguage(gameData->record,nativeLanguage.languageId,nativeLanguage.useJapaneseKanji);
    return TURTLE_PREPARE_READY;
}

/* Callers keep the language-action flag set while these native SDK calls run.
   Archive/read errors must never authorize format or replace a valid record. */
/* 调用期间语言操作标志保持有效，使原 SDK 读写主机记录；读取失败绝不授权格式化。 */
static int loadNative(TurtleGameDataView *gameData,void *heap,int *format)
{
    *format=0;
    if (!gameData || !gameData->record || !gameData->storage || !gameData->languageSelection)
        return TURTLE_PREPARE_FAILED;
    s32 archive=TurtleStorage_CheckArchiveStatus(gameData->storage,heap);
    if (archive!=TURTLE_BACKEND_SUCCESS && archive!=TURTLE_BACKEND_MISSING)
        return TURTLE_PREPARE_FAILED;
    GameData_ClearTurtle(gameData);
    if (archive==TURTLE_BACKEND_MISSING) {
        *format=1;
        return TURTLE_PREPARE_MISSING_SETTINGS;
    }
    if (GameData_LoadTurtle(gameData,heap)!=TURTLE_OBJECT_VALID) {
        return gameData->storage->primaryResult==TURTLE_BACKEND_MISSING?
            TURTLE_PREPARE_MISSING_SETTINGS:TURTLE_PREPARE_FAILED;
    }
    return TURTLE_PREPARE_READY;
}

static int failed(LanguageStateView *state)
{
    /* Never leave the stock first-game checker a pending language-save action. */
    /* 不把已处理／失败的语言请求留给进入模式后的原游戏检查再次保存。 */
    if (state->gameData && state->gameData->languageSelection) {
        state->gameData->languageSelection->languageId=0;
        state->gameData->languageSelection->useJapaneseKanji=0;
    }
    LanguageSettings_Cancel(state);
    state->phase=LANGUAGE_FAILED;
    return -1;
}

static void beginNativeSave(LanguageStateView *state)
{
    SetLanguage(state->gameData->record,languageSave.languageId,languageSave.useJapaneseKanji);
    TurtleRedirect_OriginalStateSaveBegin(state,0);
    state->phase=LANGUAGE_NATIVE_SAVE;
}

/* Original language confirmation -> original SDK save -> SD synchronization
   -> native return to title. A native failure never updates SD. */
/* 原语言确认 → 原 SDK 保存 → SD 单向同步 → 原流程返回标题；主机保存失败不更新 SD。 */
int LanguageSettings_Save(void *opaque)
{
    LanguageStateView *state=opaque;
    if (!state) return -1;
    if (state->phase==LANGUAGE_DONE) return 1;
    if (state->phase==LANGUAGE_FAILED) return -1;
    if (state->phase==LANGUAGE_BEGIN) {
        LanguageSettings_Cancel(state);
        TurtleGameDataView *gameData=state->gameData;
        if (!gameData || !gameData->languageSelection) return failed(state);
        languageSave.peer=(uintptr_t)state;
        languageSave.languageId=gameData->languageSelection->languageId;
        languageSave.useJapaneseKanji=gameData->languageSelection->useJapaneseKanji;
        if (!validLanguage(languageSave.languageId) || languageSave.useJapaneseKanji>1)
            return failed(state);
        int format;
        int result=loadNative(gameData,state->heap,&format);
        if (result==TURTLE_PREPARE_FAILED) return failed(state);
        if (format) {
            TurtleFormatStateView view={.storage=gameData->storage};
            TurtleStorage_FormatStart(&view,state->heap);
            state->phase=LANGUAGE_FORMAT;
        } else beginNativeSave(state);
        return 0;
    }
    if (languageSave.peer!=(uintptr_t)state) return failed(state);
    if (state->phase==LANGUAGE_FORMAT) {
        TurtleFormatStateView view={.storage=state->gameData->storage};
        u8 result=0xFF;
        if (!TurtleStorage_FormatPoll(&view,&result)) return 0;
        if (result!=TURTLE_BACKEND_SUCCESS) return failed(state);
        GameData_ClearTurtle(state->gameData);
        beginNativeSave(state);
        return 0;
    }
    if (state->phase==LANGUAGE_NATIVE_SAVE) {
        s32 result=TurtleRedirect_OriginalStateSavePoll(state);
        if (!result) return 0;
        if (result!=1 || !rememberNative(state->gameData)) return failed(state);
        state->gameData->languageSelection->languageId=0;
        state->gameData->languageSelection->useJapaneseKanji=0;
        if (!beginSync((uintptr_t)state,state->gameData)) return failed(state);
        state->phase=LANGUAGE_SD_SYNC;
        return 0;
    }
    if (state->phase!=LANGUAGE_SD_SYNC) return failed(state);
    int result=pollSync((uintptr_t)state);
    if (!result) return 0;
    if (result<0) return failed(state);
    languageSave.peer=0;
    state->phase=LANGUAGE_DONE;
    return 1;
}
