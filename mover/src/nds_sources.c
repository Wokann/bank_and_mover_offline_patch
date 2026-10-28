#include "nds_sources.h"
#include "bankdata_redirect.h"
#include "fs_system/diagnostic_log.h"
#include <string.h>

enum NdsSaveStep { NDS_SAVE_PREPARE=1,NDS_SAVE_NATIVE,NDS_SAVE_READ_IMAGE,NDS_SAVE_PUBLISH,NDS_SAVE_DONE };

static FsLocalSource *localSource(void)
{
    return (FsLocalSource *)FAKE_SERVER_SOURCE_CONTEXT_ADDRESS;
}

static FsSource selectedSource(const NdsScannerContext *context)
{
    FsSource source={FS_TRANSACTION_NATIVE_SOURCE,context?context->activeGameCode:0,0,0,
        *CombinePatch_ModeStorage,0,0};
    if (context && context->backend==NDS_BACKEND_SD) {
        source.kind=FS_TRANSACTION_NDS_FILE;
        source.gameCode=context->activeGameCode;
        source.path=context->path;
        source.pathSize=context->activePathSize;
        source.expected=context->selectedSaveValid?&context->selectedSave:0;
    }
    return source;
}

static const u16 ndsDirectoryPath[]={
    '/', 'r', 'o', 'm', 's', '/', 'n', 'd', 's', 0
};
static const u16 ndsRomPrefix[]={
    '/', 'r', 'o', 'm', 's', '/', 'n', 'd', 's', '/', 0
};
static const u16 ndsSavePrefix[]={
    '/', 'r', 'o', 'm', 's', '/', 'n', 'd', 's', '/', 's', 'a', 'v', 'e', 's', '/', 0
};

static NdsScannerContext *scannerContext(void)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (!context) {
        context=(NdsScannerContext *)MoverHeap_Allocate(sizeof(*context));
        if (context) {
            MoverMemory_Clear(context,sizeof(*context));
            *NDS_SCANNER_CONTEXT_SLOT=context;
        }
    }
    return context;
}

static s32 closeScanDirectory(NdsScannerContext *context)
{
    s32 result=0,closeResult;
    if (context->directoryHandle) {
        result=closeDirectory(context->directoryHandle);
        context->directoryHandle=0;
    }
    if (context->directoryArchive) {
        closeResult=closeArchive(context->directoryArchive);
        context->directoryArchive=0;
        if (!result) result=closeResult;
    }
    return result;
}

static s32 resetScanner(NdsScannerContext *context)
{
    NativeTransport_Cancel((uintptr_t)context);
    s32 result=closeScanDirectory(context);
    MoverMemory_Clear(context,sizeof(*context));
    context->phase=NDS_SCAN_PHYSICAL;
    context->backend=NDS_BACKEND_CARD;
    return result;
}

static u32 utf16Length(const u16 *text,u32 capacity)
{
    u32 length=0;
    while (length<capacity && text[length]) length++;
    return length;
}

static u16 asciiLower(u16 value)
{
    if (value>='A' && value<='Z') value=(u16)(value+('a'-'A'));
    return value;
}

static int isNdsFile(const FsDirectoryEntry *entry,u32 *nameLength)
{
    u32 length=utf16Length(entry->name,
        (u32)(sizeof(entry->name)/sizeof(entry->name[0])));
    if ((entry->attributes&FS_ATTRIBUTE_DIRECTORY) || length<5 ||
        length==(u32)(sizeof(entry->name)/sizeof(entry->name[0])) ||
        entry->fileSizeHigh || entry->fileSizeLow<NDS_ROM_HEADER_READ_SIZE)
        return 0;
    if (entry->name[length-4]!='.' || asciiLower(entry->name[length-3])!='n' ||
        asciiLower(entry->name[length-2])!='d' || asciiLower(entry->name[length-1])!='s')
        return 0;
    *nameLength=length;
    return 1;
}

static u32 appendText(u16 *destination,u32 position,const u16 *source)
{
    while (*source && position+1<NDS_PATH_CAPACITY)
        destination[position++]=*source++;
    destination[position]=0;
    return position;
}

static int buildPath(u16 *destination,const u16 *prefix,const u16 *name,u32 nameLength,
    int savePath,u32 *pathSize)
{
    u32 position=appendText(destination,0,prefix);
    u32 copyLength=savePath?nameLength-4:nameLength;
    u32 index;
    if (position+copyLength+(savePath?5u:1u)>NDS_PATH_CAPACITY) return 0;
    for (index=0;index<copyLength;index++) destination[position++]=name[index];
    if (savePath) {
        destination[position++]='.';
        destination[position++]='s';
        destination[position++]='a';
        destination[position++]='v';
    }
    destination[position]=0;
    *pathSize=(position+1)*sizeof(u16);
    return 1;
}

