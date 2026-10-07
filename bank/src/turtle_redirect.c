#include "fs_helpers.h"
#include "patch_paths.h"
#include "turtle_redirect.h"

/* Redirect the logical Turtle record to the raw SD file used by offline mode. */
/* 将 Turtle 逻辑记录重定向到离线模式使用的 SD 原始文件。 */

/* Rebind after the title view has been destroyed. The selected storage supplies
   the whole record, including its language and transaction fields. A missing
   language returns to native language selection and subsequent initialization; no SD record
   is copied into native storage. */
/* 在标题视图销毁后重新绑定。所选存储提供完整记录，包括语言及事务字段。缺少语言
   时返回原版语言选择及后续初始化链；不会把 SD 记录复制进原存档。 */
__attribute__((used,noinline))
int TurtleRedirect_PrepareSession(TurtleGameDataView *gameData,void *heap,u32 mode)
{
    s32 result;
    u32 language;
    GameData_ClearTurtle(gameData);
    gameData->languageSelection->languageId=0;
    gameData->languageSelection->useJapaneseKanji=0;
    result=mode==TURTLE_MODE_OFFLINE?
        TurtleRedirect_CheckBackend(gameData->storage,heap):
        TurtleStorage_CheckArchiveStatus(gameData->storage,heap);
    if (result || GameData_LoadTurtle(gameData,heap)!=TURTLE_OBJECT_VALID) return 0;
    language=TurtleRecord_GetLanguage(gameData->record);
    if (!language) return 0;
    Language_ApplySettings(language&0xFFu,heap,TurtleRecord_GetKanji(gameData->record));
    return 1;
}

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

static s32 validateObject(void *object)
{
    TurtleObjectView *view=(TurtleObjectView *)object;
    return view->vtable->validate(object,0);
}

static s32 inspectTurtlePath(const char *path,u32 pathSize)
{
    u64 size=0;
    s32 result=getFileSize(path,pathSize,&size);

    if (result)
        return resultIsNotFound(result)?TURTLE_PATH_MISSING:TURTLE_PATH_UNUSABLE;
    return size==TURTLE_RECORD_FILE_SIZE?TURTLE_PATH_VALID:TURTLE_PATH_UNUSABLE;
}

static s32 tryLoadTurtle(const char *path,u32 pathSize,void *object)
{
    u32 size;
    void *buffer;
    s32 result;

    buffer=objectBuffer(object);
    if (!buffer) return TURTLE_BACKEND_INVALID;
    size=bufferSize(buffer);
    if (size!=TURTLE_RECORD_SERIALIZED_OBJECT_SIZE) return TURTLE_BACKEND_INVALID;
    result=readCompleteFile(path,pathSize,TURTLE_RECORD_FILE_SIZE,0,bufferData(buffer),size);
    if (result) return resultIsNotFound(result)?TURTLE_BACKEND_MISSING:TURTLE_BACKEND_INVALID;
    return TURTLE_BACKEND_SUCCESS;
}

static int writeTurtleRecord(void *object)
{
    void *buffer;
    u32 size;
    s32 result=deleteFile(turtleTempPath,sizeof(turtleTempPath));

    if (result && !resultIsNotFound(result)) return 0;
    buffer=objectBuffer(object);
    if (!buffer) return 0;
    size=bufferSize(buffer);
    if (size!=TURTLE_RECORD_SERIALIZED_OBJECT_SIZE) return 0;
    if (!writeCompleteFile(turtleTempPath,sizeof(turtleTempPath),bufferData(buffer),size,
        TURTLE_RECORD_FILE_SIZE)) return 0;
    result=deleteFile(turtleSavePath,sizeof(turtleSavePath));
    if (result && !resultIsNotFound(result)) return 0;
    return renameFile(turtleTempPath,sizeof(turtleTempPath),turtleSavePath,
        sizeof(turtleSavePath))==0;
}

