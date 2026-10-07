.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

// 0: require native online tickets (public build); 1: auto-fallback only when
// identified Azahar lacks the ticket interface (test build). Offline stays local.
// 0：联网必须使用真实票务接口（公开版）；1：仅确认 Azahar 缺接口时自动使用
// 本地结果（测试版）。离线模式始终使用本地结果。
.definelabel ONLINE_TICKET_CHECK_BYPASS, 0

// Offline behavior is the baseline. Mode wrappers branch to these native entry
// points when Download, Unlock or Original Mode was selected on the title screen.
// 离线行为作为基线；标题界面选定下载、解锁或原版模式后，模式包装函数会跳转到这些
// 原版入口。
.definelabel OfflinePatch_VersionStorageSize, 0x40
.definelabel OfflinePatch_VersionStorageStart, TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CodeExpansion_PayloadStart, 0x003FC000
.definelabel CODE_EXPANSION_PAYLOAD_SIZE, 0x3000
.definelabel CombinePatch_CodeStart, CodeExpansion_PayloadStart
.definelabel CombinePatch_CodeEnd, CodeExpansion_PayloadStart + CODE_EXPANSION_PAYLOAD_SIZE
.definelabel OfflinePatch_RuntimeStorageStart, ReadWriteMappedEnd - 0x10
.definelabel OfflinePatch_RuntimeStorageEnd, ReadWriteMappedEnd
.definelabel CombinePatch_ModeStorage, OfflinePatch_RuntimeStorageStart
.definelabel CombinePatch_HidManager, 0x003DC3B4
.definelabel CombinePatch_ModeOffline, 0
.definelabel CombinePatch_ModeDownload, 1
.definelabel CombinePatch_ModeUnlock, 2
.definelabel CombinePatch_ModeOriginal, 3
.definelabel CombinePatch_ModeCount, 4
.definelabel CombinePatch_KeyR, 0x100
.definelabel CombinePatch_SessionModeOffset, 0
.definelabel CombinePatch_DownloadCaptureStatusOffset, 1
.definelabel DOWNLOAD_CAPTURE_NONE, 0
.definelabel DOWNLOAD_CAPTURE_COMPLETE, 1
.definelabel DOWNLOAD_CAPTURE_FAILED, 2
.definelabel CombinePatch_TitleRPreviousOffset, 2
.definelabel CombinePatch_TitleSelectedModeOffset, 3
.definelabel CombinePatch_InitialLanguagePendingOffset, 5
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
.definelabel CombinePatch_DisabledMenuMessage, 0x6A
.definelabel CombinePatch_LanguageMenuMessage, 0x6B
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

.open INPUT_CODE, OUTPUT_CODE, 0x00100000

// Enable the added pages before the original PREL32 constructor walker runs.
// 原版 PREL32 构造函数遍历前启用新增页。
.org BankStartup_ConstructorCall
    bl CodeExpansion_Startup

// Offline baseline hooks. Each site is retained so the combined project starts
// from the tested local-data behavior rather than duplicating a second patch.
// 离线基线钩子。保留每个位置，使合并工程从已测试的本地数据行为起步，而不是复制
// 第二份补丁。
.org NetworkConnectionState_Update
    b CombinePatch_NetworkUpdate
.org NetworkConnection_AvailabilityCall
    bl CombinePatch_NetworkAvailability
.org NetworkConnection_SkipRemoteJob
    b CombinePatch_NetworkSkipRemoteJob
    nop
    nop
.org NetworkConnection_MessageIdLoad
    bl CombinePatch_SelectInitialConnectionMessage
.org InitialRemoteRecordState_Update
    b CombinePatch_InitialRemoteRecordUpdate
.org InitialRemoteRecord_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org OptionalRewardState_TicketInitializeCall
    bl CombinePatch_TicketInitialize
.org OptionalRewardState_TicketPollCall
    bl CombinePatch_TicketPoll
.org OptionalRewardState_TicketUnbindCall
    bl CombinePatch_TicketUnbind
.org BankDataSyncState_Update
    b CombinePatch_BankDataSyncDispatch
.org BankDataSync_TurtleTransactionWrite
    b CombinePatch_BankDataSyncTurtleState0