static int languageRank(u8 language)
{
    static const char languages[]="JOFIDSK";
    u32 index;
    for (index=0;index<sizeof(languages)-1;index++)
        if (language==(u8)languages[index]) return (int)index;
    return -1;
}

/* Only the Gen 5 title changes; trainer details and VC titles stay native. */
/* 只更改第五世代游戏名，训练家信息与 VC 游戏名仍走原版流程。 */
u32 NdsSources_TitleMessage(const MoverSourceListStateView *state,u32 originalMessage)
{
    const NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    const NdsSourceWinner *winner;
    u32 sourceId;
    int rank;
    if (!context || state->cursor>=state->sourceCount ||
        state->cursor>=NDS_SOURCE_LIST_CAPACITY) return originalMessage;
    sourceId=state->sourceIds[state->cursor];
    if (sourceId<1 || sourceId>NDS_GAME_COUNT) return originalMessage;
    winner=&context->winners[sourceId-1];
    if (!winner->valid || winner->sourceId!=sourceId ||
        (winner->gameCode&0xFFFFu)!=0x5249u ||
        (u8)(winner->gameCode>>16)!=(u8)"BAED"[sourceId-1]) return originalMessage;
    rank=languageRank((u8)(winner->gameCode>>24));
    if (rank<0) return originalMessage;
    return NDS_GAME_TITLE_MESSAGE_BASE+(u32)rank*NDS_GAME_COUNT+sourceId-1;
}

static int gameCodeIndex(u32 code)
{
    if ((code&0xFFFFu)!=0x5249u || languageRank((u8)(code>>24))<0)
        return -1;
    if ((u8)(code>>16)=='B') return 0;
    if ((u8)(code>>16)=='A') return 1;
    if ((u8)(code>>16)=='E') return 2;
    if ((u8)(code>>16)=='D') return 3;
    return -1;
}

static u32 readGameCode(const u8 header[NDS_ROM_HEADER_READ_SIZE])
{
    return (u32)header[0x0C]|((u32)header[0x0D]<<8)|
        ((u32)header[0x0E]<<16)|((u32)header[0x0F]<<24);
}

static int readCandidateHeader(NdsScannerContext *context,u32 *nameLength,u32 *pathSize)
{
    if (!isNdsFile(&context->entry,nameLength) ||
        !buildPath(context->path,ndsRomPrefix,context->entry.name,*nameLength,0,pathSize) ||
        readFileRangeUtf16(context->path,*pathSize,0,context->header,sizeof(context->header)))
        return -1;
    return gameCodeIndex(readGameCode(context->header));
}

/* Run inside the existing native card-probe worker, before it publishes its
   completion. This is only a presence test; the list still validates the save.
   Never report an SD candidate as an inserted physical card. */
/* 在原版卡带预检后台作业完成前执行，只探测候选是否存在；列表阶段仍校验存档。
   SD 候选不会被报告为插入了实体卡带。 */
static int probeSdPresence(NdsScannerContext *context)
{
    u32 entriesRead=0,nameLength=0,pathSize=0;
    u32 checked=0;
    int found=0;
    s32 result=openDirectoryUtf16(ndsDirectoryPath,sizeof(ndsDirectoryPath),
        &context->directoryArchive,&context->directoryHandle);
    while (!result) {
        result=readDirectoryEntry(context->directoryHandle,&context->entry,&entriesRead);
        if (result || !entriesRead) break;
        checked++;
        if (readCandidateHeader(context,&nameLength,&pathSize)<0 ||
            !buildPath(context->path,ndsSavePrefix,context->entry.name,nameLength,1,&pathSize) ||
            !fileIsReadWriteUtf16(context->path,pathSize)) continue;
        found=1;
        break;
    }
    s32 closeResult=closeScanDirectory(context);
    FsDiag_Record("source.presence",result?result:closeResult,(u32)found,checked,0);
    return found;
}

