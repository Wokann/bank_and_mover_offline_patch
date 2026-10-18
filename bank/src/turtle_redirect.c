#include "fs_system/fs_helpers.h"
#include "turtle_redirect.h"
#include "language_settings.h"
#include "fakeServer/fake_server_client.h"
#include "fakeServer/sd_adapter.h"

enum {
    TURTLE_FORMAT_PENDING=0xFF, TURTLE_FORMAT_REQUEST=0x400,
    TURTLE_SETTINGS_REQUEST=0x401, TURTLE_PREPARE_REQUEST=0x402,
    TURTLE_IMPORT_REQUEST=0x403, TURTLE_PREPARE_READ=1,
    TURTLE_PREPARE_PUBLISH=2, TURTLE_PREPARE_INITIAL_CHECKED=3
};

/* Redirect the logical Turtle record to the raw SD file used by offline mode. */
/* 将 Turtle 逻辑记录重定向到离线模式使用的 SD 原始文件。 */

/* Rebind transaction/settings storage, not the global language. */
/* 只重新绑定事务／设置存储，不切换全局语言。 */
static void clearTurtle(TurtleGameDataView *gameData)
{
    GameData_ClearTurtle(gameData);
    /* This is a pending save request, not the applied interface language. */
    /* 这里只清理待保存的语言请求，不修改已应用的界面语言或主机语言快照。 */
    gameData->languageSelection->languageId=0;
    gameData->languageSelection->useJapaneseKanji=0;
}

/* UI-owned handoff, in imported data (not native BSS). No UI pointer is passed
   to the worker. The scope exists only during the original synchronous object
   decoder; missing native settings are imported only after publication. */
/* 界面持有的交接状态位于新增数据区，不占原 BSS。后台不接收界面指针。scope 仅在
   原版对象解码调用期间生效；缺失 SD 设置时，确认原存档副本发布后才完成绑定。 */
static struct {
    uintptr_t peer;
    u32 phase;
    uintptr_t scoped;
    int status;
    u8 record[TURTLE_RECORD_SERIALIZED_OBJECT_SIZE];
} turtlePrepare __attribute__((section(".data.turtle_prepare")))={0};

static void *objectBuffer(void *object)
{
    TurtleObjectView *view=(TurtleObjectView *)object;
    return view->vtable->createBuffer(object,0);
}

static u32 bufferSize(void *buffer)
{
    TurtleBufferView *view=(TurtleBufferView *)buffer;
    return view->vtable->getSize(buffer);
}

static void *bufferData(void *buffer)
{
    TurtleBufferView *view=(TurtleBufferView *)buffer;
    return view->vtable->getData(buffer);
}

__attribute__((used,noinline))
int TurtleRedirect_PrepareSessionPending(uintptr_t peer)
{
    return peer && turtlePrepare.peer==peer;
}

__attribute__((used,noinline))
void TurtleRedirect_CancelPrepareSession(uintptr_t peer)
{
    if (!peer || turtlePrepare.peer!=peer) return;
    NativeTransport_Cancel(peer);
    turtlePrepare=(__typeof__(turtlePrepare)){0};
}

static int prepareRequest(uintptr_t peer,int publish)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return FS_ASYNC_BUSY;
    if (scope.status!=FS_ASYNC_ACCEPTED) {
        turtlePrepare.status=FS_IO_ERROR;
        return FS_ASYNC_READY;
    }
    FsServiceRequest input={.operation=publish?FS_SERVICE_UPDATE_TURTLE_RECORD:
        FS_SERVICE_PREPARE_TURTLE_RECORD};
    if (publish)
        for (u32 i=0;i<sizeof(input.input.turtle.record);i++)
            input.input.turtle.record[i]=turtlePrepare.record[i];
    FsAsyncDelivery delivery;
    int received=NativeTransport_PollRequest(peer,publish?TURTLE_IMPORT_REQUEST:TURTLE_PREPARE_REQUEST,
        &input,0,publish?0:sizeof(turtlePrepare.record),&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return FS_ASYNC_BUSY;
    int result=FS_IO_ERROR;
    if (received==FS_ASYNC_READY) {
        result=delivery.result;
        if (!publish && result==FS_OK) {
            if (!delivery.body || delivery.reply.bodySize!=sizeof(turtlePrepare.record) ||
                delivery.bodySize!=sizeof(turtlePrepare.record)) result=FS_IO_ERROR;
            else for (u32 i=0;i<sizeof(turtlePrepare.record);i++)
                turtlePrepare.record[i]=((const u8 *)delivery.body)[i];
        }
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return FS_ASYNC_BUSY;
        if (released!=FS_ASYNC_ACCEPTED) result=FS_IO_ERROR;
    }
    turtlePrepare.status=result;
    return FS_ASYNC_READY;
}

