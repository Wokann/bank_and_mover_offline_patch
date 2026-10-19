.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

// 0: require native online tickets; 1: use the server API only for a confirmed
// missing Azahar ticket interface, with a read-only fixed free window.
// 0：联网必须走真实票务；1：仅确认 Azahar 缺票务接口时自动使用伪服务器。
// 联网回退固定长期免费且不提供模拟购买；离线票务才读取本地配置。
.definelabel ONLINE_FAKE_SERVER_TICKET_FALLBACK, 1

// Local EC workers parse SD configuration on their original SDK thread. The
// native 4 KiB stack cannot hold the configuration/word-pattern call chain; keep
// page-aligned headroom without changing native online workers or ownership.
// 本地 EC 线程读取 SD 配置，原 4 KiB 栈不足；按整页留余量，不改变真实联网线程或生命周期。
.definelabel LOCAL_TICKET_EC_STACK_SIZE, 0x10000

// Offline behavior is the baseline. Mode wrappers branch to these native entry
// points when Download, Unlock or Original Mode was selected on the title screen.
// 离线行为作为基线；标题界面选定下载、解锁或原版模式后，模式包装函数会跳转到这些
// 原版入口。
.definelabel OfflinePatch_VersionStorageSize, 0x40
.definelabel OfflinePatch_VersionStorageStart, TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CodeExpansion_PayloadStart, 0x003FC000
.definelabel CODE_EXPANSION_PAYLOAD_SIZE, 0x23A000
.definelabel FakeServer_ContextSize, 0x104000
.definelabel FakeServer_RuntimeSize, 0x216000
.definelabel CombinePatch_CodeStart, CodeExpansion_PayloadStart
.definelabel CombinePatch_CodeEnd, CodeExpansion_PayloadStart + CODE_EXPANSION_PAYLOAD_SIZE - FakeServer_RuntimeSize
.definelabel CodeExpansion_ExecutableSize, CombinePatch_CodeEnd - CombinePatch_CodeStart
.definelabel OfflinePatch_RuntimeStorageStart, ReadWriteMappedEnd - 0x10
.definelabel OfflinePatch_RuntimeStorageEnd, ReadWriteMappedEnd
.definelabel RuntimeEnvironment_StorageStart, OfflinePatch_RuntimeStorageStart - 0x10
.definelabel RuntimeEnvironment_StorageEnd, OfflinePatch_RuntimeStorageStart
.definelabel CombinePatch_ModeStorage, OfflinePatch_RuntimeStorageStart
.definelabel CombinePatch_HidManager, 0x003DC3B4
.definelabel CombinePatch_ModeOffline, 0
.definelabel CombinePatch_ModeDownload, 1
.definelabel CombinePatch_ModeUnlock, 2
.definelabel CombinePatch_ModeOriginal, 3
.definelabel CombinePatch_ModeCount, 4
.definelabel CombinePatch_KeyR, 0x100
.definelabel CombinePatch_KeyL, 0x200
.definelabel CombinePatch_SessionModeOffset, 0
.definelabel CombinePatch_DownloadCaptureStatusOffset, 1
.definelabel DOWNLOAD_CAPTURE_NONE, 0
.definelabel DOWNLOAD_CAPTURE_COMPLETE, 1
.definelabel DOWNLOAD_CAPTURE_FAILED, 2
.definelabel DOWNLOAD_CAPTURE_PENDING, 3
.definelabel CombinePatch_TitleRPreviousOffset, 2
.definelabel CombinePatch_TitleSelectedModeOffset, 3
.definelabel CombinePatch_InitialLanguagePendingOffset, 5
.definelabel CombinePatch_TitleLanguageOffset, 7
.definelabel CombinePatch_TitleLanguageRequested, 1
.definelabel CombinePatch_SettingsReady, 0
.definelabel CombinePatch_SettingsMissing, 1
.definelabel CombinePatch_SettingsFailed, 2
.definelabel CombinePatch_UnlockRecoveryOffset, UnlockMode_RecoveryStorage - CombinePatch_ModeStorage
.definelabel CombinePatch_BlankMessage, 0x60
.definelabel CombinePatch_DownloadProgressMessage, 0x61
.definelabel CombinePatch_DownloadSuccessMessage, 0x62
.definelabel CombinePatch_DownloadUseBankMessage, 0x63
.definelabel CombinePatch_OfflineInitialConnectMessage, 0x64
.definelabel CombinePatch_OfflineBankConnectMessage, 0x65
.definelabel CombinePatch_OfflineSaveMessage, 0x66
.definelabel CombinePatch_OfflineDisconnectMessage, 0x67
.definelabel CombinePatch_TitleModeOfflineMessage, 0x68
.definelabel CombinePatch_TitleModeDownloadMessage, 0x69
.definelabel CombinePatch_DisabledDownloadMoverMessage, 0x7C
.definelabel CombinePatch_DisabledInstalledMoverMessage, 0x7D
.definelabel CombinePatch_DisabledHomeMessage, 0x7E
.definelabel CombinePatch_OfflineMenuGreeting, 0x1B
.definelabel CombinePatch_DownloadMenuGreeting, 0x1C
.definelabel CombinePatch_UnlockMenuGreeting, 0x1D
.definelabel CombinePatch_TitleModeUnlockMessage, 0x6C
.definelabel CombinePatch_UnlockUseBankMessage, 0x6D
.definelabel CombinePatch_UnlockGameSelectionPrompt, 0x6E
.definelabel CombinePatch_UnlockChallengePrompt, 0x6F
.definelabel CombinePatch_NoGameBlockedMessage, 0x70
.definelabel CombinePatch_SupportReferencePrompt, 0x71
.definelabel CombinePatch_TitleModeOriginalMessage, 0x72
.definelabel CombinePatch_DownloadNoGameMessage, 0x73
.definelabel CombinePatch_DownloadFailedMessage, 0x74
.definelabel CombinePatch_DownloadStartMessage, 0x75
.definelabel CombinePatch_TitleLanguageMessage, 122
.definelabel CombinePatch_TitleLanguagePane, 0x29
.definelabel CombinePatch_TitleModePane, 0x2A

.open INPUT_CODE, OUTPUT_CODE, 0x00100000

// Enable the added pages before the original PREL32 constructor walker runs.
// 原版 PREL32 构造函数遍历前启用新增页。
.org BankStartup_ConstructorCall
    bl CodeExpansion_Startup

// Keep native product selection, polling and purchase-result consumption.
// Only the unavailable system-shop requests use local server adapters.
// 保留原券种选择、轮询和购买结果处理；仅转接不可离线执行的系统商店请求。
.org 0x002A1EE8
    bl CombinePatch_TicketShopRequest
.org 0x002A0560
    b CombinePatch_TicketCreditRequest
.org 0x002A2184
    bl CombinePatch_TicketPurchaseRequest
.org 0x002A1C20
    bl CombinePatch_TicketEcCatalog
.org 0x002A4604
    bl CombinePatch_TicketEcBalance
.org 0x002A214C
    bl CombinePatch_TicketEcFormat
.org 0x002A4660
    bl CombinePatch_TicketEcFormat
.org 0x002A2018
    bl CombinePatch_TicketEcWorkerConstruct
.org 0x002A0494
    bl CombinePatch_TicketEcWorkerConstruct
.org 0x002B110C
    bl CombinePatch_TicketPurchasePoll
.org 0x002B1248
    bl CombinePatch_TicketPurchasePoll

// Offline baseline hooks. Each site is retained so the combined project starts
// from the tested local-data behavior rather than duplicating a second patch.
// 离线基线钩子。保留每个位置，使合并工程从已测试的本地数据行为起步，而不是复制
// 第二份补丁。
.org NetworkConnection_AvailabilityCall
    bl CombinePatch_NetworkAvailability
.org NetworkConnectionState_Initialize + 0x54
    blne CombinePatch_ConnectionConstruct
.org DisconnectCleanupState_Initialize + 0x70
    blne CombinePatch_ConnectionConstruct
.org 0x002A43CC
    b CombinePatch_ConnectionPoll
.org 0x002A44A0
    b CombinePatch_ConnectionUnbind
.org NetworkMonitor_Update
    b CombinePatch_NetworkMonitorUpdate
.org NetworkMonitor_WorkerAccountLoad
    bl CombinePatch_NetworkMonitorWorkerAccount
.org NetworkConnection_MessageIdLoad
    bl CombinePatch_SelectInitialConnectionMessage
.org InitialRemoteRecordState_Update
    b CombinePatch_InitialRemoteRecordUpdate
.org BankRemote_RecordProvider
    b CombinePatch_RecordProvider
.org BankRemote_QueryProvider
    b CombinePatch_QueryProvider
.org 0x002A3710
    b CombinePatch_RemoteUnbind
.org BankInitialRecord_CampaignRequest
    bl CombinePatch_InitialCampaignRequest