__attribute__((used,noinline,section(".text.nds_sources.00_availability")))
u32 NdsSources_AvailabilityUpdate(MoverSourceAvailabilityStateView *state,
    u32 arg1,u32 arg2,s32 arg3)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    u32 previous=state->state;
    if (previous==MOVER_SOURCE_AVAILABILITY_INITIALIZE) {
        context=scannerContext();
        if (context) {
            /* Retire the previous selection before the native worker probes
               the card again, including returns to title/account changes. */
            /* 再次预检前清除上次选择，包括返回标题或切换账户后的旧 SD 来源。 */
            s32 result=resetScanner(context);
            context->phase=NDS_SCAN_INACTIVE;
            context->presencePending=1;
            FsDiag_Record("source.preflight.begin",result,0,0,0);
        }
    }
    u32 result=MoverSourceAvailability_Update(state,arg1,arg2,arg3);
    /* Native job completion synchronizes the worker's presence result. Only
       its no-card/no-VC branch may be extended; errors and UI remain native. */
    /* 原版作业完成时同步后台结果；只扩展无卡带且无 VC 的分支，保留原错误与界面。 */
    if (context && previous==MOVER_SOURCE_AVAILABILITY_WAIT_CARD &&
        state->state!=MOVER_SOURCE_AVAILABILITY_WAIT_CARD) context->presencePending=0;
    if (context && previous==MOVER_SOURCE_AVAILABILITY_CHECK_VC &&
        state->state==MOVER_SOURCE_AVAILABILITY_MISSING && context->presenceFound) {
        state->state=MOVER_SOURCE_AVAILABILITY_READY;
        FsDiag_Record("source.preflight.sd",0,0,0,0);
    }
    return result;
}

static u8 sourceIdForIndex(u32 index)
{
    return (u8)(index+1);
}

static int allSourcesFinal(const NdsScannerContext *context)
{
    u32 index;
    for (index=0;index<NDS_GAME_COUNT;index++) {
        const NdsSourceWinner *winner=&context->winners[index];
        if (!winner->valid || (winner->kind!=NDS_WINNER_CARD && winner->rank!=0))
            return 0;
    }
    return 1;
}

static void captureDisplay(void *sourceContext,u8 display[NDS_DISPLAY_RECORD_SIZE],u32 face)
{
    MoverNds_SelectSaveFace(sourceContext,face);
    MoverMemory_Clear(display,NDS_DISPLAY_RECORD_SIZE);
    MoverMemory_Copy(display,MoverNds_GetDisplayName(sourceContext),0x1A);
    *(u32 *)(display+0x1C)=MoverNds_GetDisplayValue(sourceContext);
    *(u32 *)(display+0x20)=MoverNds_GetDisplayFlag(sourceContext);
}

static void capturePhysicalSources(NdsScannerContext *context,MoverSourceListStateView *state)
{
    u32 item;
    u32 gameCode=0;
    if (state->sourceCount && NdsSources_OriginalReadGameCode(&gameCode)!=0)
        gameCode=0;
    for (item=0;item<state->sourceCount && item<NDS_GAME_COUNT;item++) {
        u32 sourceId=state->sourceIds[item];
        if (sourceId>=1 && sourceId<=NDS_GAME_COUNT) {
            NdsSourceWinner *winner=&context->winners[sourceId-1];
            winner->valid=1;
            winner->kind=NDS_WINNER_CARD;
            winner->sourceId=(u8)sourceId;
            winner->gameCode=gameCode;
            MoverMemory_Copy(winner->display,
                state->listUi+0x90+item*NDS_DISPLAY_RECORD_SIZE,NDS_DISPLAY_RECORD_SIZE);
        }
    }
    state->sourceCount=0;
}

static void emitNdsSources(NdsScannerContext *context,MoverSourceListStateView *state)
{
    u32 index;
    s32 closeResult=closeScanDirectory(context);
    if (closeResult) FsDiag_Record("source.directory.close",closeResult,0,0,0);
    /* An optional SD directory failure does not invalidate a verified card
       or candidate, and must not bypass the native VC enumeration. */
    /* 可选 SD 目录失败不否定已校验的卡带或候选，也不能跳过后续原版 VC 枚举。 */
    state->sourceCount=0;
    for (index=0;index<NDS_GAME_COUNT;index++) {
        NdsSourceWinner *winner=&context->winners[index];
        u32 item;
        if (!winner->valid) continue;
        item=state->sourceCount++;
        state->sourceIds[item]=winner->sourceId;
        MoverMemory_Copy(state->listUi+0x90+item*NDS_DISPLAY_RECORD_SIZE,
            winner->display,NDS_DISPLAY_RECORD_SIZE);
    }
    context->backend=NDS_BACKEND_CARD;
    context->phase=NDS_SCAN_PASSTHROUGH;
    state->state=3;
}