static void beginPrepare(uintptr_t peer)
{
    if (turtlePrepare.peer==peer) return;
    TurtleRedirect_CancelPrepareSession(turtlePrepare.peer);
    turtlePrepare.peer=peer;
    turtlePrepare.phase=TURTLE_PREPARE_READ;
}

static int freezePreparedRecord(TurtleGameDataView *gameData)
{
    void *buffer=objectBuffer(gameData->record);
    const u8 *data=buffer && bufferSize(buffer)==sizeof(turtlePrepare.record)?bufferData(buffer):0;
    if (!data) return 0;
    for (u32 i=0;i<sizeof(turtlePrepare.record);i++) turtlePrepare.record[i]=data[i];
    turtlePrepare.phase=TURTLE_PREPARE_PUBLISH;
    return 1;
}

/* Original substates 18/19 already wait. Read SD once on the worker; retain
   the original archive check, object decoder and native result branches. */
/* 原子状态 18／19 已有等待出口。SD 仅在后台读取一次，原档案检查、对象解码及
   结果分支保留；读取确认不等于允许初始化或导入已完成。 */
__attribute__((used,noinline))
s32 TurtleRedirect_InitialCheckPoll(uintptr_t peer,void *storage,void *path)
{
    if (!peer) return TURTLE_BACKEND_IO_ERROR;
    beginPrepare(peer);
    if (turtlePrepare.phase==TURTLE_PREPARE_READ) {
        if (prepareRequest(peer,0)!=FS_ASYNC_READY) return -1;
        turtlePrepare.phase=TURTLE_PREPARE_INITIAL_CHECKED;
    }
    if (turtlePrepare.phase!=TURTLE_PREPARE_INITIAL_CHECKED) return TURTLE_BACKEND_IO_ERROR;
    if (turtlePrepare.status==FS_OK) return TURTLE_BACKEND_SUCCESS;
    if (turtlePrepare.status==FS_MISSING) return TurtleStorage_CheckArchiveStatus(storage,path);
    return turtlePrepare.status==FS_INVALID?TURTLE_BACKEND_INVALID:TURTLE_BACKEND_IO_ERROR;
}

__attribute__((used,noinline))
s32 TurtleRedirect_InitialLoadPoll(uintptr_t peer,TurtleGameDataView *gameData,void *heap)
{
    if (!peer || !gameData || !gameData->record || turtlePrepare.peer!=peer)
        return TURTLE_BACKEND_INVALID;
    if (turtlePrepare.phase==TURTLE_PREPARE_INITIAL_CHECKED) {
        turtlePrepare.scoped=(uintptr_t)gameData->record;
        s32 loaded=GameData_LoadTurtle(gameData,heap);
        turtlePrepare.scoped=0;
        if (loaded!=TURTLE_OBJECT_VALID || turtlePrepare.status!=FS_MISSING) {
            TurtleRedirect_CancelPrepareSession(peer);
            return loaded;
        }
        if (!freezePreparedRecord(gameData)) {
            GameData_ClearTurtle(gameData);
            TurtleRedirect_CancelPrepareSession(peer);
            return TURTLE_BACKEND_INVALID;
        }
    }
    if (turtlePrepare.phase!=TURTLE_PREPARE_PUBLISH) return TURTLE_BACKEND_INVALID;
    if (prepareRequest(peer,1)!=FS_ASYNC_READY) return -1;
    int result=turtlePrepare.status;
    TurtleRedirect_CancelPrepareSession(peer);
    if (result==FS_OK) return TURTLE_OBJECT_VALID;
    GameData_ClearTurtle(gameData);
    return TURTLE_BACKEND_INVALID;
}