.org InitialRemoteRecord_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org OptionalRewardState_TicketInitializeCall
    bl CombinePatch_TicketInitialize
.org TicketJob_Poll
    b CombinePatch_TicketJobPoll
.org 0x002A05F8
    b CombinePatch_TicketCreditPoll
.org 0x002A1E00
    bl CombinePatch_TicketExpiryRequest
.org 0x002A1F2C
    bl CombinePatch_TicketExpiryRequest
.org OptionalRewardState_TicketUiPrepare
    b CombinePatch_TicketUiPrepare
.org OptionalRewardState_TicketFadeBeginCall
    bl CombinePatch_TicketUiFade
.org OptionalRewardState_TicketFadeEndCall
    bl CombinePatch_TicketUiFade
.org OptionalRewardState_TicketUiCleanupCall
    bl CombinePatch_TicketUiCleanup
.org 0x002A0518
    bl CombinePatch_TicketProviderBind
.org 0x002A20E8
    bl CombinePatch_TicketProviderUnbind
.org 0x00169B00
    bl CombinePatch_TicketSend
.org 0x00169DE8
    bl CombinePatch_TicketSend
.org 0x001698E8
    bl CombinePatch_TicketSend
.org 0x00169C6C
    bl CombinePatch_TicketSend
.org 0x001699F8
    bl CombinePatch_TicketSend
.org BankDataSyncState_Update
    b CombinePatch_BankDataSyncDispatch
.org BankDataSyncState_Destruct
    b CombinePatch_BankDataSyncDestruct
.org BankDataSyncState_Initialize + 0x18
    b CombinePatch_BankDataSyncInitialize

.org TitleScreenState_Update
    b CombinePatch_TitleScreenUpdate
.org TitleScreenUi_Create + 0xA0
    bl CombinePatch_TitleModeTextInitialize
.org TitleScreenState_Exit
    b CombinePatch_TitleExit
.org TitleScreenState_Destruct
    b CombinePatch_TitleDestruct
.org StartupPreview_UpdateInitialize
    b CombinePatch_StartupInitializeGate
.org StartupPreview_Initialize
    b CombinePatch_StartupInitialize
.org StartupPreview_SettingsReady
    b CombinePatch_StartupSynchronizeLanguage
.org StartupPreview_Destruct
    b CombinePatch_StartupDestruct
.org TitleScreenState_ExitUiCleanup
    b CombinePatch_TitleReleaseWaitingView
.org TitleScreenState_ExitComplete
    b CombinePatch_TitleBindSession
.org BankFlow_SelectNextState + 0x100
    beq CombinePatch_TitleResult

.org PostSelectionConnectionState_Update
    b CombinePatch_PostSelectionConnectionUpdate
.org TransactionRecoveryState_Update
    b CombinePatch_TransactionRecoveryUpdate
.org TransactionRecovery_MismatchMessageIdLoad
    bl CombinePatch_SelectMismatchMessage
.org ForceRollbackState_Update
    b CombinePatch_LocalForceRollbackUpdate
.org BankFlow_RecoveryResult
    b CombinePatch_RecoveryResult
.org BankFlow_ForceRollbackSuccess
    beq CombinePatch_ForceRollbackSuccess
.org PostSelectionConnectionState_Initialize + 0x0C
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org BankCreateConnection_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org NetworkPreparation_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage

// Only Offline replaces BOSS gift retrieval. Native receiving, UI and paired
// saving remain stock; online modes retain both gift passes and their cleanup.
// 仅离线替换 BOSS 礼物获取；原生接收／界面／配对保存及联网两轮查询均保留。
.org RewardReceiveState_Update
    b CombinePatch_GiftUpdate
.org RewardReceive_ConnectionMessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage
// Both mileage messages retain native mov r2,#14 and table selector r1=1.
// 两处里程对话保留原版 mov r2,#14 及文本表选择参数 r1=1，不安装连接文案钩子。
.org 0x002A9934
    bl CombinePatch_GiftResourceRequest
.org 0x002AAF50
    bl CombinePatch_GiftResourceRequest

.org DisconnectCleanupState_Update
    b CombinePatch_StorageNoticeDisconnectUpdate
.org 0x002ABDEC
    b CombinePatch_LanguageReturnDestruct
.org BankNoSaveRollbackState_Update
    b CombinePatch_NoSaveRollbackUpdate
.org BankSaveState_Update
    b CombinePatch_SaveStateUpdate
.org 0x001DF3E0
    b CombinePatch_RemoteBind
.org 0x002A26F8
    b CombinePatch_MetadataProvider
.org 0x002A25C4
    b CombinePatch_StageProvider
.org 0x002A3624
    b CombinePatch_DownloadProvider
.org 0x002A3954
    b CombinePatch_DownloadResponse
.org 0x002A2B24
    b CombinePatch_MetadataUpdateProvider
.org 0x001D5DF0
    b CombinePatch_CommitProvider
.org 0x001D5CA4
    b CombinePatch_RollbackProvider

.org BankCreateState_Update
    b CombinePatch_BankCreateUpdate
.org 0x0011491C
    b CombinePatch_AccountInitialize
.org 0x0022DDCC
    b CombinePatch_AccountUnbind
.org 0x00114F88
    bl CombinePatch_AccountRequestReady
.org 0x0012259C
    bl CombinePatch_AccountSend
.org 0x002A3868
    b CombinePatch_CreateProvider

.org BankFlow_SelectNextState + 0x248
    beq CombinePatch_RewardResult
.org BankFlow_SelectNextState + 0x170
    beq CombinePatch_HomeResult
.org BankFlow_SelectNextState + 0x15C
    beq CombinePatch_SelectUseBankState
.org BankFlow_SelectNextState + 0x3A0
    beq CombinePatch_HomeCleanupResult
.org InitialGameCheck_NoGameChoiceSetup
    b CombinePatch_NoGameChoice
.org InitialGameCheck_NoGameMessageLoad
    bl CombinePatch_SelectNoGameMessage
.org InitialGameCheck_NoGameChoiceSetup + 4
    bl CombinePatch_SelectNoGameDownloadButton
.org BankFlow_SelectNextState + 0x34C
    beq CombinePatch_Result21

// Download Mode must not enter reward, box, or save UI if these results are
// reached. Other modes retain their existing receipt and continuation rules.
// 下载模式即使到达这些结果也不进入领取、盒子或保存界面；其他模式保留各自的
// 领取与后续分支规则。
.org BankFlow_SelectNextState + 0x260
    beq CombinePatch_Result5
.org BankFlow_SelectNextState + 0x270
    beq CombinePatch_Result6Or12

// Disabled menu actions return before the native callback prologue, leaving
// the feature menu visible and interactive. Native state bodies stay intact.
// 禁用的菜单操作在原版回调序言前返回，选单保持显示和可操作；原版状态函数体完整保留。
.org BankMenuState_SelectionCallback
    b CombinePatch_MenuSelectionCallback

// Select a distinct upper-screen greeting and first menu label after the mode
// is latched. Both menu-return paths use the same greeting selector.
// 在模式锁定后选择不同的上屏说明与第一项菜单标签。两个返回菜单的路径共用
// 同一个说明选择器。
.org BankMenuState_ModeGreetingCall0
    bl CombinePatch_ShowModeGreeting
.org BankMenuState_ModeGreetingCall5
    bl CombinePatch_ShowModeGreeting
.org GameSelectionState_MessageIdLoad
    bl CombinePatch_SelectGameSelectionMessage
.org ForceRollbackState_ChallengePromptCall
    bl CombinePatch_ShowUnlockPrompt
.org 0x002A2838
    b CombinePatch_RecoveryTokenProvider
.org BankMenuUi_FirstLabelCall
    bl CombinePatch_SelectUseBankMenuText
.org BankMenuUi_FirstLabelCall + 0x08
    bl CombinePatch_SelectSupportMenuText
.org BankMenuUi_FirstLabelCall + 0x10
    bl CombinePatch_SelectInstalledMoverMenuText
.org BankMenuUi_FirstLabelCall + 0x18
    bl CombinePatch_SelectHomeMenuTextR6
.org BankMenuUi_FirstLabelCall + 0xA0
    bl CombinePatch_SelectDownloadMoverMenuText
.org BankMenuUi_FirstLabelCall + 0xA8
    bl CombinePatch_SelectHomeMenuTextR5

// Keep the original regional identity sources and encoder. Only observe their
// response validity and refresh the clickable support label after generation.
// 保留原区域身份来源及编码器，只观察响应有效性，并更新仍可点击的支持代码标签。
.org 0x002A6F78
    bl SupportCode_ReadFriendPrincipal
.org 0x002A704C
    bl SupportCode_ReadActPrincipal
.org 0x002A6F98
    bl CombinePatch_FormatSupportCode