static void beginDirectoryScan(NdsScannerContext *context,MoverSourceListStateView *state)
{
    s32 result;
    capturePhysicalSources(context,state);
    if (allSourcesFinal(context)) {
        emitNdsSources(context,state);
        return;
    }
    result=openDirectoryUtf16(ndsDirectoryPath,sizeof(ndsDirectoryPath),
        &context->directoryArchive,&context->directoryHandle);
    if (result) {
        FsDiag_Record("source.directory.open",result,0,0,0);
        emitNdsSources(context,state);
        return;
    }
    context->phase=NDS_SCAN_DIRECTORY;
}

static void beginCandidateValidation(NdsScannerContext *context,
    MoverSourceListStateView *state,u32 index,u32 rank,u32 gameCode,u32 pathSize)
{
    context->candidateIndex=index;
    context->candidateRank=rank;
    context->activeGameCode=gameCode;
    context->activePathSize=pathSize;
    context->backend=NDS_BACKEND_SD;
    MoverNds_StartLoad(state->sourceContext,state->workerArena);
    context->phase=NDS_SCAN_VALIDATION;
}

static void scanOneEntry(NdsScannerContext *context,MoverSourceListStateView *state)
{
    u32 entriesRead=0,nameLength=0,pathSize=0;
    s32 result=readDirectoryEntry(context->directoryHandle,&context->entry,&entriesRead);
    int index,rank;
    u32 gameCode;
    NdsSourceWinner *winner;
    if (result) {
        FsDiag_Record("source.directory.read",result,0,0,0);
        emitNdsSources(context,state);
        return;
    }
    if (entriesRead==0) {
        emitNdsSources(context,state);
        return;
    }
    index=readCandidateHeader(context,&nameLength,&pathSize);
    if (index<0) return;
    rank=languageRank(context->header[0x0F]);
    winner=&context->winners[index];
    if (winner->valid && (winner->kind==NDS_WINNER_CARD || winner->rank<=(u32)rank))
        return;
    if (!buildPath(context->path,ndsSavePrefix,context->entry.name,nameLength,1,&pathSize) ||
        !fileIsReadWriteUtf16(context->path,pathSize))
        return;
    gameCode=readGameCode(context->header);
    beginCandidateValidation(context,state,(u32)index,(u32)rank,gameCode,pathSize);
}

static void finishCandidateValidation(NdsScannerContext *context,
    MoverSourceListStateView *state)
{
    s32 result=-1;
    int validation;
    NdsSourceWinner *winner;
    if (MoverNds_PollLoad(state->sourceContext,&result)) return;
    if (result>=0) {
        validation=MoverNds_ValidateSave(context->candidateIndex>=2,
            MoverNds_GetSelectedSave(state->sourceContext));
        if (validation==0 || validation==1) {
            winner=&context->winners[context->candidateIndex];
            winner->valid=1;
            winner->kind=NDS_WINNER_SD;
            winner->rank=(u8)context->candidateRank;
            winner->sourceId=sourceIdForIndex(context->candidateIndex);
            winner->gameCode=context->activeGameCode;
            winner->savePathSize=context->activePathSize;
            MoverMemory_Copy(winner->savePath,context->path,context->activePathSize);
            captureDisplay(state->sourceContext,winner->display,(u32)validation);
        }
    }
    context->backend=NDS_BACKEND_CARD;
    if (allSourcesFinal(context)) emitNdsSources(context,state);
    else context->phase=NDS_SCAN_DIRECTORY;
}

static void updateStockLoadingUi(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3)
{
    u32 stockState=state->state;
    state->state=0xFFFFFFFFu;
    (void)MoverSourceList_Update(state,arg1,arg2,arg3);
    state->state=stockState;
}

static u32 updateStockSourceList(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3)
{
    u32 result=MoverSourceList_Update(state,arg1,arg2,arg3);
    if (result==MOVER_STATE_UPDATE_FINISHED && state->cursor>=state->sourceCount)
        state->cursor=0;
    return result;
}

