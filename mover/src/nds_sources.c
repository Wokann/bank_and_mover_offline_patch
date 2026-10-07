#include "nds_sources.h"

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

static int gameIndex(const u8 header[NDS_ROM_HEADER_READ_SIZE])
{
    if (header[0x0C]!='I' || header[0x0D]!='R' || languageRank(header[0x0F])<0)
        return -1;
    if (header[0x0E]=='B') return 0;
    if (header[0x0E]=='A') return 1;
    if (header[0x0E]=='E') return 2;
    if (header[0x0E]=='D') return 3;
    return -1;
}

static u32 readGameCode(const u8 header[NDS_ROM_HEADER_READ_SIZE])
{
    return (u32)header[0x0C]|((u32)header[0x0D]<<8)|
        ((u32)header[0x0E]<<16)|((u32)header[0x0F]<<24);
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

static void abortDirectoryScan(NdsScannerContext *context,MoverSourceListStateView *state)
{
    (void)closeScanDirectory(context);
    context->backend=NDS_BACKEND_CARD;
    context->phase=NDS_SCAN_PASSTHROUGH;
    state->sourceCount=0;
    state->state=6;
}

static void emitNdsSources(NdsScannerContext *context,MoverSourceListStateView *state)
{
    u32 index;
    if (closeScanDirectory(context)) {
        abortDirectoryScan(context,state);
        return;
    }
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
        if (resultIsNotFound(result)) emitNdsSources(context,state);
        else abortDirectoryScan(context,state);
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
        abortDirectoryScan(context,state);
        return;
    }
    if (entriesRead==0) {
        emitNdsSources(context,state);
        return;
    }
    if (!isNdsFile(&context->entry,&nameLength) ||
        !buildPath(context->path,ndsRomPrefix,context->entry.name,nameLength,0,&pathSize) ||
        readFileRangeUtf16(context->path,pathSize,0,context->header,sizeof(context->header)))
        return;
    index=gameIndex(context->header);
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

__attribute__((used,noinline,section(".text.nds_sources.01_list")))
u32 NdsSources_ListUpdate(MoverSourceListStateView *state,u32 arg1,u32 arg2,s32 arg3)
{
    NdsScannerContext *context;
    u32 result;
    context=scannerContext();
    if (!context) return MoverSourceList_Update(state,arg1,arg2,arg3);
    if (state->state==0) {
        s32 closeResult=resetScanner(context);
        result=MoverSourceList_Update(state,arg1,arg2,arg3);
        if (closeResult) {
            abortDirectoryScan(context,state);
            return 0;
        }
        return result;
    }
    if (context->phase==NDS_SCAN_PHYSICAL) {
        result=MoverSourceList_Update(state,arg1,arg2,arg3);
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
        return MoverSourceList_Update(state,arg1,arg2,arg3);
    context->backend=NDS_BACKEND_CARD;
    return MoverSourceList_Update(state,arg1,arg2,arg3);
}

__attribute__((used,noinline,section(".text.nds_sources.02_select")))
u32 NdsSources_SelectListId(u32 sourceId)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (!context || sourceId<1 || sourceId>NDS_GAME_COUNT) {
        if (context) context->backend=NDS_BACKEND_CARD;
        return sourceId;
    }
    if (context->winners[sourceId-1].kind==NDS_WINNER_SD) {
        NdsSourceWinner *winner=&context->winners[sourceId-1];
        context->activeGameCode=winner->gameCode;
        context->activePathSize=winner->savePathSize;
        MoverMemory_Copy(context->path,winner->savePath,winner->savePathSize);
        context->backend=NDS_BACKEND_SD;
    }
    else context->backend=NDS_BACKEND_CARD;
    return sourceId;
}

__attribute__((used,noinline,section(".text.nds_sources.03_read")))
s32 NdsSources_ReadSave(u32 device,u32 offset,void *data,u32 size)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
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
    return NdsSources_OriginalReadGameCode(gameCode);
}

__attribute__((used,noinline,section(".text.nds_sources.05_write")))
s32 NdsSources_WriteSave(u32 device,u32 offset,const void *data,u32 size)
{
    NdsScannerContext *context=*NDS_SCANNER_CONTEXT_SLOT;
    if (context && context->backend==NDS_BACKEND_SD)
        return writeFileRangeUtf16(context->path,context->activePathSize,offset,data,size);
    return NdsSources_OriginalWriteSave(device,offset,data,size);
}
