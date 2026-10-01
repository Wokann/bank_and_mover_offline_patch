#include "fs_helpers.h"
#include "patch_paths.h"
#include "turtle_redirect.h"

/* Redirect the logical Turtle record to the raw SD file used by offline mode. */
/* 将 Turtle 逻辑记录重定向到离线模式使用的 SD 原始文件。 */

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