.org BankDataSyncState_Initialize + 0x18
    b CombinePatch_BankDataSyncInitialize

.org TitleScreenState_Update
    b CombinePatch_TitleScreenUpdate
.org TitleScreenUi_Create + 0xA0
    bl CombinePatch_TitleModeTextInitialize
.org TitleScreenState_Initialize + 0x3C
    b CombinePatch_TitlePreview
.org TitleScreenState_ExitUiCleanup
    b CombinePatch_TitleReleaseWaitingView
.org TitleScreenState_ExitComplete
    b CombinePatch_TitleBindSession
.org BankFlow_SelectNextState + 0x100
    beq CombinePatch_TitleResult
.org BankFlow_SelectNextState + 0xC8
    b CombinePatch_TitleOrResume

.org BankFlow_PostSelectionMetadataPath
    b CombinePatch_SelectPostSelectionState
.org PostSelectionConnectionState_Update
    b CombinePatch_PostSelectionConnectionUpdate
.org TransactionRecoveryState_Update
    b CombinePatch_TransactionRecoveryUpdate
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

// Keep the native first-present flag test. Offline mode bypasses only the
// remote BOSS gift lookup after that flag is set and resumes the native local
// mileage calculation. Download and Unlock modes preserve the stock branch.
// 保留原版首次奖励标志判断。标志已设置后，离线模式只跳过远端 BOSS 礼物查询，
// 并恢复原版的本地里程计算；下载模式与解锁模式保留原分支。
.org RewardReceive_FirstPresentBranch
    b CombinePatch_FirstPresentDispatch

.org DisconnectCleanupState_Update
    b CombinePatch_DisconnectUpdate
.org DisconnectCleanup_SkipRemoteJob
    b CombinePatch_DisconnectSkipRemoteJob
    nop
    nop

.org BankCreateState_Update + 0x58
    b CombinePatch_BankCreateState0
    nop
.org BankCreateState_Update + 0x1E4
    b CombinePatch_BankCreateAfterMessages
    nop
.org BankCreateState_Update + 0x2F4
    bl CombinePatch_CreateInitial
.org BankCreateState_Update + 0x2FC
    bne CombinePatch_BankCreateSuccess

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

// Capture only after the native current/legacy body loader has completed.
// All modes retain the callback's metadata and completion processing.
// 原版当前／旧格式数据装载完成后才捕获；所有模式保留回调的元数据和完成处理。
.org BankRemote_DownloadSuccessCallback + 0x98
    bl CombinePatch_DownloadCaptureTrampoline

.org ReturnToTitleState_Update + 0x168
    bl CombinePatch_SelectDisconnectMessage

.org BankSaveState_Update + 0x84
    b CombinePatch_SaveBegin
    nop
.org BankSaveState_Update + 0x104
    b CombinePatch_SaveDisplayWait
    nop
    nop
.org BankSave_TurtleCommitWrite
    b CombinePatch_SaveTurtleState2
.org BankSave_TurtleRollbackWrite
    b CombinePatch_SaveTurtleState1
.org BankSave_SerializeAndStage + 0x180
    bl CombinePatch_SaveStage
.org BankSaveState_Update + 0x3B4
    bl CombinePatch_SaveCommit
.org BankSaveState_Update + 0x3C8
    b CombinePatch_SaveCommitWait
    nop
    nop
.org BankSaveState_Update + 0x3F0
    bl CombinePatch_SaveRollback
.org BankSaveState_Update + 0x408
    b CombinePatch_SaveRollbackWait
    nop
    nop
.org BankSaveState_Initialize + 0x28
    bl CombinePatch_SelectSaveMessage

// Redirect only the Turtle record's storage backend. The stock wrappers still
// validate the object, set the loaded flag and map backend errors.
// 仅重定向 Turtle 记录的存储后端。原版包装函数仍负责对象校验、loaded 标志和
// 后端错误映射。
.org TurtleStorage_LoadBackendCall
    bl CombinePatch_TurtleLoad
.org TurtleStorage_SaveBackendCall
    bl CombinePatch_TurtleSave
.org Title_TurtleStorageCheckCall
    bl TurtleRedirect_CheckBackend