// Capture only after the native current/legacy body loader has completed.
// All modes retain the callback's metadata and completion processing.
// 原版当前／旧格式数据装载完成后才捕获；所有模式保留回调的元数据和完成处理。
.org BankRemote_DownloadSuccessCallback + 0x98
    bl CombinePatch_DownloadCaptureTrampoline

.org ReturnToTitleState_Update + 0x168
    bl CombinePatch_SelectDisconnectMessage

.org BankSaveState_Initialize + 0x28
    bl CombinePatch_SelectSaveMessage

// Bank insertion, clearing and group moves request a new slot timestamp.
// Only Offline Mode replaces the native network-clock dependency.
// 银行槽写入、清空及批量移动需要新的槽位时间戳；仅离线模式替换原版联网时钟依赖。
.org BankSlot_WriteDateCall
    bl CombinePatch_SelectBankSlotDate
.org BankSlot_ClearDateCall
    bl CombinePatch_SelectBankSlotDate
.org BankSlots_MoveDateCall
    bl CombinePatch_SelectBankSlotDate

// Redirect only the Turtle record's storage backend. The stock wrappers still
// validate the object, set the loaded flag and map backend errors.
// 仅重定向 Turtle 记录的存储后端。原版包装函数仍负责对象校验、loaded 标志和
// 后端错误映射。
.org TurtleStorage_LoadBackendCall
    bl CombinePatch_TurtleLoad
.org TurtleStorage_SaveBackendCall
    bl CombinePatch_TurtleSave
.org Initial_TurtleStorageCheckCall
    b CombinePatch_InitialTurtleCheck
.org Initial_TurtleStorageLoadCall
    b CombinePatch_InitialTurtleLoad
.org InitialGameCheck_Destruct
    b CombinePatch_InitialTurtleDestruct
.org Initial_TurtleStorageFormatCall
    bl CombinePatch_TurtleFormat
.org Initial_TurtleStorageFormatPollCall
    bl CombinePatch_TurtleFormatPoll
.org StateLocalSave_Begin
    b CombinePatch_LocalSaveBegin
.org StateLocalSave_Poll
    b CombinePatch_LocalSavePoll

.org CombinePatch_CodeStart
.area CombinePatch_CodeEnd-CombinePatch_CodeStart

// The stock menu's greeting uses BMG line 17. The LayeredFS archive appends
// one explanation for each of the first three modes; Original keeps stock text.
// Select the explanation after the startup mode latch.
// 原版菜单说明使用 BMG 第 17 行。LayeredFS 为前三种模式各追加一条说明，原版模式
// 保留原文；在启动模式锁定后选择其中之一。
CombinePatch_ShowModeGreeting:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOriginal
    moveq r2,#0x11
    beq @@show
    cmp r12,#CombinePatch_ModeDownload
    moveq r2,#CombinePatch_DownloadMenuGreeting
    beq @@show
    cmp r12,#CombinePatch_ModeUnlock
    moveq r2,#CombinePatch_UnlockMenuGreeting
    movne r2,#CombinePatch_OfflineMenuGreeting
@@show:
    b BankUi_ShowMessageLine
    .pool

// Unlock Mode supplies combination-button guidance in game selection.
// Other modes retain stock message 3 and the original display call.
// 解锁模式在游戏选择中提供组合键说明；其他模式保留原消息 3 和原显示调用。
CombinePatch_SelectGameSelectionMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeUnlock
    moveq r1,#CombinePatch_UnlockGameSelectionPrompt
    movne r1,#3
    bx lr
    .pool

// This call is immediately followed by a conditional branch that relies on
// the caller's flags. Preserve those flags while replacing only its label ID.
// 该调用之后紧跟依赖调用者标志位的条件分支。这里只替换标签 ID，并保留原标志位。
CombinePatch_SelectUseBankMenuText:
    mrs r3,cpsr
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeDownload
    moveq r12,#CombinePatch_DownloadUseBankMessage
    beq @@selected
    cmp r12,#CombinePatch_ModeUnlock
    moveq r12,#CombinePatch_UnlockUseBankMessage
    movne r12,#0x29
@@selected:
    msr cpsr_f,r3
    mov r3,r12
    bx lr
    .pool

// Select only the labels deliberately replaced by the current patch modes.
// Other stock menu labels remain unchanged.
// 这里只选择当前补丁模式明确替换的标签；其余原版选单标签保持不变。
CombinePatch_SelectInstalledMoverMenuText:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r5,#0x2D
    movne r5,#CombinePatch_DisabledInstalledMoverMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

CombinePatch_SelectSupportMenuText:
    push {r0-r5,r12,lr}
    mrs r4,cpsr
    bl SupportCode_MenuMessage
    mov r8,r0
    msr cpsr_f,r4
    pop {r0-r5,r12,pc}

