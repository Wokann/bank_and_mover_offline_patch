#include "server_validation.h"
#include "fs_system/console_settings.h"
#include <string.h>

/* Classify integrity and occupancy without modifying the source record.
   Decoded words are temporary values; the native continuation decrypts its
   own copy and retains every local transfer check. */
/* 只分类记录完整性与占用状态，不修改来源记录。解码字仅作为临时值；原版续接
   路径会解码自己的副本，并保留全部本地传送检查。 */
static u16 read16(const u8 *data)
{
    return (u16)((u16)data[0]|((u16)data[1]<<8));
}

static u32 read32(const u8 *data)
{
    return (u32)read16(data)|((u32)read16(data+2)<<16);
}

static void copyName(u16 *out,const u8 *data,u32 units)
{
    u32 i=0;
    for (;i<units && i+1<FS_MOVER_NAME_UNITS;i++) {
        u16 value=read16(data+2*i);
        if (!value || value==0xffff) break;
        out[i]=value;
    }
    out[i]=0;
}

static void gen5RecordCandidate(const MoverGen5RecordView *record,FsMoverCandidate *candidate,
    FsMoverDetails *details)
{
    u32 index;
    u32 seed=record->checksum;
    u32 checksum=0;
    u32 order=(record->pid>>MOVER_GEN5_BLOCK_ORDER_SHIFT)&
        MOVER_GEN5_BLOCK_ORDER_MASK;
    u32 speciesWord=MoverGen5_BlockOrderTable[
        order*MOVER_GEN5_BLOCK_ORDER_ROW_SIZE]/sizeof(u16);
    u16 species=0;
    u16 control=record->control;
    u16 decoded[MOVER_GEN5_BODY_WORD_COUNT];

    candidate->species=0;
    candidate->structurallyValid=0;
    candidate->excluded=(control&MOVER_GEN5_RECORD_EXCLUDED)!=0;
    if (candidate->excluded) return;
    for (index=0;index<MOVER_GEN5_BODY_WORD_COUNT;index++) {
        u16 value=record->body[index];
        if (!(control&MOVER_GEN5_BODY_DECODED)) {
            seed=seed*MOVER_GEN5_DECRYPT_MULTIPLIER+
                MOVER_GEN5_DECRYPT_INCREMENT;
            value^=(u16)(seed>>16);
        }
        checksum+=value;
        decoded[index]=value;
        if (index==speciesWord) species=value;
    }
    candidate->species=species;
    candidate->structurallyValid=(u16)checksum==record->checksum;
    if (!candidate->structurallyValid || !species || species>MOVER_GEN5_SPECIES_MAX) return;
    const u8 *row=MoverGen5_BlockOrderTable+order*MOVER_GEN5_BLOCK_ORDER_ROW_SIZE;
    const u8 *bytes=(const u8 *)decoded;
    const u8 *growth=bytes+row[0],*moves=bytes+row[1];
    const u8 *names=bytes+row[2],*trainer=bytes+row[3];
    details->source=FS_MOVER_GEN5;
    details->tid=read16(growth+4); details->sid=read16(growth+6);
    details->form=moves[0x18]>>3;
    details->level=(u8)Pokemon_CalculateLevel(species,details->form,read32(growth+8),0);
    details->shiny=(details->tid^details->sid^(u16)record->pid^(u16)(record->pid>>16))<8;
    details->language=growth[0x0f]; details->origin=names[0x17];
    /* Public PK5 field layout: 0x84 low 7 bits, 0x40 bit 0, after unshuffle. */
    /* 公开 PK5 格式：解重排后 0x84 低七位与 0x40 位零；只观察，不改写。 */
    details->metLevel=trainer[0x1c]&127; details->fateful=moves[0x18]&1;
    details->nicknamed=(u8)(read32(moves+0x10)>>31);
    for (index=0;index<4;index++) details->moves[index]=read16(moves+2*index);
    copyName(details->nickname,names,11); copyName(details->trainer,trainer,8);
}

static int observeSlot(uint32_t index,FsMoverCandidate *candidate,FsMoverDetails *details,void *user)
{
    MoverVcValidationStateView *converted=user;
    if (!converted || !converted->request || index>=MOVER_VALIDATION_SLOT_COUNT) return FS_INVALID;
    if (converted->request->format==MOVER_VALIDATION_GEN5) {
        gen5RecordCandidate((const MoverGen5RecordView *)converted->request->records[index],
            candidate,details);
    } else if (converted->request->format==MOVER_VALIDATION_VC) {
        MoverVcCandidateView *native=converted->candidates[index];
        candidate->species=0; candidate->structurallyValid=1; candidate->excluded=0;
        details->source=FS_MOVER_VC;
        if (native && native->record) {
            void *record=native->record;
            Pokemon_BeginAccess(record);
            const u8 *growth=Pokemon_GetGrowthBlock(record,0);
            const u8 *moves=Pokemon_GetMoveBlock(record,0);
            const u8 *trainer=Pokemon_GetTrainerBlock(record,0);
            const u8 *raw=((MoverPokemonAccessView *)record)->raw;
            candidate->excluded=(read16(raw+4)&MOVER_GEN5_RECORD_EXCLUDED)!=0;
            candidate->species=read16(growth);
            if (candidate->species && candidate->species<=255 && !candidate->excluded) {
                details->tid=read16(growth+4); details->sid=read16(growth+6);
                details->form=growth[0x15]>>3;
                details->level=(u8)Pokemon_CalculateLevel(candidate->species,details->form,read32(growth+8),0);
                u32 pid=read32(growth+0x10);
                details->shiny=(details->tid^details->sid^(u16)pid^(u16)(pid>>16))<16;
                details->language=trainer[0x33]; details->origin=trainer[0x2f];
                details->metLevel=trainer[0x2d]&127; details->fateful=growth[0x15]&1;
                details->nicknamed=(u8)(read32(moves+0x34)>>31);
                for (u32 j=0;j<4;j++) details->moves[j]=read16(moves+0x1a+2*j);
                copyName(details->nickname,moves,13); copyName(details->trainer,trainer,13);
            }
            Pokemon_EndAccess(record);
        }
    } else return FS_INVALID;
    return FS_OK;
}