__attribute__((used,noinline))
int TurtleRedirect_PrepareSessionPoll(uintptr_t peer,TurtleGameDataView *gameData,void *heap,u32 mode)
{
    if (!peer || !gameData || !gameData->record || !gameData->languageSelection)
        return TURTLE_PREPARE_FAILED;
    if (mode!=TURTLE_MODE_OFFLINE) {
        TurtleRedirect_CancelPrepareSession(turtlePrepare.peer);
        clearTurtle(gameData);
        if (TurtleStorage_CheckArchiveStatus(gameData->storage,heap) ||
            GameData_LoadTurtle(gameData,heap)!=TURTLE_OBJECT_VALID) return TURTLE_PREPARE_FAILED;
        return LanguageSettings_BindRecord(gameData);
    }
    beginPrepare(peer);
    if (turtlePrepare.phase==TURTLE_PREPARE_READ) {
        if (prepareRequest(peer,0)!=FS_ASYNC_READY) return TURTLE_PREPARE_WAIT;
        int status=turtlePrepare.status;
        if (status!=FS_OK && status!=FS_MISSING) {
            TurtleRedirect_CancelPrepareSession(peer);
            return TURTLE_PREPARE_FAILED;
        }
        if (status==FS_MISSING) {
            s32 archive=TurtleStorage_CheckArchiveStatus(gameData->storage,heap);
            if (archive!=TURTLE_BACKEND_SUCCESS) {
                TurtleRedirect_CancelPrepareSession(peer);
                if (archive!=TURTLE_BACKEND_MISSING) return TURTLE_PREPARE_FAILED;
                clearTurtle(gameData);
                return TURTLE_PREPARE_FAILED;
            }
        }
        clearTurtle(gameData);
        turtlePrepare.scoped=(uintptr_t)gameData->record;
        int loaded=GameData_LoadTurtle(gameData,heap)==TURTLE_OBJECT_VALID;
        turtlePrepare.scoped=0;
        if (!loaded) {
            TurtleRedirect_CancelPrepareSession(peer);
            return TURTLE_PREPARE_FAILED;
        }
        if (status!=FS_MISSING) {
            TurtleRedirect_CancelPrepareSession(peer);
            return LanguageSettings_BindRecord(gameData);
        }
        if (!freezePreparedRecord(gameData)) {
            clearTurtle(gameData);
            TurtleRedirect_CancelPrepareSession(peer);
            return TURTLE_PREPARE_FAILED;
        }
    }
    if (prepareRequest(peer,1)!=FS_ASYNC_READY) return TURTLE_PREPARE_WAIT;
    int result=turtlePrepare.status;
    TurtleRedirect_CancelPrepareSession(peer);
    if (result!=FS_OK) {
        clearTurtle(gameData);
        return TURTLE_PREPARE_FAILED;
    }
    return LanguageSettings_BindRecord(gameData);
}

static int writeTurtleRecord(void *object)
{
    void *buffer;
    u32 size;
    buffer=objectBuffer(object);
    if (!buffer) return 0;
    size=bufferSize(buffer);
    if (size!=TURTLE_RECORD_SERIALIZED_OBJECT_SIZE) return 0;
    return FsSettings_UpdateRecord(&FsSdTransactions,FS_TRANSACTION_OWNER_BANK,bufferData(buffer),size,
        *TurtleRedirect_SaveActiveStorage!=0)==FS_OK;
}