CombinePatch_FormatSupportCode:
    push {r0-r3,r4,lr}
    bl SupportCode_MenuMessage
    mov r3,r0
    ldr r0,[r4,#0x38]
    mov r1,#1
    mov r2,#0x24
    bl BankUi_SetMessageLine
    pop {r0-r3,r4,lr}
    b 0x002A4078

CombinePatch_SelectDownloadMoverMenuText:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r6,#0x2C
    movne r6,#CombinePatch_DisabledDownloadMoverMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

CombinePatch_SelectHomeMenuTextR6:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r6,#0x56
    movne r6,#CombinePatch_DisabledHomeMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

CombinePatch_SelectHomeMenuTextR5:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r5,#0x56
    movne r5,#CombinePatch_DisabledHomeMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

// The stock rollback state has already placed the challenge number in number
// register 0 when this wrapper runs. Unlock Mode additionally copies the first
// server candidate into number register 1, using the same modulo rule as the
// stock input validator, and then expands the appended three-line message.
// Empty candidate lists retain the original two-line prompt.
// 此包装函数运行时，原版回滚状态已把挑战码写入数字寄存器 0。解锁模式再按原版
// 输入验证使用的同一取模规则，把服务器第一个候选值写入数字寄存器 1，并展开追加的
// 三行消息。候选列表为空时仍显示原版两行提示。
CombinePatch_ShowUnlockPrompt:
    push {r4-r8,lr}
    sub sp,sp,#8
    mov r5,r0
    ldr r6,=CombinePatch_ModeStorage
    ldrb r6,[r6,#CombinePatch_SessionModeOffset]
    cmp r6,#CombinePatch_ModeUnlock
    beq @@candidate
    cmp r6,#CombinePatch_ModeOffline
    bne @@stock
@@candidate:
    ldr r0,[r4,#0x54]
    ldr r1,[r4,#0x58]
    mov r2,sp
    bl UnlockMode_TryGetFirstCandidateCode
    cmp r0,#0
    beq @@stock
    ldr r6,[sp]
    mov r0,#2
    mov r1,#1
    str r0,[sp]
    str r1,[sp,#4]
    ldr r0,[r5,#0x5C]
    mov r1,#1
    mov r2,r6
    mov r3,#8
    bl G2dUtil_SetRegisterNumber
    mov r0,r5
    mov r1,#CombinePatch_UnlockChallengePrompt
    bl FlowView_SetTextMessage
    b @@done
@@stock:
    mov r0,r5
    cmp r6,#CombinePatch_ModeOriginal
    moveq r1,#0x22
    movne r1,#CombinePatch_SupportReferencePrompt
    bl FlowView_SetTextMessage
@@done:
    add sp,sp,#8
    pop {r4-r8,pc}
    .pool

CombinePatch_SelectDisconnectMessage:
    ldrb r1,[r4,#0x3C]
    ldr r2,=CombinePatch_ModeStorage
    ldrb r3,[r2,#CombinePatch_SessionModeOffset]
    cmp r1,#0
    movne r1,#CombinePatch_BlankMessage
    bxne lr
    cmp r3,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_OfflineDisconnectMessage
    bxeq lr
    cmp r3,#CombinePatch_ModeDownload
    movne r1,#0x0D
    bxne lr
    ldrb r2,[r2,#CombinePatch_DownloadCaptureStatusOffset]
    mov r1,#0x0D
    cmp r2,#DOWNLOAD_CAPTURE_COMPLETE
    moveq r1,#CombinePatch_DownloadSuccessMessage
    cmp r2,#DOWNLOAD_CAPTURE_FAILED
    moveq r1,#CombinePatch_DownloadFailedMessage
    bx lr

// Every language choice saves native settings first, then synchronizes SD.
// Existing SD gets only language; missing SD gets the complete native record.
// 语言选择先保存主机记录再同步 SD；已有 SD 仅更新语言，缺失才完整复制，不按模式分流。
CombinePatch_DisconnectWithLanguageSave:
    push {r4,lr}
    mov r4,r0
    ldr r1,=CombinePatch_ModeStorage
    mov r2,#CombinePatch_TitleLanguageRequested
    strb r2,[r1,#CombinePatch_TitleLanguageOffset]
    bl LanguageSettings_Save
    cmp r0,#0
    beq @@pending
    cmp r0,#1
    bne @@failed
    b @@disconnect
@@pending:
    mov r0,#0
    pop {r4,pc}
@@failed:
    mov r1,#3
    strb r1,[r4,#0x30]
    mov r0,#1
    pop {r4,pc}
@@disconnect:
    mov r0,r4
    bl CombinePatch_DisconnectUpdateOfficial
    pop {r4,pc}
    .pool

CombinePatch_LanguageReturnDestruct:
    push {r0,lr}
    bl LanguageSettings_Cancel
    pop {r0,lr}
    ldr r1,=0x00361A90
    b 0x002ABDF0
    .pool



// C-owned paths and all feature payloads share the added pages.
// C 路径数据与全部功能荷载共同放在新增页内。
CombinePatch_PathDataBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/patch_paths.o"
CombinePatch_PathDataEnd:

.align 4

// Keep the selected mode when returning to the title; its zero-initialized
// byte defaults to Offline on a fresh launch. Reset only per-session flags.
// 返回标题时保留所选模式，首次启动时该字节的零初值表示离线；只重置会话标志。
CombinePatch_TitleModeTextInitialize:
    ldr r12,=CombinePatch_ModeStorage
    mov r1,#CombinePatch_ModeOffline
    strb r1,[r12,#CombinePatch_SessionModeOffset]
    strb r1,[r12,#CombinePatch_DownloadCaptureStatusOffset]
    strb r1,[r12,#CombinePatch_InitialLanguagePendingOffset]
    strb r1,[r12,#CombinePatch_TitleLanguageOffset]
    // Require release before accepting a key held across title creation.
    // 创建标题时仍按住的肩键须先松开，避免语言保存返回后再次进入。
    mov r1,#3
    strb r1,[r12,#CombinePatch_TitleRPreviousOffset]
    ldrb r3,[r12,#CombinePatch_TitleSelectedModeOffset]
    cmp r3,#CombinePatch_ModeDownload
    moveq r3,#CombinePatch_TitleModeDownloadMessage
    beq @@show
    cmp r3,#CombinePatch_ModeUnlock
    moveq r3,#CombinePatch_TitleModeUnlockMessage
    beq @@show
    cmp r3,#CombinePatch_ModeOriginal
    moveq r3,#CombinePatch_TitleModeOriginalMessage
    movne r3,#CombinePatch_TitleModeOfflineMessage
@@show:
    mov r1,#1
    mov r2,#0x10
    b BankdataRedirect_TitleInitialize
    .pool

// Reproduce the stock title-state update. L/R are sampled only while the title
// remains active. On the same title-exit frame produced by A or a touch input,
// the selected value is copied to the session value used by every later hook.
// 重现原版标题状态更新。只在标题仍处于活动状态时采样 L/R 键。A 键或触摸输入导致
// 标题退出的同一帧，会将所选值复制到之后各钩子使用的会话值。
CombinePatch_TitleScreenUpdate:
    push {r3,r4,r5,r6,r7,lr}
    mov r4,r0
    ldr r0,[r0,#0x10]
    cmp r0,#0
    beq @@initialize
    cmp r0,#1
    bne @@pending
    b @@process
@@initialize:
    ldr r5,[r4,#0x38]
    mov r6,#1
    mov r2,#0
    str r6,[sp]
    ldr r0,[r5,#0x5C]
    mov r3,r6
    mov r1,r2
    bl Ui_SetLayerVisible
    mov r3,#1
    str r6,[sp]
    ldr r0,[r5,#0x5C]
    mov r2,#0
    mov r1,r3
    bl Ui_SetLayerVisible
    ldr r0,[r4,#0x38]
    mov r1,#1
    strb r1,[r0,#0x2D]
    str r6,[r4,#0x10]
@@process:
    ldrb r0,[r4,#0x3C]
    cmp r0,#0
    bne @@check
    mov r0,r4
    bl CombinePatch_TitleProcessModeToggle
@@check:
    mov r0,r4
    ldr r1,=CombinePatch_ModeStorage
    ldrb r1,[r1,#CombinePatch_TitleSelectedModeOffset]
    bl TitleUsers_Update
    cmp r0,#0
    bne @@pending
    ldrb r0,[r4,#0x3C]
    cmp r0,#0
    bne @@latch
    b @@pending
@@pending:
    mov r0,#0
    pop {r3,r4,r5,r6,r7,pc}
@@latch:
    ldr r1,=CombinePatch_ModeStorage
    ldrb r2,[r1,#CombinePatch_TitleSelectedModeOffset]
    strb r2,[r1,#CombinePatch_SessionModeOffset]
    mov r0,#0
    str r0,[r4,#0x10]
    mov r0,#1
    pop {r3,r4,r5,r6,r7,pc}
    .pool

// L opens native language selection through the stock title-confirm/fade
// callback. R cycles modes. The previous-key byte holds R/L bits 0/1; L wins
// simultaneous presses, and held keys cannot repeat.
// L 经原版标题确认／淡出回调进入语言选择，R 切换模式。上一帧字节的位 0/1
// 分别记录 R/L；同时按下时优先 L，持续按住不重复触发。
CombinePatch_TitleProcessModeToggle:
    push {r4,r5,r6,lr}
    mov r4,r0
    mov r5,#0
    ldr r0,=CombinePatch_HidManager
    ldr r0,[r0,#4]
    cmp r0,#0
    beq @@sample
    ldr r1,[r0,#0x10]
    cmp r1,#7
    bhi @@sample
    add r1,r0,r1,lsl #4
    ldr r1,[r1,#0x28]
    mov r5,r1,lsr #8
    and r5,r5,#3
@@sample:
    ldr r6,=CombinePatch_ModeStorage
    ldrb r1,[r6,#CombinePatch_TitleRPreviousOffset]
    strb r5,[r6,#CombinePatch_TitleRPreviousOffset]
    bic r5,r5,r1
    ldr r0,[r4,#0x38]
    cmp r0,#0
    beq @@done
    ldrb r0,[r0,#0x2D]
    cmp r0,#0
    beq @@done
    tst r5,#2
    bne @@language
    tst r5,#1
    beq @@done
    ldrb r1,[r6,#CombinePatch_TitleSelectedModeOffset]
    add r1,r1,#1
    cmp r1,#CombinePatch_ModeCount
    movcs r1,#CombinePatch_ModeOffline
    strb r1,[r6,#CombinePatch_TitleSelectedModeOffset]
    ldr r0,[r4,#0x38]
    cmp r0,#0
    beq @@done
    mov r1,#1
    mov r2,#CombinePatch_TitleModePane
    ldrb r3,[r6,#CombinePatch_TitleSelectedModeOffset]
    cmp r3,#CombinePatch_ModeDownload
    moveq r3,#CombinePatch_TitleModeDownloadMessage
    beq @@setText
    cmp r3,#CombinePatch_ModeUnlock
    moveq r3,#CombinePatch_TitleModeUnlockMessage
    beq @@setText
    cmp r3,#CombinePatch_ModeOriginal
    moveq r3,#CombinePatch_TitleModeOriginalMessage
    movne r3,#CombinePatch_TitleModeOfflineMessage
@@setText:
    bl BankUi_SetMessageLine
    b @@done
@@language:
    mov r0,#CombinePatch_TitleLanguageRequested
    strb r0,[r6,#CombinePatch_TitleLanguageOffset]
    add r0,r4,#0x34
    bl TitleScreenState_Confirm
@@done:
    pop {r4,r5,r6,pc}
    .pool

// Mode changes bind storage only, never language. Release the cached prompt
// before the title-L language action; ordinary START keeps its native lifetime.
// 模式仅切换存储，不切换语言。只有标题 L 操作清理缓存提示视图；START 保留原生命周期。
CombinePatch_TitleReleaseWaitingView:
    ldr r4,[r5,#0x2C]
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_TitleLanguageOffset]
    cmp r12,#CombinePatch_TitleLanguageRequested
    bne TitleScreenState_ExitUiCleanup + 4
    mov r0,r4
    bl BankUi_ReleaseWaitingView
    cmp r0,#0
    beq TitleScreenState_ExitComplete + 8
    b TitleScreenState_ExitUiCleanup + 4
    .pool

CombinePatch_TitleBindSession:
    mov r0,r5
    ldr r1,[r5,#8]
    ldr r2,[r5,#0x0C]
    ldr r6,=CombinePatch_ModeStorage
    ldrb r3,[r6,#CombinePatch_TitleLanguageOffset]
    cmp r3,#CombinePatch_TitleLanguageRequested
    bne @@mode
    mov r0,#1
    b @@prepared
@@mode:
    ldrb r3,[r6,#CombinePatch_SessionModeOffset]
    bl TurtleRedirect_PrepareSessionPoll
@@prepared:
    cmp r0,#0
    popeq {r4,r5,r6,pc}
    sub r0,r0,#1
    strb r0,[r6,#CombinePatch_InitialLanguagePendingOffset]
    mov r0,#0
    strb r0,[r6,#CombinePatch_UnlockRecoveryOffset]
    mov r0,#1
    pop {r4,r5,r6,pc}
    .pool

// A pending rebind starts only after the native UI release. On the next Exit
// poll skip that completed release, whose old title pointer is no longer live.
// 重新绑定只在原版 UI 释放后启动；再次轮询 Exit 时不重复访问已经释放的标题指针。
CombinePatch_TitleExit:
    push {r4,r5,r6,lr}
    mov r5,r0
    bl TurtleRedirect_PrepareSessionPending
    cmp r0,#0
    bne CombinePatch_TitleBindSession
    mov r0,r5
    b TitleScreenState_Exit + 8
CombinePatch_TitleDestruct:
    push {r0,lr}
    bl TurtleRedirect_CancelPrepareSession
    pop {r0,lr}
    ldr r1,=0x00361FF0
    b TitleScreenState_Destruct + 4
    .pool

// Startup's helper ignores Initialize's return value. Keep its phase zero
// while the same owned prepare request is pending; no fade/UI advancement.
// 启动辅助流程忽略 Initialize 返回值；同一次准备未完成时保留阶段零，不提前推进渐变／界面。
CombinePatch_StartupInitializeGate:
    bl StartupPreview_Initialize
    mov r0,r4
    bl LanguageSettings_SyncPending
    cmp r0,#0
    bne StartupPreview_UpdateWaiting
    b StartupPreview_UpdateInitialize + 4
// Language is global: native startup reads the console record before any mode
// selection. The language-action flag forces only Turtle I/O to native here.
// 语言不分模式：启动先读主机记录，此时尚未选模式；语言标志仅让 Turtle I/O 走原后端。
CombinePatch_StartupInitialize:
    push {r4,r5,r6,r7,r8,lr}
    mov r4,r0
    sub sp,sp,#0x10
    ldr r12,=CombinePatch_ModeStorage
    mov r1,#CombinePatch_TitleLanguageRequested
    strb r1,[r12,#CombinePatch_TitleLanguageOffset]
    bl LanguageSettings_SyncPending
    cmp r0,#0
    bne CombinePatch_StartupSynchronizeLanguage
    b StartupPreview_Initialize + 0x0C
    .pool
// Native language is already applied. Wait before constructing any localized
// UI; retries skip heap allocation, native reload and font/message reload.
// 主机语言已由原版应用；同步完成前不创建界面。重试跳过建堆、原档及字体／文本重载。
CombinePatch_StartupSynchronizeLanguage:
    mov r7,#0
    ldr r8,=0x003AB90C
    ldr r0,[r8]
    ldr r1,[r0,#0x1C]
    mov r0,r4
    bl LanguageSettings_StartupSync
    cmp r0,#0
    addeq sp,sp,#0x10
    popeq {r4,r5,r6,r7,r8,pc}
    mov r5,#1
    b StartupPreview_SettingsReady + 4
    .pool
CombinePatch_StartupDestruct:
    push {r0,lr}
    bl LanguageSettings_Cancel
    pop {r0,lr}
    ldr r1,=0x003616E8
    b StartupPreview_Destruct + 4
    .pool

CombinePatch_TitleResult:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r2,[r12,#CombinePatch_InitialLanguagePendingOffset]
    cmp r2,#CombinePatch_SettingsFailed
    moveq r0,#20
    beq @@done
    ldrb r12,[r12,#CombinePatch_TitleLanguageOffset]
    cmp r12,#CombinePatch_TitleLanguageRequested
    moveq r0,#1
    beq @@done
    mov r0,#3
@@done:
    b BankFlow_SelectNextState + 0xCC
    .pool

CombinePatch_TurtleLoad:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne TurtleStorage_LoadAtOnce
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_TitleLanguageOffset]
    cmp r12,#CombinePatch_TitleLanguageRequested
    beq TurtleStorage_LoadAtOnce
    b TurtleRedirect_LoadBackend
    .pool

CombinePatch_TurtleSave:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne TurtleStorage_SaveAtOnce
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_TitleLanguageOffset]
    cmp r12,#CombinePatch_TitleLanguageRequested
    beq TurtleStorage_SaveAtOnce
    b TurtleRedirect_SaveBackend
    .pool

CombinePatch_LocalSaveBegin:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_StateSaveBegin
    b TurtleRedirect_OriginalStateSaveBegin
    .pool

CombinePatch_LocalSavePoll:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_StateSavePoll
    b TurtleRedirect_OriginalStateSavePoll
    .pool

// Replay only the replaced prologues. All original state transitions, physical
// game-save jobs and online settings writes continue in the untouched bodies.
// 只回放被替换的函数前言；原状态转换、实体游戏保存及联网设置写入继续使用原函数体。
TurtleRedirect_OriginalStateSaveBegin:
    push {r4-r6,lr}
    b StateLocalSave_Begin + 4
TurtleRedirect_OriginalStateSavePoll:
    push {r4-r6,lr}
    b StateLocalSave_Poll + 4

CombinePatch_InitialTurtleCheck:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r2,r1
    mov r1,r0
    mov r0,r4
    bl TurtleRedirect_InitialCheckPoll
    cmn r0,#1
    beq InitialGameCheck_UpdateWaiting
    b Initial_TurtleStorageCheckCall + 4
@@native:
    bl TurtleStorage_CheckArchiveStatus
    b Initial_TurtleStorageCheckCall + 4
    .pool

CombinePatch_InitialTurtleLoad:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r2,r1
    mov r1,r0
    mov r0,r4
    bl TurtleRedirect_InitialLoadPoll
    cmn r0,#1
    beq InitialGameCheck_UpdateWaiting
    b Initial_TurtleStorageLoadCall + 4
@@native:
    bl GameData_LoadTurtle
    b Initial_TurtleStorageLoadCall + 4
    .pool

CombinePatch_InitialTurtleDestruct:
    push {r0,lr}
    bl TurtleRedirect_CancelPrepareSession
    pop {r0,lr}
    ldr r1,=0x00361AB8
    b InitialGameCheck_Destruct + 4
    .pool

CombinePatch_TurtleFormat:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_FormatBackend
    b TurtleStorage_FormatStart
    .pool

CombinePatch_TurtleFormatPoll:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_FormatPoll
    b TurtleStorage_FormatPoll
    .pool

CombinePatch_SelectNoGameMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOriginal
    moveq r2,r3
    bxeq lr
    cmp r12,#CombinePatch_ModeDownload
    moveq r2,#CombinePatch_DownloadNoGameMessage
    movne r2,#CombinePatch_NoGameBlockedMessage
    bx lr
    .pool

CombinePatch_NoGameChoice:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOriginal
    beq @@choice
    cmp r12,#CombinePatch_ModeDownload
    bne InitialGameCheck_FailureTransition
@@choice:
    ldr r5,[r4,#0x40]
    b InitialGameCheck_NoGameChoiceSetup + 4
    .pool

// Replace only the Download confirmation label; retain the native HOME label
// in other modes, the cancel label, and the caller's flags and arguments.
// 只替换下载确认按钮；其他模式保留原版 HOME 标签，取消标签及调用者标志和参数不变。
CombinePatch_SelectNoGameDownloadButton:
    mrs r3,cpsr
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeDownload
    moveq r12,#CombinePatch_DownloadStartMessage
    movne r12,#0x59
    msr cpsr_f,r3
    mov r3,r12
    bx lr
    .pool

// Keep the native connection argument and date result outside Offline Mode.
// Offline shares the console-date conversion already used by local tickets.
// 非离线模式保留原版连接参数和日期结果；离线复用本地票据使用的主机日期转换。
CombinePatch_SelectBankSlotDate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne BankSlot_GetCurrentDate
    mov r0,r1
    b SystemTime_GetCurrentDate
    .pool

// Connection availability reads only the session mode latched as the title
// exits. It never samples R or changes the selected title mode.
// 连接可用性只读取标题退出时锁定的会话模式。它绝不采样 R 键，也不改变标题所选模式。
CombinePatch_NetworkAvailability:
    push {r1-r3,lr}
    ldr r2,=CombinePatch_ModeStorage
    mov r3,#DOWNLOAD_CAPTURE_NONE
    strb r3,[r2,#CombinePatch_DownloadCaptureStatusOffset]
    ldrb r1,[r2,#CombinePatch_SessionModeOffset]
    cmp r1,#CombinePatch_ModeOffline
    moveq r0,#1
    popeq {r1-r3,pc}
    pop {r1-r3,lr}
    b 0x001103CC
    .pool

// Replace only the system-session carrier; initialization, poll consumer,
// timers, error handling and cleanup of the client state remain native.
// 仅替换系统会话载体；客户端初始化、轮询消费、计时、错误处理及清理保持原版。
CombinePatch_ConnectionConstruct:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne 0x002312CC
    ldr r2,=0x00236A90
    b NativeResponse_CreateConnection
    .pool

CombinePatch_ConnectionPoll:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq NativeResponse_PollConnection
    push {r4-r6,lr}
    b 0x002A43D0
    .pool

CombinePatch_ConnectionUnbind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bxeq lr
    push {r4,lr}
    b 0x002A44A4
    .pool

// SD sessions have no system-network handle. Keep the native SDK job scheduler,
// but do not activate AC/NEX connection monitoring for Offline. The worker still
// observes its stop flag, locks, sleeps and exits through its original code.
// SD 会话没有系统网络句柄；保留原 SDK 作业调度，仅离线不执行 AC/NEX 连接监控。
// 后台线程仍按原代码处理停止标志、加锁、休眠和退出，不改账户或错误状态。
CombinePatch_NetworkMonitorUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bxeq lr
    push {r4-r6,lr}
    b NetworkMonitor_Update + 4
    .pool

CombinePatch_NetworkMonitorWorkerAccount:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r4,#0
    ldrne r4,[r6,#0x28]
    bx lr
    .pool

CombinePatch_InitialRemoteRecordUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq @@local
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#2
    bne @@native
    ldr r12,[r0,#0x10]
    cmp r12,#7
    bne @@native
@@local:
    b BankdataRedirect_InitialRecordUpdate
@@native:
BankdataRedirect_OriginalInitialRecordUpdate:
    push {r4-r6,lr}
    b InitialRemoteRecordState_Update + 4
    .pool

CombinePatch_RecordProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r5
    mov r2,#5
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b BankRemote_RecordProviderResult
@@native:
    ldr r0,[r4,#0x10]
    b BankRemote_RecordProvider + 4
    .pool

CombinePatch_QueryProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r5
    mov r2,#4
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b BankRemote_QueryProviderResult
@@native:
    ldr r0,[r4,#0x10]
    b BankRemote_QueryProvider + 4
    .pool

CombinePatch_DownloadProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r6
    mov r2,#6
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b 0x002A363C
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A3628
    .pool

CombinePatch_DownloadResponse:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne BankdataRedirect_OriginalDownloadResponse
    ldr r2,=BankdataRedirect_OriginalDownloadResponse
    b NativeResponse_DownloadResponse
BankdataRedirect_OriginalDownloadResponse:
    push {r4-r6,lr}
    b 0x002A3958
    .pool

CombinePatch_MetadataUpdateProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r5
    mov r2,#10
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b 0x002A2B3C
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A2B28
    .pool

CombinePatch_InitialCampaignRequest:
    // Select before the initial resource callback, not from a previous job.
    // 初始资源回调前选择本次后端，不能沿用前一个作业的标志。
    push {r0-r4,lr}
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#1
    movne r0,#0
    ldr r12,=CombinePatch_OnlineTicketFallback
    ldrb r1,[r12]
    bl FakeServerClient_SelectTicket
    mov r12,r0
    pop {r0-r4,lr}
    cmp r12,#0
    // Native initialization and observer binding have already run. Local
    // transport delivers the resource on the next native wait update.
    // 原初始化及观察者绑定已完成；本地传输在下一次原等待更新时交付资源。
    movne r0,#0
    bxne lr
    b 0x001D5A14
    .pool

CombinePatch_GiftUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq ServerGifts_Update
ServerGifts_OriginalUpdate:
    push {r4-r11,lr}
    b RewardReceiveState_Update + 4
    .pool

CombinePatch_GiftResourceRequest:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq ServerGifts_Request
    b 0x001D5A14
    .pool

CombinePatch_BankDataSyncDispatch:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_DownloadUpdate
    cmp r12,#CombinePatch_ModeDownload
    beq BankdataRedirect_ExportDownloadUpdate
BankdataRedirect_OriginalDownloadUpdate:
    push {r4-r6,lr}
    b BankDataSyncState_Update + 4
    .pool

CombinePatch_BankDataSyncDestruct:
    push {r0,lr}
    bl BankdataRedirect_CancelDownloaded
    pop {r0,lr}
    ldr r1,=0x00361EC8
    b BankDataSyncState_Destruct + 4
    .pool

// Download Mode uses its SD destination message on the complete-file route.
// Preserve the native waiting UI and initializer continuation; the other
// modes retain their existing message selection.
// 下载模式在完整文件路线显示 SD 下载位置；保留原版等待界面和初始化器后续流程，
// 其他模式维持各自原有的文本选择。
CombinePatch_BankDataSyncInitialize:
    mov r3,r1
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeUnlock
    beq @@native
    cmp r12,#CombinePatch_ModeOriginal
    beq @@native
    cmp r12,#CombinePatch_ModeDownload
    bne @@offline
    mov r1,#CombinePatch_DownloadProgressMessage
    b @@show
@@offline:
    mov r1,#CombinePatch_OfflineBankConnectMessage
    b @@show
@@native:
    cmp r3,#0
    beq @@return
    mov r1,#0x0E
@@show:
    sub sp,sp,#8
    str r1,[sp]
    bl MessageUi_Begin
    ldr r1,[sp]
    add sp,sp,#8
    ldr r0,[r4,#0x38]
    b BankDataSyncState_Initialize + 0x2C
@@return:
    b BankDataSyncState_Initialize + 0x30
    .pool

// Keep the stock Internet and save messages in Download and Unlock modes. Only
// the Offline route selects the appended local-data messages.
// 下载模式与解锁模式保留原版联网与保存文本；只有离线路线选择追加的本地数据文本。
CombinePatch_SelectInitialConnectionMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_OfflineInitialConnectMessage
    movne r1,#0x0C
    bx lr
    .pool

CombinePatch_SelectSaveMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_OfflineSaveMessage
    movne r1,#8
    bx lr
    .pool

// Post-selection and gift-query connection messages use the session mode;
// all surrounding stock UI, sound and animation instructions remain intact.
// 选游戏后和礼物查询的连接提示按会话模式选择；周围原版 UI、声音和
// 动画指令全部保持不变。
CombinePatch_SelectPostSelectionConnectionMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_OfflineBankConnectMessage
    movne r1,#0x0E
    bx lr
    .pool

CombinePatch_PostSelectionConnectionUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b BankdataRedirect_SourceRecoveryUpdate
@@native:
    cmp r12,#CombinePatch_ModeUnlock
    ldreq r1,=CombinePatch_PostSelectionConnectionUpdateOfficial
    beq UnlockMode_PostSelectionUpdate
CombinePatch_PostSelectionConnectionUpdateOfficial:
BankdataRedirect_OriginalSourceRecoveryUpdate:
    push {r4-r6,lr}
    b PostSelectionConnectionState_Update + 4
    .pool

// Only Unlock Mode changes the acknowledged mismatch result. The flow
// controller still runs the native finalizer/destructor before creating state 10,
// the same state reached after confirming the first feature-menu entry.
// 只有解锁模式改动不匹配提示确认后的结果。流程控制器仍先执行原版收尾与析构，
// 再创建 state 10，与确认功能选单第一项后进入的状态相同。
// Offline appends one native-confirmed page; online modes keep stock line 10.
// 离线仅追加一页原版按键确认说明，联网各模式保留原消息 10。
CombinePatch_SelectMismatchMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r2,#131
    movne r2,#10
    bx lr
    .pool

CombinePatch_TransactionRecoveryUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_TransactionRecoveryUpdate
    cmp r12,#CombinePatch_ModeUnlock
    ldreq r1,=CombinePatch_TransactionRecoveryUpdateOfficial
    beq UnlockMode_RecoveryUpdate
CombinePatch_TransactionRecoveryUpdateOfficial:
BankdataRedirect_OriginalTransactionRecoveryUpdate:
    push {r4-r7,lr}
    b TransactionRecoveryState_Update + 4
    .pool

CombinePatch_LocalForceRollbackUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_ForceRollbackUpdate
BankdataRedirect_OriginalForceRollbackUpdate:
    push {r4,r5,lr}
    b ForceRollbackState_Update + 4
    .pool

CombinePatch_RecoveryResult:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeUnlock
    bne @@native
    cmp r2,#19
    moveq r0,#10
    popeq {r4,pc}
@@native:
    cmp r2,#3
    b BankFlow_RecoveryResult + 4
    .pool

// Latch success only after the official forced rollback reports completion;
// pressing the combination or opening the keyboard does not set this flag.
// 仅在官方强制回滚报告完成后记录成功；按组合键或打开输入界面不会设置此标记。
CombinePatch_ForceRollbackSuccess:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r3,[r12,#CombinePatch_SessionModeOffset]
    cmp r3,#CombinePatch_ModeUnlock
    moveq r3,#1
    streqb r3,[r12,#CombinePatch_UnlockRecoveryOffset]
    b BankFlow_ForceRollbackSuccess + 0x18
    .pool

// Every mode retains the native rollback request object, wait and finalizer.
// 所有模式保留原回滚请求对象、等待及收尾；离线仅替换实际请求接口。
CombinePatch_NoSaveRollbackUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_NoSaveUpdate
CombinePatch_NoSaveRollbackUpdateOfficial:
BankdataRedirect_OriginalNoSaveUpdate:
    push {r4-r6,lr}
    b BankNoSaveRollbackState_Update + 4
    .pool

// Retain native construction and destruction of all SDK-owned containers.
// 原版继续构造和销毁 SDK 容器；离线只使用不触发联网的提供者绑定。
CombinePatch_RemoteBind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_BindRemote
BankdataRedirect_OriginalBindRemote:
    push {r4-r6,lr}
    b 0x001DF3E4
    .pool

// Cancel delivery before native containers/jobs are released. Never join the
// worker or dereference this remote from its background task.
// 原容器／作业释放前取消投递；不等待后台线程，后台任务不解引用远端对象。
CombinePatch_RemoteUnbind:
    push {r0-r3,r12,lr}
    bl NativeResponse_CancelRemote
    pop {r0-r3,r12,lr}
    push {r4-r6,lr}
    b 0x002A3714
    .pool

CombinePatch_MetadataProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r6
    mov r2,r5
    bl NativeResponse_AcceptMetadata
    pop {r4,lr}
    b 0x002A2714
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A26FC
    .pool

CombinePatch_StageProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r6
    mov r2,#8
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b 0x002A25DC
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A25C8
    .pool

CombinePatch_RecoveryTokenProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r5
    mov r1,r4
    mov r2,r6
    bl BankdataRedirect_RecoveryTokenProvider
    pop {r4,lr}
    b 0x002A2854
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A283C
    .pool

CombinePatch_CommitProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r6
    mov r1,r4
    mov r2,#1
    mov r3,r7
    bl BankdataRedirect_TransactionProvider
    pop {r4,lr}
    b 0x001D5E0C
@@native:
    ldr r0,[r4,#0x10]
    b 0x001D5DF4
    .pool

CombinePatch_RollbackProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r6
    mov r1,r4
    mov r2,#0
    mov r3,r7
    bl BankdataRedirect_TransactionProvider
    pop {r4,lr}
    b 0x001D5CC0
@@native:
    ldr r0,[r4,#0x10]
    b 0x001D5CA8
    .pool

CombinePatch_StorageNoticeDisconnectUpdate:
    push {r0,lr}
    bl BankStorageNotice_Poll
    cmp r0,#0
    pop {r0,lr}
    moveq r0,#0
    bxeq lr
    b CombinePatch_DisconnectUpdate

CombinePatch_DisconnectUpdate:
    ldrb r12,[r0,#0x3C]
    cmp r12,#0
    bne CombinePatch_DisconnectWithLanguageSave
@@native:
CombinePatch_DisconnectUpdateOfficial:
    push {r4,lr}
    b DisconnectCleanupState_Update + 4
    .pool


CombinePatch_BankCreateUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_CreateUpdate
BankdataRedirect_OriginalCreateUpdate:
    push {r4-r6,lr}
    b BankCreateState_Update + 4
    .pool

CombinePatch_AccountInitialize:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_InitializeAccount
    push {r4-r6,lr}
    b 0x00114920
    .pool

CombinePatch_AccountUnbind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_UnbindAccount
    push {r4-r6,lr}
    b 0x0022DDD0
    .pool

CombinePatch_AccountSend:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,r1
    bxeq lr
    b 0x0021EB10
    .pool

// The provider-null check precedes this hook. Only the local initial request
// treats its provider as ready; the native transport's initialized byte is
// neither fabricated nor cleared, including when returning to online modes.
// 此钩子前已检查提供者非空；仅本地初始请求视为就绪，不伪造或清除原版联网
// 初始化字节，切回联网模式时仍由原版创建和管理真实会话。
CombinePatch_AccountRequestReady:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#1
    ldrneb r0,[r4]
    bx lr
    .pool

CombinePatch_CreateProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r5
    mov r2,#7
    bl NativeResponse_AcceptOperation
    pop {r4,lr}
    b 0x002A3884
@@native:
    ldr r0,[r4,#0x10]
    b 0x002A386C
    .pool

// Preserve the receipt entry outside Download Mode. Its fallback route must
// still leave without entering mileage or Bank Box.
// 下载模式之外保留领取入口；下载模式的备用分支仍须不经里程或盒子直接结束。
CombinePatch_RewardResult:
    ldr r1,=CombinePatch_ModeStorage
    ldrb r1,[r1,#CombinePatch_SessionModeOffset]
    cmp r1,#CombinePatch_ModeDownload
    moveq r0,#19
    movne r0,#12
    pop {r4,pc}
    .pool

CombinePatch_HomeResult:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOriginal
    beq BankFlow_SelectNextState + 0x1F8
    // Defensive fallback for a stale menu result; no language entry in a mode.
    // 防御旧选单残留结果：模式内部没有语言切换入口。
    mov r0,#20
    pop {r4,pc}
    .pool

// The first Download menu item uses the native game-independent downloader.
// Its state-28 object keeps the native flag that skips selected-game writes.
// 下载选单第一项使用原版不依赖游戏的下载器；state 28 对象保留跳过所选游戏写入的
// 原版标志。
CombinePatch_SelectUseBankState:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeDownload
    moveq r0,#28
    pop {r4,pc}
    .pool

// State 29 finishes its native remote cleanup and UI teardown before Download
// Mode disconnects. Original Mode continues to the HOME transfer controller.
// state 29 完成原版远端收尾和界面清理后，下载模式断网；原版模式仍进入 HOME
// 传送控制器。
CombinePatch_HomeCleanupResult:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeDownload
    moveq r0,#20
    pop {r4,pc}
    .pool

CombinePatch_Result21:
    // Keep native state 19 so the local service releases the load reservation.
    // 保留原版 state 19，由本地服务释放载入预留后再断开。
    b BankFlow_SelectNextState + 0x370

CombinePatch_Result5:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeDownload
    moveq r0,#19
    movne r0,#13
    b BankFlow_SelectNextState + 0xCC
    .pool

CombinePatch_Result6Or12:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeDownload
    beq BankFlow_SelectNextState + 0x370
    b BankFlow_SelectNextState + 0x294
    .pool

// Download, Unlock and Original must not inherit Offline save substitutions.
// The native branches below replay exactly the instructions overwritten at
// each hook and then resume the original save state.
// 下载、解锁和原版模式不能继承任何离线保存替换。下面的原版分支会重放各钩子处被
// 覆盖的指令，然后回到原版保存状态。
CombinePatch_SaveStateUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankdataRedirect_SaveUpdate
BankdataRedirect_OriginalSaveUpdate:
    push {r4-r6,lr}
    b BankSaveState_Update + 4
    .pool

// Mover/eShop and HOME are disabled in the first three modes. Support code
// stays clickable even with an invalid identity; language is title-only.
// 前三种模式禁用 Mover/eShop、HOME；支持代码均走原路径，语言仅在标题 L 设置。
CombinePatch_MenuSelectionCallback:
    ldr r2,=CombinePatch_ModeStorage
    ldrb r2,[r2,#CombinePatch_SessionModeOffset]
    cmp r2,#CombinePatch_ModeOriginal
    beq @@native
    ldr r2,[r0,#0x10]
    cmp r2,#1
    bne @@native
    cmp r1,#3
    beq @@disabled
    cmp r1,#5
    beq @@disabled
@@native:
    push {r4-r6,lr}
    b BankMenuState_SelectionCallback + 4
@@disabled:
CombinePatch_MenuSelectionDisabled:
    push {r4-r6,lr}
    mov r4,r0
    ldr r5,[r4,#0x38]
    mov r6,#1
    ldr r0,[r5,#0x10]
    cmp r0,#0
    beq @@noCursorObject
    strb r6,[r0,#0x25]
@@noCursorObject:
    ldr r0,[r5,#0x80]
    mov r1,r6
    bl Ui_SetSelectionFocus
    mov r0,#0
    strb r0,[r5,#0x88]
    mov r0,#1
    str r0,[r4,#0x10]
    mov r0,#0
    pop {r4-r6,pc}

// Save the natively loaded current-format object before its per-session flags
// are adjusted. Replay the displaced comparison for the native continuation.
// 原版对象装载为当前格式后、会话标志调整前保存；重放覆盖的比较指令再继续原回调。
CombinePatch_DownloadCaptureTrampoline:
    push {r0-r3,r12,lr}
    sub sp,sp,#8
    mrs r12,cpsr
    str r12,[sp]
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeDownload
    bne @@restore
    ldrb r0,[r4,#0x41]
    cmp r0,#0
    beq @@restore
    ldr r0,=CombinePatch_ModeStorage
    mov r1,#DOWNLOAD_CAPTURE_FAILED
    strb r1,[r0,#CombinePatch_DownloadCaptureStatusOffset]
    ldr r1,[r4,#8]
    ldr r1,[r1,#0xCC]
    mov r0,r4
    bl BankdataRedirect_CaptureDownloaded
    cmp r0,#0
    beq @@restore
    ldr r1,=CombinePatch_ModeStorage
    mov r0,#DOWNLOAD_CAPTURE_PENDING
    strb r0,[r1,#CombinePatch_DownloadCaptureStatusOffset]
@@restore:
    ldr r12,[sp]
    msr cpsr_f,r12
    add sp,sp,#8
    pop {r0-r3,r12,lr}
    cmp r6,#0
    bx lr
    .pool

// Import the Turtle backend into the added pages; hook sites remain assembly-only.
// Turtle 后端放在新增页；原址钩子仅保留汇编跳转。
TurtleRedirect_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/turtle_redirect.o"
TurtleRedirect_PayloadEnd:
LanguageSettings_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/language_settings.o"
LanguageSettings_PayloadEnd:

// Keep the remaining feature modules consecutive inside the added pages.
// 其余功能模块在新增页内连续排列。
CombinePatch_PayloadBegin:
FsHelpers_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/fs_helpers.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/diagnostic_log.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/storage_notice.o"
    .importobj BUILD_DIRECTORY + "/storage_notice.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/memory.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/console_settings.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/console_identity.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/utf_path.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/title_users.o"
    .importobj BUILD_DIRECTORY + "/support_code.o"
FsHelpers_PayloadEnd:
SystemTime_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/system_time.o"
SystemTime_PayloadEnd:
BankdataRedirect_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:
FakeServer_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fakeServer/console_profile.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/memory.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/fake_server_client.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/configuration.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/configuration_edit.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/fake_servers.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/fakeserverapi.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/native_response.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/native_transport.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/service_request.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/request_broker.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/async_task.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/native_worker.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/service_worker.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/settings_store.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/protected_file.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/source.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/unlock.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/gifts.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/gift_files.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/mover_checks.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/name_policy.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/custom_settings.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/mover_expression.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/word_pattern.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/ticket.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/ticket_settings.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/numbers.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/transaction.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/transaction_view.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/sd_adapter.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/user_profiles.o"
    .importobj BUILD_DIRECTORY + "/common/fakeServer/account_configuration.o"
FakeServer_PayloadEnd:
ServerGifts_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/server_gifts.o"
ServerGifts_PayloadEnd:
ServerTicket_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/server_ticket.o"
ServerTicket_PayloadEnd:
UnlockMode_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/unlock_mode.o"
UnlockMode_PayloadEnd:
CombinePatch_PayloadEnd:

// Choose the job backend once at initialization; subsequent interfaces read
// the stored result. Keep the native state, constructor and destructor.
// 初始化时选择作业后端，后续接口读取已保存的结果。保留原版状态、构造与析构。
.align 4
// Offline never opens the entitlement applet. Keep its existing connection
// pane visible instead of clearing it and fading into an empty applet screen.
// 离线不打开使用权小程序；保留当前连接提示，不清屏渐变到空的小程序界面。
CombinePatch_TicketUiPrepare:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq OptionalRewardState_TicketUiAdvance
    ldr r5,[r4,#0x3C]
    b OptionalRewardState_TicketUiPrepare + 4
CombinePatch_TicketUiFade:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#1
    bxeq lr
    b Ui_SetAppletFade
CombinePatch_TicketUiCleanup:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne MessageUi_Animate
    // Only successful local tickets keep the pane for the next connection
    // message. Errors and no-pass dialogs retain their native UI cleanup.
    // 仅本地票务成功时保留提示框衔接下一条连接消息；错误、未持券仍按原版收尾。
    ldrb r12,[r4,#0x30]
    cmp r12,#4
    moveq r0,#1
    bxeq lr
    b MessageUi_Animate
CombinePatch_TicketInitialize:
    ldr r1,[r4,#0x28]
    ldr r12,=CombinePatch_ModeStorage
    ldrb r2,[r12,#CombinePatch_SessionModeOffset]
    ldr r12,=CombinePatch_OnlineTicketFallback
    ldrb r3,[r12]
    b ServerTicket_InitializeSelected
CombinePatch_TicketShopRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq 0x0022DD84
    mov r0,r4
    ldr r12,=ServerTicket_RequestApplet + 1
    bx r12
CombinePatch_TicketCreditRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    moveq r1,#0
    beq 0x002A0564
    mov r1,#6
    ldr r12,=ServerTicket_RequestApplet + 1
    bx r12
CombinePatch_TicketPurchaseRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq 0x0022DD84
    sub r0,r4,#0x148
    ldr r12,=ServerTicket_RequestApplet + 1
    bx r12
CombinePatch_TicketPurchasePoll:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq TicketJob_PollPurchase
    mov r2,r4
    ldr r12,=ServerTicket_PollPurchase + 1
    bx r12
CombinePatch_TicketEcCatalog:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq 0x00162658
    ldr r12,=ServerTicket_EcCatalog + 1
    bx r12
CombinePatch_TicketEcBalance:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq 0x00162E78
    ldr r12,=ServerTicket_EcBalance + 1
    bx r12
CombinePatch_TicketEcFormat:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    beq 0x00162F00
    ldr r12,=ServerTicket_EcFormat + 1
    bx r12
CombinePatch_TicketEcWorkerConstruct:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    movne r2,#LOCAL_TICKET_EC_STACK_SIZE
    b 0x0022DFD0
CombinePatch_TicketSend:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_Send
    b 0x0021EB10
CombinePatch_TicketJobPoll:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_Poll
ServerTicket_OriginalPoll:
    push {r4,r5,r6,lr}
    b TicketJob_Poll + 4
CombinePatch_TicketCreditPoll:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_PollCredit
ServerTicket_OriginalCredit:
    ldr r0,[r0,#4]
    b 0x002A05FC
CombinePatch_TicketExpiryRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    movne r3,#0
    b 0x001D6FB8
CombinePatch_TicketProviderBind:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    movne r0,#1
    bxne lr
    bx r2
CombinePatch_TicketProviderUnbind:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_CancelProvider
    bx r1
    .pool
CombinePatch_TicketWrappersEnd:
CombinePatch_OnlineTicketFallback:
    .byte ONLINE_FAKE_SERVER_TICKET_FALLBACK
    .align 4
CombinePatch_CodeUsedEnd:
.endarea

// Only the small startup loader uses original executable padding. Native
// function bodies remain unchanged because assembly starts from the base image.
// 仅小型启动加载器使用原可执行填充；从原版镜像构建，完整保留原函数体。
.org 0x00313A40
.area OfflinePatch_VersionStorageStart - 0x00313A40
CodeExpansion_Startup:
    push {r0-r12,lr}
    ldr r0,=CodeExpansion_PayloadStart
    ldr r1,=CodeExpansion_ExecutableSize
    bl CodeExpansion_Enable
    cmp r0,#0
    bne @@failure
    pop {r0-r12,lr}
    ldr pc,=BankStartup_RunConstructors + 1
@@failure:
    mov r0,#0
    swi 0x3C
    b @@failure
    .pool
CodeExpansion_LoaderBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/runtime_environment.o"
CodeExpansion_LoaderEnd:
.endarea

// Reserve the final 64 bytes of the mapped text segment as a zero-padded ASCII
// identifier. No runtime code reads it in the current version.
// 将已映射 text 段的最后 64 字节预留为零填充 ASCII 标识。当前版本没有运行时代码
// 读取它。
.org OfflinePatch_VersionStorageStart
.area OfflinePatch_VersionStorageSize, 0
OfflinePatch_VersionIdentifier:
    .incbin BUILD_DIRECTORY + "/patch-version.bin"
.endarea

// Dedicated service data, outside code and all original runtime objects.
// 服务专用数据区，与代码及全部原版运行时对象分开。
.org CombinePatch_CodeEnd
FakeServer_ContextStart:
.area FakeServer_ContextSize, 0
.endarea
FakeServer_ContextEnd:
FakeServer_WorkerStart:
.area 0x1000, 0
.endarea
FakeServer_TaskBodyStart:
.area 0x100000, 0
.endarea
FakeServer_WorkerStackStart:
.area 0x10000, 0
.endarea
FakeServer_WorkerStackEnd:
FakeServer_ReservedTailStart:
.area 0x1000, 0
.endarea
FakeServer_RuntimeEnd:

.close