/* Replace only the HTTP launch in the native candidate request. Its serialization,
   SDK job allocation and callback registration all remain native. Completion is
   deferred to the next update so the caller cannot overwrite a callback result. */
/* 只替换原候选请求中的 HTTP 启动；序列化、SDK 作业分配及回调注册均保留。
   下一次更新才交付响应，防止调用方覆盖回调结果。 */
__attribute__((used,noinline,section(".text.offline.04_validation")))
void ServerValidation_Start(u32 *accepted,void *client,NativeRemoteResult *job,
    const void *resource,void *payload,u32 flags)
{
    (void)resource; (void)flags;
    u8 valid=client && job && payload;
    if (valid) {
        FsServiceRequest input={.operation=FS_SERVICE_CHECK_MOVER};
        valid=NativeResponse_InitializeRuntime() &&
            NativeTransport_Accept((uintptr_t)job,(uintptr_t)job,&input)==FS_ASYNC_ACCEPTED;
    }
    MoverValidation_Result(accepted,&valid);
}

__attribute__((used,noinline,section(".text.offline.04_validation")))
void ServerValidation_Poll(MoverStateView *state)
{
    MoverVcValidationStateView *view=(MoverVcValidationStateView *)state;
    MoverValidationRequestView *request=view?view->request:0;
    MoverValidationClientView *client=request?request->client:0;
    NativeRemoteResult *job=client?client->job:0;
    if (!request || !request->initialized || !client || !client->busy || !job || job->state) return;
    u8 body[FS_MOVER_REPLY_SIZE],ok=0;
    uint32_t size=0;
    u32 result[3];
    u32 source=request->format==MOVER_VALIDATION_GEN5?FS_MOVER_GEN5:
        request->format==MOVER_VALIDATION_VC?FS_MOVER_VC:0;
    FsServiceRequest input={.operation=FS_SERVICE_CHECK_MOVER};
    input.input.checks.source=source;
    input.input.checks.count=request->count;
    FsAsyncDelivery delivery;
    int status=FS_IO_ERROR,received;
    if (request->count!=FS_MOVER_SLOT_COUNT || !source) goto complete;
    /* Submit owns the value snapshot before wake. Native accessors stay on
       UI; no Pokemon, SDK or selected-game pointer enters the worker. */
    /* 唤醒前自持值快照；原对象访问留在界面，不传递宝可梦／SDK／游戏选择指针。 */
    if (!NativeTransport_Submitted((uintptr_t)job,(uintptr_t)job)) {
        u32 region,country;
        ConsoleSettings_GetLocation(&region,&country);
        input.input.checks.region=region;input.input.checks.country=country;
        FsMoverSlotSnapshot *slots=MoverValidation_Allocate(FS_MOVER_SLOT_COUNT*sizeof(*slots));
        if (!slots) goto complete;
        memset(slots,0,FS_MOVER_SLOT_COUNT*sizeof(*slots));
        status=FS_OK;
        for (u32 i=0;i<FS_MOVER_SLOT_COUNT && status==FS_OK;i++)
            status=observeSlot(i,&slots[i].candidate,&slots[i].details,view);
        if (status==FS_OK)
            received=NativeTransport_Poll((uintptr_t)job,(uintptr_t)job,&input,
                slots,FS_MOVER_SLOT_COUNT*sizeof(*slots),&delivery);
        else received=FS_ASYNC_INVALID;
        MoverValidation_Free(slots);
    } else received=NativeTransport_Poll((uintptr_t)job,(uintptr_t)job,&input,0,
        FS_MOVER_SLOT_COUNT*sizeof(FsMoverSlotSnapshot),&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return;
    status=FS_IO_ERROR;
    if (received==FS_ASYNC_READY) {
        status=delivery.result;
        if (status==FS_OK && delivery.reply.bodySize==sizeof(body) && delivery.body &&
            delivery.bodySize>=sizeof(body)) {
            memcpy(body,delivery.body,sizeof(body));
            size=sizeof(body);
        } else if (status==FS_OK) status=FS_INVALID;
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return;
        if (released!=FS_ASYNC_ACCEPTED) status=FS_IO_ERROR;
    }
complete: ;
    NativeTransport_Cancel((uintptr_t)job);
    MoverValidationPayloadView *payload=client->payload;
    if (status==FS_OK && payload && payload->body &&
        MoverValidation_Append(payload,body,size)) {
        MoverValidationVectorView *vector=payload->body;
        ok=vector->begin && (u32)(vector->end-vector->begin)==size;
        if (ok) payload->httpStatus=200;
    }
    MoverValidation_Result(result,&ok);
    /* The SDK completion API invokes the original HTTP parser and consumer,
       releases the job and clears busy/pointers through the original callbacks. */
    /* SDK 完成接口调用原 HTTP 解析器和消费者；原回调清理忙标记、指针及作业。 */
    MoverValidation_Complete(job,2,result,1);
}

__attribute__((used,noinline,section(".text.offline.04_validation")))
void ServerValidation_UnbindClient(MoverValidationClientView *client)
{
    if (client && client->job) NativeTransport_Cancel((uintptr_t)client->job);
    ServerValidation_OriginalUnbindClient(client);
}