__attribute__((used,noinline,section(".text.nds_sources.01_list")))
u32 NdsSources_ListUpdate(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3)
{
    NdsScannerContext *context;
    u32 result;
    if (state->state==0) {
        /* A reused native object must not turn old SD entries into freshly
           detected cards. The native probe rebuilds this round's list. */
        /* 复用原版对象时不能把旧 SD 条目当作本轮卡带；本轮来源由原检测重新建立。 */
        state->sourceCount=0;
        MoverMemory_Clear(state->sourceIds,sizeof(state->sourceIds));
    }
    context=scannerContext();
    if (!context) return updateStockSourceList(state,arg1,arg2,arg3);
    if (state->state==0) {
        s32 closeResult=resetScanner(context);
        context->sourceContext=state->sourceContext;
        result=updateStockSourceList(state,arg1,arg2,arg3);
        if (closeResult) FsDiag_Record("source.directory.reset",closeResult,0,0,0);
        return result;
    }
    if (context->phase==NDS_SCAN_PHYSICAL) {
        result=updateStockSourceList(state,arg1,arg2,arg3);
        if (state->state==3) beginDirectoryScan(context,state);
        return result;
    }
    if (context->phase==NDS_SCAN_DIRECTORY) {
        updateStockLoadingUi(state,arg1,arg2,arg3);
        scanOneEntry(context,state);
        return 0;
    }
    if (context->phase==NDS_SCAN_VALIDATION) {
        updateStockLoadingUi(state,arg1,arg2,arg3);
        finishCandidateValidation(context,state);
        return 0;
    }
    if (context->phase==NDS_SCAN_PASSTHROUGH)
        return updateStockSourceList(state,arg1,arg2,arg3);
    context->backend=NDS_BACKEND_CARD;
    return updateStockSourceList(state,arg1,arg2,arg3);
}

/* Observe concurrently with the native confirmation/fade. Only the native
   fade-completion call waits for the snapshot, not the entire UI update. */
/* 文件观察与原确认／淡出并行；仅淡出完成边界等待快照，不阻塞整次界面更新。 */
u32 NdsSources_SelectionUpdate(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && state->state==0) {
        NativeTransport_Cancel((uintptr_t)context);
        context->selectionSourceId=0;
        context->selectionReady=0;
        context->selectedSaveValid=0;
        context->settlementReady=0;
        context->validatedWork.present=0;
    }
    if (context && state->state==9 && state->cursor<state->sourceCount &&
        state->sourceCount<=NDS_SOURCE_LIST_CAPACITY) {
        u32 sourceId=state->sourceIds[state->cursor];
        if (sourceId>=1 && sourceId<=NDS_GAME_COUNT &&
            context->winners[sourceId-1].kind==NDS_WINNER_SD) {
            if (context->selectionSourceId!=sourceId) {
                NativeTransport_Cancel((uintptr_t)context);
                NdsSourceWinner *winner=&context->winners[sourceId-1];
                context->selectionSourceId=sourceId;
                context->selectionReady=0;
                context->selectedSaveValid=0;
                context->settlementReady=0;
                context->validatedWork.present=0;
                context->activeGameCode=winner->gameCode;
                context->activePathSize=winner->savePathSize;
                if (winner->savePathSize>sizeof(context->path)) {
                    context->activePathSize=0;
                    context->selectionReady=1;
                } else memcpy(context->path,winner->savePath,winner->savePathSize);
            }
            if (!context->selectionReady) {
                NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
                if (scope.status==FS_ASYNC_BUSY) goto resume_native;
                FsSource source={FS_TRANSACTION_NDS_FILE,context->activeGameCode,
                    context->path,context->activePathSize,*CombinePatch_ModeStorage,0,0};
                FsAsyncDelivery delivery;
                int received=NativeTransport_PollSource((uintptr_t)context,0x500u,
                    FS_SERVICE_OBSERVE_SOURCE,&source,0,&delivery);
                if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY)
                    goto resume_native;
                FsTransactionFingerprint observed={0};
                int valid=received==FS_ASYNC_READY && delivery.result==FS_OK &&
                    delivery.reply.output.fingerprint.present &&
                    delivery.reply.output.fingerprint.size==NDS_SAVE_SIZE;
                if (valid) observed=delivery.reply.output.fingerprint;
                if (received==FS_ASYNC_READY) {
                    int released=NativeTransport_Release(&delivery);
                    if (released==FS_ASYNC_BUSY) goto resume_native;
                    if (released!=FS_ASYNC_ACCEPTED) valid=0;
                }
                context->selectedSave=observed;
                context->selectedSaveValid=(u32)valid;
                context->selectionReady=1;
                /* A failed observation never authorizes staging. Preserve the
                   existing native load/failure route, not a fabricated success. */
                /* 观察失败不能授权暂存；保留既有原载入／失败路线，不伪造成功。 */
            }
        }
    }