__attribute__((used,noinline))
s32 TurtleRedirect_LoadBackend(void *storage,void *path,void *object)
{
    s32 result=tryLoadTurtle(turtleSavePath,sizeof(turtleSavePath),object);
    if (result==TURTLE_BACKEND_SUCCESS) return TURTLE_BACKEND_SUCCESS;
    if (result!=TURTLE_BACKEND_MISSING) return TURTLE_BACKEND_IO_ERROR;

    result=TurtleStorage_LoadAtOnce(storage,path,object);
    if (result) return result;
    if (validateObject(object)==TURTLE_OBJECT_VALID && !writeTurtleRecord(object))
        return TURTLE_BACKEND_IO_ERROR;
    return TURTLE_BACKEND_SUCCESS;
}

__attribute__((used,noinline))
s32 TurtleRedirect_SaveBackend(void *storage,void *path,void *object)
{
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
int TurtleRedirect_ClearTransactionAndSave(void *storage,void *object)
{
    /* Offline Bankdata commit/rollback is maintained by bankdata.tmp/.bin/.bak,
       so no remote transaction remains after either local operation completes.
       Use the stock setter and object serializer instead of editing sav.bin so
       the native checksum is regenerated before the redirected write. */
    /* 离线 Bankdata 的提交／回滚由 bankdata.tmp/.bin/.bak 维护，因此任一本地
       操作完成后都不存在待处理的远端事务。通过原版 setter 与对象序列化器清零，
       而不直接修改 sav.bin，使重定向写入前能够重新生成原版校验值。 */
    if (!storage || !object) return 0;
    TurtleRecord_SetTransactionState(object,0);
    return TurtleRedirect_SaveBackend(storage,0,object)==TURTLE_BACKEND_SUCCESS;
}

__attribute__((used,noinline))
s32 TurtleRedirect_CheckBackend(void *storage,void *path)
{
    s32 status=inspectTurtlePath(turtleSavePath,sizeof(turtleSavePath));
    s32 operation;

    if (status==TURTLE_PATH_VALID) return TURTLE_BACKEND_SUCCESS;
    if (status<0) return TURTLE_BACKEND_INVALID;
    status=inspectTurtlePath(turtleTempPath,sizeof(turtleTempPath));
    if (status==TURTLE_PATH_VALID) {
        operation=renameFile(turtleTempPath,sizeof(turtleTempPath),turtleSavePath,
            sizeof(turtleSavePath));
        return operation?TURTLE_BACKEND_INVALID:TURTLE_BACKEND_SUCCESS;
    }
    if (status<0) {
        operation=deleteFile(turtleTempPath,sizeof(turtleTempPath));
        if (operation && !resultIsNotFound(operation)) return TURTLE_BACKEND_INVALID;
    }
    return TurtleStorage_CheckArchiveStatus(storage,path);
}

__attribute__((used,noinline))
s32 TurtleRedirect_FormatBackend(void *state)
{
    TurtleStorageView *storage=((TurtleFormatStateView *)state)->storage;
    s32 result;

    ApplicationCondition_Add(TURTLE_APPLICATION_CONDITION_ID);
    result=deleteFile(turtleTempPath,sizeof(turtleTempPath));
    if (result && !resultIsNotFound(result)) goto failed;
    result=deleteFile(turtleSavePath,sizeof(turtleSavePath));
    if (result && !resultIsNotFound(result)) goto failed;
    storage->secondaryResult=TURTLE_BACKEND_SUCCESS;
    return TURTLE_BACKEND_SUCCESS;
failed:
    storage->secondaryResult=TURTLE_BACKEND_IO_ERROR;
    return TURTLE_BACKEND_IO_ERROR;
}

__attribute__((used,noinline))
s32 TurtleRedirect_FormatPoll(void *state,u8 *result)
{
    TurtleStorageView *storage=((TurtleFormatStateView *)state)->storage;
    *result=storage->secondaryResult;
    ApplicationCondition_Remove(TURTLE_APPLICATION_CONDITION_ID);
    return 1;
}
