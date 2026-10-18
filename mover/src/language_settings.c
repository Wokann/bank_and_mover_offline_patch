#include "language_settings.h"
#include "fakeServer/fake_server_client.h"
#include <stddef.h>

_Static_assert(sizeof(BankLocalSettingsFileView)==512,"native Bank record size");
_Static_assert(offsetof(BankLocalSettingsFileView,language)==0x28,"native Bank language offsets");

static struct {
    u32 languageId,useJapaneseKanji,pending;
    uintptr_t peer;
} synchronization __attribute__((section(".data.language_settings")))={0,0,0,0};
static uintptr_t startupPeer __attribute__((section(".data.language_settings")))=0;
enum { MOVER_LANGUAGE_STARTUP_REQUEST=0x405 };

static int validLocalRecord(const BankLocalSettingsFileView *record)
{
    return record->integrityMarker==BANK_LOCAL_RECORD_INTEGRITY_MARKER &&
        record->checksum==Crc16_Calculate(record->transaction,sizeof(record->transaction),0xffff);
}

static int languageIsSupported(u32 languageId)
{
    return languageId!=0 && languageId<=BANK_LANGUAGE_ID_MAX &&
        languageId!=BANK_LANGUAGE_ID_UNUSED;
}

void LanguageSettings_CancelStartup(uintptr_t peer)
{
    if (!peer || startupPeer!=peer) return;
    NativeTransport_Cancel(peer);
    startupPeer=0;
}

/* Wait before native heap/view construction. The worker reads Bank-native settings;
   release precedes changing the cached language and invoking native startup. */
/* 在原堆／界面构造前等待。后台只读 Bank 原档，释放结果后才更新缓存并调用原启动流程。 */
int LanguageSettings_PrepareStartup(uintptr_t peer,MoverGameDataLanguageView *gameData)
{
    if (!peer || !gameData) return 1;
    if (startupPeer!=peer) {
        LanguageSettings_CancelStartup(startupPeer);
        startupPeer=peer;
    }
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return 0;
    if (scope.status!=FS_ASYNC_ACCEPTED) {
        LanguageSettings_CancelStartup(peer);
        return 1;
    }
    FsServiceRequest request={.operation=FS_SERVICE_READ_BANK_NATIVE_SETTINGS};
    request.input.file.size=sizeof(BankLocalSettingsFileView);
    FsAsyncDelivery delivery;
    int status=NativeTransport_PollRequest(peer,MOVER_LANGUAGE_STARTUP_REQUEST,&request,0,
        sizeof(BankLocalSettingsFileView),&delivery);
    if (status==FS_ASYNC_ACCEPTED || status==FS_ASYNC_BUSY) return 0;
    u32 language=0,kanji=0;
    if (status==FS_ASYNC_READY) {
        if (delivery.result==FS_OK && delivery.body &&
            delivery.reply.bodySize==sizeof(BankLocalSettingsFileView) &&
            delivery.bodySize==sizeof(BankLocalSettingsFileView)) {
            const BankLocalSettingsFileView *record=delivery.body;
            if (validLocalRecord(record) && languageIsSupported(record->language.languageId) &&
                record->language.useJapaneseKanji<=1) {
                language=record->language.languageId;
                kanji=record->language.useJapaneseKanji!=0;
            }
        }
        int released=NativeTransport_Release(&delivery);
        if (released==FS_ASYNC_BUSY) return 0;
        if (released!=FS_ASYNC_ACCEPTED) language=0;
    }
    LanguageSettings_CancelStartup(peer);
    if (language) {
        gameData->languageId=language;
        gameData->useJapaneseKanji=(u8)kanji;
    }
    return 1;
}

/* Confirmation only applies the native selection and retains numerical values.
   Do not hold the language view open for I/O: the native title-return state
   starts its loading animation before it owns and polls the SD request. */
/* 确认仅应用原生选择并保留数值，不为 I/O 留住语言界面；原返回标题状态先
   启动转圈动画，再持有并轮询“更新 Bank 原档 → 同步 SD”的后台请求。 */
void LanguageSettings_StoreSelection(void *selection,u32 languageId,u32 useJapaneseKanji)
{
    LanguageSelection_SetSettings(selection,languageId,(u8)useJapaneseKanji);
    LanguageSettings_CancelSynchronization((void *)synchronization.peer);
    if (!languageIsSupported(languageId)) return;
    synchronization.languageId=languageId;
    synchronization.useJapaneseKanji=useJapaneseKanji!=0;
    synchronization.pending=1;
}

u32 LanguageSettings_PollSynchronization(void *state)
{
    if (!synchronization.pending) return 1;
    if (!state) return 1;
    if (!synchronization.peer) synchronization.peer=(uintptr_t)state;
    if (synchronization.peer!=(uintptr_t)state) return 1;
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if (scope.status==FS_ASYNC_BUSY) return 0;
    if (scope.status!=FS_ASYNC_ACCEPTED) {
        LanguageSettings_CancelSynchronization(state);
        return 1;
    }
    FsServiceRequest request={.operation=FS_SERVICE_SYNC_MOVER_LANGUAGE};
    request.input.language.languageId=synchronization.languageId;
    request.input.language.useJapaneseKanji=synchronization.useJapaneseKanji;
    FsAsyncDelivery delivery;
    int status=NativeTransport_PollRequest((uintptr_t)state,FS_SERVICE_SYNC_MOVER_LANGUAGE,
        &request,0,0,&delivery);
    if (status==FS_ASYNC_ACCEPTED || status==FS_ASYNC_BUSY) return 0;
    if (status!=FS_ASYNC_READY) {
        LanguageSettings_CancelSynchronization(state);
        return 1;
    }
    int released=NativeTransport_Release(&delivery);
    if (released==FS_ASYNC_BUSY) return 0;
    if (released!=FS_ASYNC_ACCEPTED) {
        LanguageSettings_CancelSynchronization(state);
        return 1;
    }
    /* Preserve the native return path for every service result. Original Mover
       has no language-save notification; do not replace title mode text. */
    /* 无论服务结果如何，均回到原返回流程；原版没有语言保存提示，不替换标题模式文字。 */
    synchronization.pending=0;
    synchronization.peer=0;
    return 1;
}

void LanguageSettings_CancelSynchronization(void *state)
{
    if (synchronization.peer && synchronization.peer!=(uintptr_t)state) return;
    if (synchronization.peer) NativeTransport_Cancel(synchronization.peer);
    synchronization.pending=0;
    synchronization.peer=0;
}