resume_native: ;
    u32 result=NdsSources_OriginalSelectionUpdate(state,arg1,arg2,arg3);
    if (result==MOVER_STATE_UPDATE_FINISHED && context)
        NativeTransport_Cancel((uintptr_t)context);
    return result;
}

/* A completed failure also releases the native load/error route, but never
   grants a valid source fingerprint. Physical and VC choices are unaffected. */
/* 失败完成也交还原载入／错误路线，但不授予有效摘要；实体卡带与 VC 不变。 */
u32 NdsSources_SelectionReady(MoverSourceListStateView *state)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && state && state->cursor<state->sourceCount &&
        state->sourceCount<=NDS_SOURCE_LIST_CAPACITY) {
        u32 sourceId=state->sourceIds[state->cursor];
        if (sourceId>=1 && sourceId<=NDS_GAME_COUNT &&
            context->winners[sourceId-1].kind==NDS_WINNER_SD)
            return context->selectionSourceId==sourceId && context->selectionReady;
    }
    return 1;
}

__attribute__((used,noinline,section(".text.nds_sources.02_select")))
u32 NdsSources_SelectListId(u32 sourceId)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (!context || sourceId<1 || sourceId>NDS_GAME_COUNT) {
        if (context) {
            context->backend=NDS_BACKEND_CARD;
            context->activeGameCode=sourceId;
            context->selectedSaveValid=0;
        }
        return sourceId;
    }
    if (context->winners[sourceId-1].kind==NDS_WINNER_SD) {
        if (context->selectionSourceId!=sourceId || !context->selectionReady) {
            NdsSourceWinner *winner=&context->winners[sourceId-1];
            context->activeGameCode=winner->gameCode;
            context->activePathSize=winner->savePathSize;
            if (winner->savePathSize<=sizeof(context->path))
                memcpy(context->path,winner->savePath,winner->savePathSize);
            context->selectedSaveValid=0;
        }
        context->backend=NDS_BACKEND_SD;
    }
    else {
        context->backend=NDS_BACKEND_CARD;
        context->activeGameCode=context->winners[sourceId-1].gameCode;
        context->selectedSaveValid=0;
    }
    return sourceId;
}

__attribute__((used,noinline,section(".text.nds_sources.03_read")))
s32 NdsSources_ReadSave(u32 device,u32 offset,void *data,u32 size)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && context->backend==NDS_BACKEND_SD &&
        (context->workActive || (context->phase==NDS_SCAN_PASSTHROUGH && context->selectedSaveValid))) {
        NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
        if (scope.status!=FS_ASYNC_ACCEPTED) return -1;
        if (!FsLocalSource_Contains(localSource(),context->path,context->activePathSize,
            context->activeGameCode)) return -1;
        return FsLocalSource_Read(localSource(),offset,data,size)==FS_OK?0:-1;
    }
    if (context && context->backend==NDS_BACKEND_SD)
        return readFileRangeUtf16(context->path,context->activePathSize,offset,data,size);
    return NdsSources_OriginalReadSave(device,offset,data,size);
}

__attribute__((used,noinline,section(".text.nds_sources.04_code")))
s32 NdsSources_ReadGameCode(u32 *gameCode)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && context->backend==NDS_BACKEND_SD) {
        *gameCode=context->activeGameCode;
        return 0;
    }
    s32 result=NdsSources_OriginalReadGameCode(gameCode);
    if (context && context->presencePending) {
        context->presencePending=0;
        if (result<0 || gameCodeIndex(*gameCode)<0)
            context->presenceFound=(u32)probeSdPresence(context);
    }
    return result;
}

__attribute__((used,noinline,section(".text.nds_sources.05_write")))
s32 NdsSources_WriteSave(u32 device,u32 offset,const void *data,u32 size)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && context->backend==NDS_BACKEND_SD) {
        s32 result;
        NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(1)};
        if (scope.status!=FS_ASYNC_ACCEPTED) { context->workError=1; return -1; }
        if (!context->workActive || context->workError || offset>NDS_SAVE_SIZE ||
            size>NDS_SAVE_SIZE-offset) {
            context->workError=1;
            return -1;
        }
        result=FsLocalSource_Write(localSource(),offset,data,size)==FS_OK?0:-1;
        if (result) context->workError=1;
        else context->workBytesWritten+=size;
        return result;
    }
    return NdsSources_OriginalWriteSave(device,offset,data,size);
}