__attribute__((used,noinline))
s32 TurtleRedirect_LoadBackend(void *storage,void *path,void *object)
{
    if (turtlePrepare.scoped && turtlePrepare.scoped==(uintptr_t)object) {
        if (turtlePrepare.status==FS_MISSING)
            return TurtleStorage_LoadAtOnce(storage,path,object);
        void *buffer=objectBuffer(object);
        u8 *data=buffer && bufferSize(buffer)==sizeof(turtlePrepare.record)?bufferData(buffer):0;
        if (!data || turtlePrepare.status!=FS_OK) return TURTLE_BACKEND_IO_ERROR;
        for (u32 i=0;i<sizeof(turtlePrepare.record);i++) data[i]=turtlePrepare.record[i];
        return TURTLE_BACKEND_SUCCESS;
    }
    /* All offline decoder calls use a released worker snapshot. An unexpected
       unscoped call cannot fall back to synchronous SD I/O or import data. */
    /* 离线解码只使用已释放的后台快照；意外的无 scope 调用不能同步读取 SD 或导入记录。 */
    return TURTLE_BACKEND_IO_ERROR;
}

__attribute__((used,noinline))
s32 TurtleRedirect_SaveBackend(void *storage,void *path,void *object)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
    if (scope.status!=FS_ASYNC_ACCEPTED) return TURTLE_BACKEND_IO_ERROR;
    TurtleStorageView *storageView=(TurtleStorageView *)storage;
    TurtleObjectView *objectView=(TurtleObjectView *)object;
    u32 callbackResult[2];
    s32 result;
    (void)path;

    ApplicationCondition_Add(TURTLE_APPLICATION_CONDITION_ID);
    storageView->object=object;
    objectView->vtable->prepare(object);
    objectView->vtable->finalize(object);
    result=writeTurtleRecord(object)?TURTLE_BACKEND_SUCCESS:TURTLE_BACKEND_IO_ERROR;
    objectView->vtable->validate(object,callbackResult);
    storageView->primaryResult=(u8)result;
    storageView->secondaryResult=(u8)result;
    TurtleStorage_FinishSave(storage,0,0,0);
    return result;
}

__attribute__((used,noinline))
s32 TurtleRedirect_FormatBackend(void *state)
{
    TurtleStorageView *storage=((TurtleFormatStateView *)state)->storage;
    ApplicationCondition_Add(TURTLE_APPLICATION_CONDITION_ID);
    NativeTransport_Cancel((uintptr_t)state);
    /* This is a start acknowledgement, not a completed format. The original
       substate 21 already polls before resetting the native record. */
    /* 此处仅确认启动，不代表格式化完成；原子状态 21 会等待结果后才重置原记录。 */
    storage->secondaryResult=TURTLE_FORMAT_PENDING;
    return TURTLE_BACKEND_SUCCESS;
}

__attribute__((used,noinline))
s32 TurtleRedirect_FormatPoll(void *state,u8 *result)
{
    TurtleStorageView *storage=((TurtleFormatStateView *)state)->storage;
    if (storage->secondaryResult==TURTLE_FORMAT_PENDING) {
        NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
        if (scope.status==FS_ASYNC_BUSY) return 0;
        FsAsyncDelivery delivery;
        const FsServiceRequest input={.operation=FS_SERVICE_FORMAT_TURTLE};
        int received=scope.status==FS_ASYNC_ACCEPTED?
            NativeTransport_PollRequest((uintptr_t)state,TURTLE_FORMAT_REQUEST,&input,0,0,&delivery):
            FS_ASYNC_INVALID;
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
        int ok=received==FS_ASYNC_READY && delivery.result==FS_OK;
        if (received==FS_ASYNC_READY) {
            int released=NativeTransport_Release(&delivery);
            if (released==FS_ASYNC_BUSY) return 0;
            if (released!=FS_ASYNC_ACCEPTED) ok=0;
        }
        storage->secondaryResult=ok?TURTLE_BACKEND_SUCCESS:TURTLE_BACKEND_IO_ERROR;
        NativeTransport_Cancel((uintptr_t)state);
    }
    *result=storage->secondaryResult;
    ApplicationCondition_Remove(TURTLE_APPLICATION_CONDITION_ID);
    return 1;
}