.org Initial_TurtleStorageCheckCall
    bl CombinePatch_TurtleCheck
.org Initial_TurtleStorageFormatCall
    bl CombinePatch_TurtleFormat
.org Initial_TurtleStorageFormatPollCall
    bl CombinePatch_TurtleFormatPoll

.org CombinePatch_CodeStart
.area CombinePatch_CodeEnd-CombinePatch_CodeStart

CombinePatch_RedirectHomeToLanguage:
    ldr r0,[r0,#0x10]
    ldr r0,[r0,#0x38]
    cmp r0,#0
    blne WaitingUi_Hide
    mov r0,#1
    pop {r4,pc}

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
CombinePatch_SelectSupportMenuText:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r8,#0x2B
    movne r8,#CombinePatch_DisabledMenuMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

CombinePatch_SelectInstalledMoverMenuText:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r5,#0x2D
    movne r5,#CombinePatch_DisabledMenuMessage
    pop {r0,r12}
    msr cpsr_f,r12
    bx lr
    .pool

CombinePatch_SelectDownloadMoverMenuText:
    mrs r12,cpsr
    push {r0,r12}
    ldr r0,=CombinePatch_ModeStorage
    ldrb r0,[r0]
    cmp r0,#CombinePatch_ModeOriginal
    moveq r6,#0x2C
    movne r6,#CombinePatch_DisabledMenuMessage
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
    movne r6,#CombinePatch_LanguageMenuMessage
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
    movne r5,#CombinePatch_LanguageMenuMessage
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
    bne @@stock
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

// Persist the patch's language-menu selection to the active backend. Native
// modes then resume their real disconnect job; Offline uses its local delay.
// 把补丁语言选单的选择保存至当前后端。原存档模式随后继续真实断开作业，离线模式
// 使用本地延时。首次原存档初始化仍由原版游戏检查链提交待选语言。
CombinePatch_DisconnectWithLanguageSave:
    push {r4,lr}
    mov r4,r0
    ldrb r1,[r4,#0x3C]
    cmp r1,#0
    beq @@disconnect
    ldr r1,[r4,#0x10]
    cmp r1,#0
    beq @@beginSave
    cmp r1,#1
    bne @@disconnect
    bl StateLocalSave_Poll
    cmp r0,#0
    beq @@pending
    cmp r0,#1
    bne @@failed
    mov r1,#2
    str r1,[r4,#0x10]
    mov r0,r4
    bl StateTimer_Reset
@@pending:
    mov r0,#0
    pop {r4,pc}
@@beginSave:
    ldr r3,[r4,#8]
    cmp r3,#0
    beq @@disconnect
    ldr r0,[r3,#0x74]
    ldr r2,[r3,#0xF0]
    cmp r0,#0
    cmnne r2,#0
    beq @@disconnect
    ldr r1,[r2,#4]
    cmp r1,#0
    beq @@disconnect
    ldrb r2,[r2,#8]
    bl LocalSettings_SetLanguage
    mov r0,r4
    mov r1,#0
    bl StateLocalSave_Begin
    mov r1,#1
    str r1,[r4,#0x10]
    mov r0,#0
    pop {r4,pc}
@@failed:
    mov r1,#3
    strb r1,[r4,#0x30]
    mov r0,#1
    pop {r4,pc}
@@disconnect:
    mov r0,r4
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq @@localDisconnect
    bl CombinePatch_DisconnectUpdateOfficial
    pop {r4,pc}
@@localDisconnect:
    bl OfflinePatch_DisconnectUpdate
    pop {r4,pc}
    .pool

// Load a local file, then resume the original metadata and selected-game
// callbacks at substate 6.
// 载入本地文件后，从原版子状态 6 继续元数据与所选游戏回调。
CombinePatch_BankDataSyncEntry:
    ldr r1,[r0,#0x10]
    cmp r1,#6
    bcs @@resumeNative
    push {r4,lr}
    mov r4,r0
    bl OfflinePatch_LoadBankData
    cmp r0,#0
    beq @@loadFailed
    mov r1,#6
    str r1,[r4,#0x10]
    mov r0,r4
    pop {r4,lr}
@@resumeNative:
    push {r4,r5,r6,lr}
    b BankDataSyncState_Update + 4
@@loadFailed:
    mov r0,r4
    mov r1,#3
    strb r1,[r0,#0x30]
    mov r0,#1
    pop {r4,pc}
    .pool

// The server callback fills a 32-byte transaction descriptor before the
// native state copies it into Turtle. Offline loading has no server descriptor,
// so preserve bytes 0x00-0x1B and update only transaction state 0 before using
// the unchanged native save callback. Download mode keeps the complete copy.
// 服务器回调会先填充 32 字节事务描述，随后原版状态才把它复制进 Turtle。离线
// 载入没有服务器描述，因此保留 0x00-0x1B，只把事务状态更新为 0，再沿用原版
// 保存回调；下载模式与解锁模式仍执行完整复制。
CombinePatch_BankDataSyncTurtleState0:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    ldr r0,[r4,#0x08]
    ldr r5,[r0,#0x74]
    mov r0,r5
    mov r1,#0
    bl TurtleRecord_SetTransactionState
    b BankDataSync_TurtleSaveBegin
@@native:
    ldr r0,[r4,#0x08]
    b BankDataSync_TurtleTransactionWrite + 4
    .pool

// Native save substates copy the server-generated descriptor together with
// states 2 or 1. The local stage operation deliberately has no such descriptor;
// retain the existing online identity/version fields while preserving the
// native intermediate state and Turtle-save sequence.
// 原版保存子状态会连同状态 2 或 1 一起复制服务器生成的描述。本地暂存操作并不
// 生成该描述，因此保留既有在线身份／版本字段，同时保留原版中间状态和 Turtle
// 保存顺序。
CombinePatch_SaveTurtleState2:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    ldr r0,[r4,#0x08]
    ldr r5,[r0,#0x74]
    mov r0,r5
    mov r1,#2
    bl TurtleRecord_SetTransactionState
    b BankSave_TurtleCommitSaveBegin
@@native:
    ldr r0,[r4,#0x08]
    b BankSave_TurtleCommitWrite + 4
    .pool

CombinePatch_SaveTurtleState1:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    ldr r0,[r4,#0x08]
    ldr r5,[r0,#0x74]
    mov r0,r5
    mov r1,#1
    bl TurtleRecord_SetTransactionState
    b BankSave_TurtleRollbackSaveBegin
@@native:
    ldr r0,[r4,#0x08]
    b BankSave_TurtleRollbackWrite + 4
    .pool

// C-owned paths and all feature payloads share the added pages.
// C 路径数据与全部功能荷载共同放在新增页内。
CombinePatch_PathDataBegin:
    .importobj BUILD_DIRECTORY + "/patch_paths.o"
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
    strb r1,[r12,#CombinePatch_TitleRPreviousOffset]
    strb r1,[r12,#CombinePatch_InitialLanguagePendingOffset]
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
    b BankUi_SetMessageLine
    .pool

// Reproduce the stock title-state update. R is sampled only while the title
// remains active. On the same title-exit frame produced by A or a touch input,
// the selected value is copied to the session value used by every later hook.
// 重现原版标题状态更新。只在标题仍处于活动状态时采样 R 键。A 键或触摸输入导致
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
    bne @@latch
    mov r0,r4
    bl CombinePatch_TitleProcessModeToggle
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

// Advance Offline -> Download -> Unlock -> Original -> Offline on each R rising edge and
// immediately rebind the bottom title help line. No caller after title exit
// invokes this routine.
// 每次 R 键上升沿按“离线→下载→解锁→原版→离线”循环切换，并立即重新绑定标题底部
// 帮助行。标题退出后不会有调用者再执行此例程。
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
    tst r1,#CombinePatch_KeyR
    movne r5,#1
@@sample:
    ldr r6,=CombinePatch_ModeStorage
    ldrb r1,[r6,#CombinePatch_TitleRPreviousOffset]
    strb r5,[r6,#CombinePatch_TitleRPreviousOffset]
    cmp r5,#0
    beq @@done
    cmp r1,#0
    bne @@done
    ldrb r1,[r6,#CombinePatch_TitleSelectedModeOffset]
    add r1,r1,#1
    cmp r1,#CombinePatch_ModeCount
    movcs r1,#CombinePatch_ModeOffline
    strb r1,[r6,#CombinePatch_TitleSelectedModeOffset]
    ldr r0,[r4,#0x38]
    cmp r0,#0
    beq @@done
    mov r1,#1
    mov r2,#0x10
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
@@done:
    pop {r4,r5,r6,pc}
    .pool

// The stock initializer has removed the previous view before this hook. Rebind
// the common title preview to SD only when returning from a native-save mode.
// 原版初始化器已销毁上一视图；从原存档模式返回时，将公共标题预览重新绑定到 SD。
CombinePatch_TitlePreview:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r0,[r12,#CombinePatch_SessionModeOffset]
    cmp r0,#CombinePatch_ModeOffline
    beq @@resume
    mov r2,#CombinePatch_ModeOffline
    strb r2,[r12,#CombinePatch_SessionModeOffset]
    ldr r0,[r4,#8]
    ldr r1,[r4,#0x0C]
    bl TurtleRedirect_PrepareSession
@@resume:
    ldr r5,[r4,#0x2C]
    b TitleScreenState_Initialize + 0x40
    .pool

// Only after title teardown may the record and localized resources switch.
// A fresh backend uses stock language selection and subsequent initialization, then
// continues to game checking without choosing the mode for a second time.
// 仅在标题销毁后切换记录与语言资源。新后端走原版语言选择及后续初始化状态，之后继续
// 检查游戏，不要求重新选择模式。
// The shared prompt view holds the language archive and font from startup.
// Finish its native asynchronous release while the title view is still alive;
// subsequent states recreate it after the selected record's language is applied.
// 公共提示视图保留启动时的文本包及字体。在标题视图仍有效时等待原版异步清理完成，
// 后续状态会在应用所选记录的语言后重新创建它。
CombinePatch_TitleReleaseWaitingView:
    ldr r4,[r5,#0x2C]
    mov r0,r4
    bl BankUi_ReleaseWaitingView
    cmp r0,#0
    beq TitleScreenState_ExitComplete + 8
    b TitleScreenState_ExitUiCleanup + 4

CombinePatch_TitleBindSession:
    ldr r0,[r5,#8]
    ldr r1,[r5,#0x0C]
    ldr r6,=CombinePatch_ModeStorage
    ldrb r2,[r6,#CombinePatch_SessionModeOffset]
    bl TurtleRedirect_PrepareSession
    eor r0,r0,#1
    strb r0,[r6,#CombinePatch_InitialLanguagePendingOffset]
    mov r0,#0
    strb r0,[r6,#CombinePatch_UnlockRecoveryOffset]
    mov r0,#1
    pop {r4,r5,r6,pc}
    .pool

CombinePatch_TitleResult:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_InitialLanguagePendingOffset]
    cmp r12,#0
    moveq r0,#3
    movne r0,#1
    b BankFlow_SelectNextState + 0xCC
    .pool

CombinePatch_TitleOrResume:
    cmp r1,#21
    bne @@title
    ldr r12,=CombinePatch_ModeStorage
    ldrb r2,[r12,#CombinePatch_InitialLanguagePendingOffset]
    cmp r2,#0
    beq @@title
    mov r2,#0
    strb r2,[r12,#CombinePatch_InitialLanguagePendingOffset]
    mov r0,#3
    b BankFlow_SelectNextState + 0xCC
@@title:
    mov r0,#2
    b BankFlow_SelectNextState + 0xCC
    .pool

CombinePatch_TurtleLoad:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_LoadBackend
    b TurtleStorage_LoadAtOnce
    .pool

CombinePatch_TurtleSave:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_SaveBackend
    b TurtleStorage_SaveAtOnce
    .pool

CombinePatch_TurtleCheck:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq TurtleRedirect_CheckBackend
    b TurtleStorage_CheckArchiveStatus
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

// In Offline Mode skip allocating the stock remote connection job. Download
// and Unlock modes replay the overwritten instructions before resuming it.
// 离线模式跳过原版远端连接任务的分配；下载模式与解锁模式会重放被覆盖的指令，再
// 继续原版流程。
CombinePatch_NetworkSkipRemoteJob:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,#0
    str r0,[r4,#0x38]
    b NetworkConnectionState_Initialize + 0x5C
@@native:
CombinePatch_NetworkSkipRemoteJobOfficial:
    ldr r1,[r4,#0x0C]
    mov r0,#0x2B0
    bl Allocator_Allocate
    b NetworkConnectionState_Initialize + 0x4C
    .pool

CombinePatch_NetworkUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b OfflinePatch_NetworkUpdate
@@native:
    push {r4-r6,lr}
    b NetworkConnectionState_Update + 4
    .pool

CombinePatch_InitialRemoteRecordUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b OfflinePatch_InitialRemoteRecordUpdate
@@native:
    push {r4-r6,lr}
    b InitialRemoteRecordState_Update + 4
    .pool

CombinePatch_FirstPresentDispatch:
    cmp r0,#0
    beq RewardReceive_FirstPresentPath
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    bne RewardReceive_FirstPresentBranch + 4
    // Skip both online-gift passes only for this offline state instance. Keep
    // the persistent update-gift flag intact for a later official session.
    // 仅在当前离线状态实例中跳过两轮联网礼物查询，保留持久化的更新礼物
    // 标志，供之后的原版联网会话使用。
    mov r0,#1
    str r0,[r4,#0x4C]
    mov r0,#0x1B
    str r0,[r4,#0x10]
    b RewardReceive_UpdateReturnPending
    .pool

CombinePatch_BankDataSyncDispatch:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b CombinePatch_BankDataSyncEntry
@@native:
    push {r4-r6,lr}
    b BankDataSyncState_Update + 4
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

// Both message sites in the post-selection state use the same mode dispatch;
// all surrounding stock UI, sound and animation instructions remain intact.
// 选定游戏后状态中的两个消息位置共用同一模式分派；周围原版 UI、声音和动画指令
// 全部保持不变。
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
    push {r4,lr}
    mov r4,r0
    bl OfflinePatch_PostSelectionConnectionUpdate
    mov r4,r0
    cmp r4,#0
    beq @@offlineReturn
    mov r0,r4
@@offlineReturn:
    pop {r4,pc}
@@native:
    cmp r12,#CombinePatch_ModeUnlock
    ldreq r1,=CombinePatch_PostSelectionConnectionUpdateOfficial
    beq UnlockMode_PostSelectionUpdate
CombinePatch_PostSelectionConnectionUpdateOfficial:
    push {r4-r6,lr}
    b PostSelectionConnectionState_Update + 4
    .pool

// Only Unlock Mode changes the acknowledged mismatch result. The flow
// controller still runs the native finalizer/destructor before creating state 10,
// the same state reached after confirming the first feature-menu entry.
// 只有解锁模式改动不匹配提示确认后的结果。流程控制器仍先执行原版收尾与析构，
// 再创建 state 10，与确认功能选单第一项后进入的状态相同。
CombinePatch_TransactionRecoveryUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeUnlock
    ldreq r1,=CombinePatch_TransactionRecoveryUpdateOfficial
    beq UnlockMode_RecoveryUpdate
CombinePatch_TransactionRecoveryUpdateOfficial:
    push {r4-r7,lr}
    b TransactionRecoveryState_Update + 4
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

CombinePatch_DisconnectUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq CombinePatch_DisconnectWithLanguageSave
    cmp r12,#CombinePatch_ModeOriginal
    beq @@native
    ldrb r12,[r0,#0x3C]
    cmp r12,#0
    beq @@native
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_InitialLanguagePendingOffset]
    cmp r12,#0
    beq CombinePatch_DisconnectWithLanguageSave
@@native:
CombinePatch_DisconnectUpdateOfficial:
    push {r4,lr}
    b DisconnectCleanupState_Update + 4
    .pool

CombinePatch_DisconnectSkipRemoteJob:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,#0
    str r0,[r4,#0x38]
    b DisconnectCleanupState_Initialize + 0x78
@@native:
CombinePatch_DisconnectSkipRemoteJobOfficial:
    ldr r1,[r4,#0x0C]
    mov r0,#0x2B0
    bl Allocator_Allocate
    b DisconnectCleanupState_Initialize + 0x68
    .pool

// These three sites preserve stock first-use creation in Download and Unlock
// modes while retaining the local first-use initializer and checked write in
// Offline mode.
// 三个位置在下载模式与解锁模式保留原版首次创建，在离线模式保留本地首次初始化与
// 检查写入。
CombinePatch_BankCreateState0:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,#1
    b BankCreateState_Update + 0x238
@@native:
CombinePatch_BankCreateState0Official:
    ldr r0,=BankRootPointerSlot
    ldr r1,[r4,#0x0C]
    b BankCreateState_Update + 0x60
    .pool

CombinePatch_BankCreateAfterMessages:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,#6
    b BankCreateState_Update + 0x238
@@native:
CombinePatch_BankCreateAfterMessagesOfficial:
    ldr r0,=BankRootPointerSlot
    ldr r5,[r0]
    b BankCreateState_Update + 0x1EC
    .pool

CombinePatch_CreateInitial:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_CreateInitial
    b BankRemote_CreateSerializedFile
    .pool

CombinePatch_BankCreateSuccess:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOriginal
    cmpne r12,#CombinePatch_ModeDownload
    moveq r0,#7
    movne r0,#8
    b BankCreateState_Update + 0x238
    .pool

CombinePatch_SelectPostSelectionState:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#17
    movne r0,#18
    pop {r4,pc}
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
    b CombinePatch_RedirectHomeToLanguage
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
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq BankFlow_SelectNextState + 0x3B4
    b BankFlow_SelectNextState + 0x370
    .pool

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
CombinePatch_SaveBegin:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,#1
    b BankSaveState_Update + 0x47C
@@native:
    ldr r0,=BankRootPointerSlot
    ldr r5,[r0]
    b BankSaveState_Update + 0x8C
    .pool

CombinePatch_SaveDisplayWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r0,r4
    bl OfflinePatch_SaveDisplayDelayUpdate
    b BankSaveState_Update + 0x5BC
@@native:
    ldrb r1,[r4,#0x48]
    cmp r1,#1
    bne BankSaveState_Update + 0x5BC
    b BankSaveState_Update + 0x110
    .pool

CombinePatch_SaveStage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b OfflinePatch_Stage
@@native:
    b BankRemote_StageFileUpdate
    .pool

CombinePatch_SaveCommit:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    // The native save state has already persisted transaction state 2. Once
    // the local file rotation succeeds, clear that now-obsolete remote marker
    // through the stock Turtle object and persist the regenerated record.
    // 原版保存状态此时已经持久化事务状态 2。本地文件轮换成功后，通过原版
    // Turtle 对象清除这个已失去意义的远端标记，并保存重新生成的记录。
    push {r4,lr}
    bl OfflinePatch_Commit
    cmp r0,#0
    beq @@return
    ldr r0,[r4,#0x08]
    ldr r1,[r0,#0x74]
    ldr r0,[r0,#0x78]
    bl TurtleRedirect_ClearTransactionAndSave
@@return:
    pop {r4,pc}
@@native:
    b BankRemote_CommitStagedUpdate
    .pool

CombinePatch_SaveCommitWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r1,#13
    str r1,[r4,#0x10]
    b BankSaveState_Update + 0x5BC
@@native:
    ldrb r1,[r4,#0x48]
    cmp r1,#1
    bne BankSaveState_Update + 0x5BC
    b BankSaveState_Update + 0x3D4
    .pool

CombinePatch_SaveRollback:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    // A completed local rollback also has no pending remote transaction. This
    // symmetrically clears native state 1 after bankdata.tmp is discarded.
    // 本地回滚完成后同样不存在待处理的远端事务；删除 bankdata.tmp 后，对称地
    // 清除原版状态 1。
    push {r4,lr}
    bl OfflinePatch_Rollback
    cmp r0,#0
    beq @@return
    ldr r0,[r4,#0x08]
    ldr r1,[r0,#0x74]
    ldr r0,[r0,#0x78]
    bl TurtleRedirect_ClearTransactionAndSave
@@return:
    pop {r4,pc}
@@native:
    b BankRemote_RollbackStagedUpdate
    .pool

CombinePatch_SaveRollbackWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    mov r1,#16
    str r1,[r4,#0x10]
    b BankSaveState_Update + 0x5BC
@@native:
    ldrb r1,[r4,#0x48]
    cmp r1,#1
    bne BankSaveState_Update + 0x5BC
    b BankSaveState_Update + 0x414
    .pool

// Support code and Mover/eShop are disabled in the first three modes. HOME is deliberately
// left native here so its existing Bank-flow hook can redirect it to language.
// 前三种模式禁用支持代码与 Mover/eShop；原版模式直接使用原回调。HOME 在这里保持原版，使 Bank
// 流程钩子能够把它重定向至语言选择。
CombinePatch_MenuSelectionCallback:
    ldr r2,=CombinePatch_ModeStorage
    ldrb r2,[r2,#CombinePatch_SessionModeOffset]
    cmp r2,#CombinePatch_ModeOriginal
    beq @@native
    ldr r2,[r0,#0x10]
    cmp r2,#1
    bne @@native
    cmp r1,#2
    beq @@disabled
    cmp r1,#3
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
    ldr r0,[r4,#8]
    ldr r0,[r0,#0xCC]
    bl BankdataRedirect_CaptureDownloaded
    cmp r0,#0
    beq @@restore
    ldr r1,=CombinePatch_ModeStorage
    mov r0,#DOWNLOAD_CAPTURE_COMPLETE
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

// Keep the remaining feature modules consecutive inside the added pages.
// 其余功能模块在新增页内连续排列。
CombinePatch_PayloadBegin:
FsHelpers_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/fs_helpers.o"
FsHelpers_PayloadEnd:
BankdataRedirect_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:
OfflineFlow_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/offline_flow.o"
OfflineFlow_PayloadEnd:
LocalMileage_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/local_mileage.o"
LocalMileage_PayloadEnd:
LocalTicket_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/local_ticket.o"
LocalTicket_PayloadEnd:
UnlockMode_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/unlock_mode.o"
UnlockMode_PayloadEnd:
CombinePatch_PayloadEnd:

// Choose the job backend once at initialization; subsequent interfaces read
// the stored result. Keep the native state, constructor and destructor.
// 初始化时选择作业后端，后续接口读取已保存的结果。保留原版状态、构造与析构。
.align 4
CombinePatch_TicketInitialize:
    ldr r1,[r4,#0x28]
    ldr r12,=CombinePatch_ModeStorage
    ldrb r2,[r12,#CombinePatch_SessionModeOffset]
    ldr r12,=CombinePatch_OnlineTicketFallback
    ldrb r3,[r12]
    b LocalTicket_InitializeSelected
CombinePatch_TicketPoll:
    ldr r12,=LocalTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne CombinePatch_TicketPollLocal
    b TicketJob_Poll
CombinePatch_TicketPollLocal:
    b LocalTicket_Poll
CombinePatch_TicketUnbind:
    ldr r12,=LocalTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne CombinePatch_TicketUnbindLocal
    b TicketJob_Unbind
CombinePatch_TicketUnbindLocal:
    // No network client was bound; the native destructor still runs afterwards.
    // 未绑定网络客户端；返回后仍执行原版析构。
    mov r0,#1
    bx lr
    .pool
CombinePatch_TicketWrappersEnd:
CombinePatch_OnlineTicketFallback:
    .byte ONLINE_TICKET_CHECK_BYPASS
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
    ldr r1,=CODE_EXPANSION_PAYLOAD_SIZE
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
    .importobj BUILD_DIRECTORY + "/code_expansion.o"
CodeExpansion_LoaderEnd:
.endarea

// Reserve the final 64 bytes of the mapped text segment as a zero-padded ASCII
// identifier. No runtime code reads it in the current version.
// 将已映射 text 段的最后 64 字节预留为零填充 ASCII 标识。当前版本没有运行时代码
// 读取它。
.org OfflinePatch_VersionStorageStart
.area OfflinePatch_VersionStorageSize, 0
OfflinePatch_VersionIdentifier:
    .asciiz "offline_patch_v1.0.0"
.endarea

.close