int NdsSources_GetSelectedSource(FsSource *source)
{
    if (!source) return 0;
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    *source=selectedSource(context);
    source->mode=0;
    /* Borrowed UI views are copied by PollStage before worker wake. No file
       operation or server decision belongs in this source getter. */
    /* 借用的界面视图在 PollStage 唤醒后台前复制；此访问器不执行文件操作或服务端决定。 */
    return source->kind!=FS_TRANSACTION_NDS_FILE || source->expected!=0;
}

static int validateWorkingSave(NdsScannerContext *context,MoverStateView *state,
    const FsTransactionFingerprint *fingerprint)
{
    MoverNdsImageView *source=(MoverNdsImageView *)state->bankdataFlow;
    MoverSourceTransactionView transaction;
    u8 hash[FS_TRANSACTION_HASH_SIZE];
    u32 i;
    int face;
    if (!source || !source->saveImage || context->workError || !context->workBytesWritten ||
        !fingerprint->present || fingerprint->size!=NDS_SAVE_SIZE) return 0;
    face=MoverNds_ValidateSave((u8)(context->activeGameCode>>16)=='E' ||
        (u8)(context->activeGameCode>>16)=='D',source->saveImage);
    if (face!=0 && face!=1) return 0;
    MoverNds_SelectSaveFace(source,(u32)face);
    /* Whole-save CRCs alone cannot prove that the intended slot/token edits
       reached the file. Compare those native views with the pre-job plan. */
    /* 整体存档 CRC 不足以证明预期的槽位／令牌改动已落盘，还须核对作业前的原生视图。 */
    FsTransaction_Hash(MoverNds_GetBox1(source),30u*0x88u,hash);
    for (i=0;i<sizeof(hash);i++) if (hash[i]!=context->plannedBoxHash[i]) return 0;
    MoverSource_GetTransaction(source,&transaction);
    FsTransaction_Hash(&transaction,sizeof(transaction),hash);
    for (i=0;i<sizeof(hash);i++) if (hash[i]!=context->plannedTransactionHash[i]) return 0;
    return 1;
}

void NdsSources_SaveBegin(MoverStateView *state,u32 kind)
{
    NdsScannerContext *context=scannerContext();
    if (!context) {
        state->saveKind=0;
        state->savePending=0;
        return;
    }
    context->saveState=state;
    NativeTransport_Cancel((uintptr_t)context);
    context->saveStep=NDS_SAVE_PREPARE;
    context->pendingSaveKind=kind;
    context->nativeSaveResult=0;
    context->settlementReady=0;
    context->saveResult=0;
    context->validatedWork.present=0;
    context->saveBlocked=0;
    context->publicationError=0;
    context->workActive=0;
    context->workError=0;
    context->workBytesWritten=0;
    if (kind!=2 || context->backend!=NDS_BACKEND_SD) {
        context->saveStep=NDS_SAVE_NATIVE;
        NdsSources_OriginalSaveBegin(state,kind);
        return;
    }
    /* Local file preparation runs on the existing worker, without opening a
       server. Physical/VC/settings saves keep their original SDK path. */
    /* 本地文件准备使用既有后台，不构造服务器；卡带／VC／设置仍走原 SDK。 */
    if (kind==2 && context->backend==NDS_BACKEND_SD) {
        MoverSourceTransactionView transaction;
        MoverNdsImageView *source=(MoverNdsImageView *)state->bankdataFlow;
        if (!source || !source->saveImage) goto blocked;
        FsTransaction_Hash(MoverNds_GetBox1(source),30u*0x88u,context->plannedBoxHash);
        MoverSource_GetTransaction(source,&transaction);
        FsTransaction_Hash(&transaction,sizeof(transaction),context->plannedTransactionHash);
    }
    return;
blocked:
    state->saveKind=0;
    state->savePending=0;
    context->workActive=0;
    context->saveBlocked=1;
    context->saveStep=NDS_SAVE_DONE;
}

