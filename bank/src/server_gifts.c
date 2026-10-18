#include "server_gifts.h"
#include "system_time.h"
#include "fs_system/console_settings.h"
#include "fakeServer/fake_server_client.h"
#include "fakeServer/ticket.h"
#include <string.h>

int ServerGifts_Request(BankBossResourceView *boss,u32 flags,const char *address,u32 option)
{
    (void)flags; (void)option;
    return boss && boss->allocator && boss->body && address?0:(int)0x80000000u;
}

static int receive(BankGiftStateView *state,BankBossResourceView *boss)
{
    BankGiftObserverView *observer=(BankGiftObserverView *)&state->resourceObserver;
    int registered=0;
    for (u32 i=0;i<16;i++) if (boss && boss->observers[i]==observer) registered=1;
    if (!registered) return 1;
    BankGiftReplyView reply;
    for (u32 i=0;i<sizeof(reply.reserved00);i++)
        ((volatile u8 *)reply.reserved00)[i]=0;
    reply.requestId=state->requestId;
    BankGift_ReceiveFn consume=(BankGift_ReceiveFn)observer->vtable[4];
    FsServiceRequest input;
    if (NativeTransport_Request((uintptr_t)state,1,&input)==FS_ASYNC_ACCEPTED) goto poll;
    input=(FsServiceRequest){.operation=FS_SERVICE_GIFT_REPLY};
    BankSelectedGameView *game=state->flow?BankGame_GetSelected(state->flow):0;
    if (!game || game->version<1 || game->version>8) goto failure;
    BankRuntimeObjectView *bank=state->flow->bankObject;
    if (!bank || bank->vtable!=BankFile_Vtable) goto failure;
    FsGiftRequest request;
    for (u32 i=0;i<sizeof(request);i++) ((u8 *)&request)[i]=0;
    u32 date[2];
    if (!SystemTime_GetCurrentDate(date)) {
        goto failure;
    }
    request.game=game->version-1;
    request.nativeContentIds[0]=bank->record.header.remoteContentIds[0];
    request.nativeContentIds[1]=bank->record.header.remoteContentIds[1];
    const uint32_t packedDate[2]={date[0],date[1]};
    request.date=FsDate_FromPacked(packedDate);
    u32 region,country;
    ConsoleSettings_GetLocation(&region,&country);
    request.region=region; request.country=country;
    BankGiftGameLayout *layout=(BankGiftGameLayout *)game;
    if (request.game<=FS_GAME_AS) {
        BankGiftRecipientView *recipient=&layout->gen6.link;
        BankGift_GetLinkFn getLink=(BankGift_GetLinkFn)recipient->vtable[2];
        const u8 *link=getLink(recipient);
        /* Same native Poké Link gate as the preceding eligibility state. */
        /* 与前一原生资格状态相同的宝可梦连接门槛。 */
        request.nativeReady=link && !link[0x200] && !link[0x201];
    } else request.nativeReady=1;
    input.input.gift.request=request;
    input.input.gift.pass=state->pass;
poll: ;
    FsAsyncDelivery delivery;
    if (!boss->body) goto failure;
    int received=NativeTransport_PollRequest((uintptr_t)state,1,&input,0,FS_GIFT_MAX_SIZE,&delivery);
    if (received==FS_ASYNC_ACCEPTED || received==FS_ASYNC_BUSY) return 0;
    if (received!=FS_ASYNC_READY) goto failure;
    uint32_t length=delivery.reply.bodySize;
    int copied=delivery.result==FS_OK && length && length<=FS_GIFT_MAX_SIZE &&
        length<=delivery.bodySize && delivery.body;
    if (copied) memcpy(boss->body,delivery.body,length);
    int released=NativeTransport_Release(&delivery);
    if (released==FS_ASYNC_BUSY) return 0;
    if (released!=FS_ASYNC_ACCEPTED || !copied) goto failure;
    consume((BankGiftStateView *)observer,&reply,length,boss->body);
    return 1;
failure:
    /* Original observer thunk owns the failure state just as it owns success. */
    /* 失败状态与成功状态均由原观察者跳板及接收器决定。 */
    consume((BankGiftStateView *)observer,0,0,0);
    return 1;
}

int ServerGifts_Update(BankGiftStateView *state)
{
    if (!state->substate) NativeTransport_Cancel((uintptr_t)state);
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return BANK_STATE_UPDATE_WAITING;
    if (state->substate==14 && !receive(state,*BankGift_BossStorage)) return BANK_STATE_UPDATE_WAITING;
    int result=ServerGifts_OriginalUpdate(state);
    if (result==BANK_STATE_UPDATE_FINISHED) NativeTransport_Cancel((uintptr_t)state);
    return result;
}
