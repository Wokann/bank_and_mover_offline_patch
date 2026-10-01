#include "local_ticket.h"
#include "local_mileage.h"

/* Local completion of the composite entitlement/optional-reward state. */
/* 在本地完成复合的使用权／可选奖励状态。 */

__attribute__((used,noinline,section(".text.offline")))
int OfflinePatch_OptionalRewardBypassUpdate(BankStateView *state)
{
    BankTicketSharedView *shared=(BankTicketSharedView *)state->sharedData;
    if (!shared) {
        state->phase=BANK_STATE_PHASE_ERROR;
        return BANK_STATE_UPDATE_FINISHED;
    }
    /* This state normally obtains a trusted current timestamp and entitlement
       values remotely. Supply the current console time for the stock local
       mileage calculation while retaining a 999-day offline entitlement. */
    /* 此状态原本会从远端取得可信当前时间和使用权数值。为原版本地里程计算提供
       主机当前时间，同时保留 999 天离线使用权。 */
    if (!LocalMileage_GetCurrentDate(shared->currentDate)) {
        state->phase=BANK_STATE_PHASE_ERROR;
        return BANK_STATE_UPDATE_FINISHED;
    }
    shared->entitlementDays=BANK_TICKET_ENTITLEMENT_DAYS;
    shared->entitlementHours=BANK_TICKET_ENTITLEMENT_DAYS*BANK_TICKET_HOURS_PER_DAY;
    shared->entitlementValid=1;
    shared->remoteRewardFlag=0;
    /* State 9 itself checks mode '5'. Existing mode '4' skips creation and
       reaches the feature menu; missing mode '5' runs native initialization. */
    /* state 9 自行检查模式“5”。既有模式“4”跳过创建并进入功能选单；
       缺失模式“5”则执行原版初始化。 */
    state->phase=BANK_STATE_PHASE_COMPLETE;
    return BANK_STATE_UPDATE_FINISHED;
}