u32 NdsSources_SavePoll(MoverStateView *state)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return 0;
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    FsAsyncDelivery delivery;
    u32 result;
    if (scope.status!=FS_ASYNC_ACCEPTED) return 2;
    if (!context || context->saveState!=state) return NdsSources_OriginalSavePoll(state);
    if (context->saveBlocked) return 2;
    if (context->saveResult) return context->saveResult;
    if (context->pendingSaveKind!=2 || context->backend!=NDS_BACKEND_SD)
        return NdsSources_OriginalSavePoll(state);
    FsSource selected=selectedSource(context);
    if (context->saveStep==NDS_SAVE_PREPARE) {
        int received=NativeTransport_PollSource((uintptr_t)context,1,FS_SERVICE_BEGIN_SOURCE_SAVE,
            &selected,0,&delivery);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
        if (received!=FS_ASYNC_READY) goto blocked;
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return 0;
        if (released!=FS_ASYNC_ACCEPTED || delivery.result!=FS_OK) goto blocked;
        context->workActive=context->pendingSaveKind==2 && context->backend==NDS_BACKEND_SD;
        context->saveStep=NDS_SAVE_NATIVE;
        NdsSources_OriginalSaveBegin(state,context->pendingSaveKind);
        return 0;
    }
    if (context->saveStep==NDS_SAVE_NATIVE) {
        result=NdsSources_OriginalSavePoll(state);
        if (!result) return 0;
        context->nativeSaveResult=result;
        if (result!=1) {
            /* Keep the native failure branch; a failed save is not a publication decision. */
            /* 保留原版保存失败分支；保存报错本身不等于文件发布失败。 */
            context->workActive=0;
            FsLocalSource_Discard(localSource());
            context->selectedSaveValid=0;
            context->saveResult=result;
            context->saveStep=NDS_SAVE_DONE;
            return result;
        }
        context->saveStep=context->workActive?NDS_SAVE_READ_IMAGE:NDS_SAVE_PUBLISH;
    }
    if (context->saveStep==NDS_SAVE_READ_IMAGE) {
        FsServiceRequest input={.operation=FS_SERVICE_READ_SOURCE_IMAGE};
        input.input.file.size=NDS_SAVE_SIZE;
        int received=NativeTransport_PollRequest((uintptr_t)context,2,&input,0,NDS_SAVE_SIZE,&delivery);
        if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
        if (received!=FS_ASYNC_READY) goto failed;
        MoverNdsImageView *source=(MoverNdsImageView *)state->bankdataFlow;
        int copied=delivery.result==FS_OK && delivery.reply.bodySize==NDS_SAVE_SIZE && delivery.body &&
            source && source->saveImage;
        if (copied) {
            memcpy(source->saveImage,delivery.body,NDS_SAVE_SIZE);
            context->validatedWork=delivery.reply.output.fingerprint;
        }
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return 0;
        if (released!=FS_ASYNC_ACCEPTED || !copied ||
            !validateWorkingSave(context,state,&context->validatedWork)) goto failed;
        context->saveStep=NDS_SAVE_PUBLISH;
    }
    if (context->saveStep!=NDS_SAVE_PUBLISH) goto failed;
    if (context->workActive) selected.expected=&context->validatedWork;
    int received=NativeTransport_PollSource((uintptr_t)context,3,FS_SERVICE_SOURCE_SAVE_RESULT,
        &selected,context->nativeSaveResult,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
    if (received!=FS_ASYNC_READY) goto failed;
    int released=NativeTransport_Release(&delivery);
    if (released==FS_ASYNC_BUSY) return 0;
    if (released!=FS_ASYNC_ACCEPTED || delivery.result!=FS_OK) goto failed;
    context->workActive=0;
    if (delivery.reply.output.fingerprint.present) {
        context->selectedSave=delivery.reply.output.fingerprint;
        context->selectedSaveValid=1;
    }
    context->saveStep=NDS_SAVE_DONE;
    context->saveResult=context->nativeSaveResult;
    return context->saveResult;
blocked:
    state->saveKind=0;
    state->savePending=0;
    context->saveBlocked=1;
    context->saveStep=NDS_SAVE_DONE;
    context->saveResult=2;
    return 2;
failed:
    context->saveStep=NDS_SAVE_DONE;
    context->workActive=0;
    context->publicationError=1;
    context->saveResult=2;
    return 2;
}

int NdsSources_PublicationFailed(void)
{
    const NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    return context && context->publicationError;
}

int NdsSources_SaveCompleted(void)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (!context || context->backend!=NDS_BACKEND_SD || context->settlementReady) return 1;
    /* Publication/Finish already confirmed this save. Completing a native
       state is not another remote receipt query or source-file read. */
    /* 发布／Finish 已确认本次保存，原状态结束不再查询服务收据或读取来源文件。 */
    if (context->saveStep==NDS_SAVE_DONE && context->saveResult==1 &&
        context->validatedWork.present==1) {
        context->selectedSave=context->validatedWork;
        context->selectedSaveValid=1;
    }
    if (!context->selectedSaveValid) return -1;
    context->settlementReady=1;
    return 1;
}