/* All native settings-save states share this begin/poll boundary. Physical
   game saves remain native. Prepare/checksum still run on that UI thread. Only
   copied record bytes reach the worker, never object/storage/state pointers. */
/* 原版设置保存状态共用此开始／轮询边界，实体游戏保存仍走原版。对象准备及校验更新仍在界面线程执行。
   后台只接收复制的记录字节，不保存对象、存储或状态指针。 */
__attribute__((used,noinline))
void TurtleRedirect_StateSaveBegin(void *opaque,u32 kind)
{
    TurtleRedirect_OriginalStateSaveBegin(opaque,kind);
    if (kind) return;
    TurtleLocalSaveStateView *state=opaque;
    TurtleStorageView *storage=state->gameData->storage;
    TurtleObjectView *object=state->gameData->record;
    NativeTransport_Cancel((uintptr_t)opaque);
    ApplicationCondition_Add(TURTLE_APPLICATION_CONDITION_ID);
    storage->object=object;
    object->vtable->prepare(object);
    object->vtable->finalize(object);
    storage->secondaryResult=TURTLE_FORMAT_PENDING;
}

__attribute__((used,noinline))
s32 TurtleRedirect_StateSavePoll(void *opaque)
{
    TurtleLocalSaveStateView *state=opaque;
    if (state->kind) return TurtleRedirect_OriginalStateSavePoll(opaque);
    TurtleStorageView *storage=state->gameData->storage;
    if (storage->secondaryResult==TURTLE_FORMAT_PENDING) {
        NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
        if (scope.status==FS_ASYNC_BUSY) return 0;
        int result=FS_IO_ERROR;
        if (scope.status==FS_ASYNC_ACCEPTED) {
            FsServiceRequest input={.operation=FS_SERVICE_UPDATE_TURTLE_RECORD};
            void *buffer=objectBuffer(storage->object);
            const u8 *data=buffer && bufferSize(buffer)==sizeof(input.input.turtle.record)?bufferData(buffer):0;
            if (data) {
                for (u32 i=0;i<sizeof(input.input.turtle.record);i++) input.input.turtle.record[i]=data[i];
                input.input.turtle.activeSave=*TurtleRedirect_SaveActiveStorage!=0;
                /* This result belongs to the native settings poll, not to the
                   enclosing transaction checkpoint. Release before the native
                   finish, including errors, so StatePending cannot skip it. */
                /* 此结果由原设置轮询消费，而非外层事务检查点。错误也须先释放再原版收尾，
                   避免外层 StatePending 提前跳走、漏掉对象及应用条件清理。 */
                FsAsyncDelivery delivery;
                int received=NativeTransport_PollRequest((uintptr_t)opaque,TURTLE_SETTINGS_REQUEST,&input,0,0,&delivery);
                if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
                if (received==FS_ASYNC_READY) {
                    result=delivery.result;
                    int released=NativeTransport_Release(&delivery);
                    if (released==FS_ASYNC_BUSY) return 0;
                    if (released!=FS_ASYNC_ACCEPTED) result=FS_IO_ERROR;
                }
            }
        }
        u32 callbackResult[2];
        TurtleObjectView *object=storage->object;
        object->vtable->validate(object,callbackResult);
        storage->primaryResult=result==FS_OK?TURTLE_BACKEND_SUCCESS:TURTLE_BACKEND_IO_ERROR;
        storage->secondaryResult=storage->primaryResult;
        TurtleStorage_FinishSave(storage,0,0,0);
        NativeTransport_Cancel((uintptr_t)opaque);
    }
    s32 result=storage->secondaryResult==TURTLE_BACKEND_SUCCESS?1:2;
    StateLocalSave_Complete(opaque,(u32)result);
    return result;
}
