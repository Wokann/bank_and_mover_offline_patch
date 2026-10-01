.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

// Offline behavior is the baseline. Mode wrappers branch to these native entry
// points when Download Mode was selected on the title screen.
// 离线行为作为基线；标题界面选定下载模式后，模式包装函数会跳转到这些原版入口。
.definelabel OfflinePatch_VersionStorageSize, 0x40
.definelabel OfflinePatch_VersionStorageStart, TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CombinePatch_CodeStart, 0x00313A40
.definelabel CombinePatch_CodeEnd, OfflinePatch_VersionStorageStart
.definelabel CombinePatch_PayloadStart, OptionalRewardState_Update + 4
.definelabel CombinePatch_PayloadEndLimit, 0x002B14E0
.definelabel TurtleRedirect_PayloadEndLimit, 0x002A8760
.definelabel CombinePatch_ModeStorage, 0x003ABFFC
.definelabel CombinePatch_HidManager, 0x003DC3B4
.definelabel CombinePatch_ModeOffline, 0
.definelabel CombinePatch_ModeDownload, 1
.definelabel CombinePatch_KeyR, 0x100
.definelabel CombinePatch_SessionModeOffset, 0
.definelabel CombinePatch_DownloadCapturedOffset, 1
.definelabel CombinePatch_TitleRPreviousOffset, 2
.definelabel CombinePatch_TitleSelectedModeOffset, 3
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
.definelabel CombinePatch_DownloadGameSelectionPrompt, 0x6C

.open "../rom/exefs/00040000000C9B00.dec.code", "../build/00040000000C9B00.dec.code", 0x00100000

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
.org OptionalRewardState_Update
    b CombinePatch_OptionalRewardBypassUpdate
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

.org BankFlow_PostSelectionMetadataPath
    b CombinePatch_SelectPostSelectionState
.org PostSelectionConnectionState_Update
    b CombinePatch_PostSelectionConnectionUpdate
.org PostSelectionConnectionState_Initialize + 0x0C
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org BankCreateConnection_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage
.org NetworkPreparation_MessageIdLoad
    bl CombinePatch_SelectPostSelectionConnectionMessage

// Keep the native first-present flag test. Offline mode bypasses only the
// remote BOSS gift lookup after that flag is set and resumes the native local
// mileage calculation. Download mode preserves the stock branch.
// 保留原版首次奖励标志判断。标志已设置后，离线模式只跳过远端 BOSS 礼物查询，
// 并恢复原版的本地里程计算；下载模式保留原分支。
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
.org BankFlow_SelectNextState + 0x34C
    beq CombinePatch_Result21

// Download mode ends the first ordinary-Bank use without reward, box, or save
// UI. Offline mode restores the native local points calculation and reward UI.
// 下载模式会在首次普通 Bank 使用后跳过奖励、盒子和保存 UI；离线模式恢复原版
// 本地里程计算与领取界面。
.org BankFlow_SelectNextState + 0x260
    beq CombinePatch_Result5
.org BankFlow_SelectNextState + 0x270
    beq CombinePatch_Result6Or12

// Do not enter menu-only operations whose state bodies are repurposed as code
// caves. Returning before the stock callback prologue leaves the feature menu
// visible and interactive.
// 不进入其状态函数体已被复用为代码空位的菜单操作。在原版回调序言前返回，使功能
// 选单保持显示和可操作。
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

// Capture only an ordinary-Bank successful server download while explicit
// download mode is active. The original callback resumes unchanged.
// 仅在明确的下载模式且普通 Bank 下载成功时保存服务器数据，随后不改变原版回调。
.org BankRemote_DownloadSuccessCallback + 0x14
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
    bl TurtleRedirect_LoadBackend
.org TurtleStorage_SaveBackendCall
    bl TurtleRedirect_SaveBackend
.org Title_TurtleStorageCheckCall
    bl TurtleRedirect_CheckBackend
.org Initial_TurtleStorageCheckCall
    bl TurtleRedirect_CheckBackend
.org Initial_TurtleStorageFormatCall
    bl TurtleRedirect_FormatBackend
.org Initial_TurtleStorageFormatPollCall
    bl TurtleRedirect_FormatPoll

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
// one short explanation for each mode; select it after the startup mode latch.
// 原版菜单说明使用 BMG 第 17 行。LayeredFS 档案为两种模式各追加一条简短说明，
// 在启动模式锁定后选择其中之一。
CombinePatch_ShowModeGreeting:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeDownload
    moveq r2,#CombinePatch_DownloadMenuGreeting
    movne r2,#CombinePatch_OfflineMenuGreeting
    b BankUi_ShowMessageLine
    .pool

// Select the static upper-screen guide message in the game-selection state,
// then let its original display call run unchanged. Download mode substitutes
// a dedicated explanation; offline mode keeps stock message 3.
// 在游戏选择状态中选择上屏静态说明文本，随后让原版显示调用保持不变。下载模式
// 替换为专用说明；离线模式保留原消息 3。
CombinePatch_SelectGameSelectionMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeDownload
    moveq r1,#CombinePatch_DownloadGameSelectionPrompt
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
    movne r12,#0x29
    msr cpsr_f,r3
    mov r3,r12
    bx lr
    .pool

// Preserve every stock menu label in original mode. The two patched modes
// substitute only the labels whose callbacks they deliberately redirect.
// 原版模式保留全部原始选单标签。两种补丁模式只替换其明确重定向回调的项目标签。
CombinePatch_SelectSupportMenuText:
    mov r8,#CombinePatch_DisabledMenuMessage
    bx lr

CombinePatch_SelectInstalledMoverMenuText:
    mov r5,#CombinePatch_DisabledMenuMessage
    bx lr

CombinePatch_SelectDownloadMoverMenuText:
    mov r6,#CombinePatch_DisabledMenuMessage
    bx lr

CombinePatch_SelectHomeMenuTextR6:
    mov r6,#CombinePatch_LanguageMenuMessage
    bx lr

CombinePatch_SelectHomeMenuTextR5:
    mov r5,#CombinePatch_LanguageMenuMessage
    bx lr
    .pool

CombinePatch_SelectDisconnectMessage:
    ldrb r1,[r4,#0x3C]
    ldr r2,=CombinePatch_ModeStorage
    ldrb r3,[r2,#CombinePatch_SessionModeOffset]
    cmp r1,#0
    movne r1,#CombinePatch_BlankMessage
    bxne lr
    cmp r3,#CombinePatch_ModeDownload
    movne r1,#CombinePatch_OfflineDisconnectMessage
    bxne lr
    ldrb r2,[r2,#CombinePatch_DownloadCapturedOffset]
    cmp r2,#0
    moveq r1,#0x0D
    movne r1,#CombinePatch_DownloadSuccessMessage
    bx lr

// State 21 persists a changed language before using the local disconnect
// delay. State 20 uses OfflinePatch_DisconnectUpdate directly.
// state 21 会先持久化变更后的语言，再进入本地断开延时；state 20 则直接使用
// OfflinePatch_DisconnectUpdate。
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
// 保存回调；下载模式仍执行完整复制。
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

// C-owned path data occupies this second verified injection region.
// C 后端使用的路径数据放在第二个已验证注入区。
CombinePatch_PathDataBegin:
    .importobj "../build/patch_paths.o"
CombinePatch_PathDataEnd:
.endarea

// Reserve the final 64 bytes of the mapped text segment as a zero-padded ASCII
// identifier. No runtime code reads it in the current version.
// 将已映射 text 段的最后 64 字节预留为零填充 ASCII 标识。当前版本没有运行时代码
// 读取它。
.org OfflinePatch_VersionStorageStart
.area OfflinePatch_VersionStorageSize, 0
OfflinePatch_VersionIdentifier:
    .asciiz "offline_patch_v0.9.0"
.endarea

// The outer Bank-flow hook redirects HOME to language selection before this
// state is created. Its full state body is therefore available for mode dispatch.
// 外层 Bank 流程钩子会在创建此状态前将 HOME 重定向至语言选择，因此其完整状态体
// 可用于模式分发。
.org HomeTransferState_Update
.area TurtleRedirect_PayloadEndLimit-HomeTransferState_Update

// Reset the title selector whenever its UI is created, then replace the stock
// bottom HOME-help line. The narrow version pane remains completely native.
// 每次创建标题界面 UI 时重置模式选择器，随后替换原版底部 HOME 帮助行。狭窄的
// 版本号窗格保持完全原版。
CombinePatch_TitleModeTextInitialize:
    ldr r12,=CombinePatch_ModeStorage
    mov r1,#CombinePatch_ModeOffline
    strb r1,[r12,#CombinePatch_SessionModeOffset]
    strb r1,[r12,#CombinePatch_DownloadCapturedOffset]
    strb r1,[r12,#CombinePatch_TitleRPreviousOffset]
    strb r1,[r12,#CombinePatch_TitleSelectedModeOffset]
    mov r1,#1
    mov r2,#0x10
    mov r3,#CombinePatch_TitleModeOfflineMessage
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

// Toggle only on an R rising edge and immediately rebind the bottom title
// help line. No caller after title exit invokes this routine.
// 只在 R 键上升沿切换，并立即重新绑定标题底部帮助行。标题退出后不会有调用者
// 再执行此例程。
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
    eor r1,r1,#1
    strb r1,[r6,#CombinePatch_TitleSelectedModeOffset]
    ldr r0,[r4,#0x38]
    cmp r0,#0
    beq @@done
    mov r1,#1
    mov r2,#0x10
    ldrb r3,[r6,#CombinePatch_TitleSelectedModeOffset]
    cmp r3,#CombinePatch_ModeDownload
    moveq r3,#CombinePatch_TitleModeDownloadMessage
    movne r3,#CombinePatch_TitleModeOfflineMessage
    bl BankUi_SetMessageLine
@@done:
    pop {r4,r5,r6,pc}
    .pool

// Connection availability reads only the session mode latched as the title
// exits. It never samples R or changes the selected title mode.
// 连接可用性只读取标题退出时锁定的会话模式。它绝不采样 R 键，也不改变标题所选模式。
CombinePatch_NetworkAvailability:
    push {r1-r3,lr}
    ldr r2,=CombinePatch_ModeStorage
    mov r3,#0
    strb r3,[r2,#CombinePatch_DownloadCapturedOffset]
    ldrb r1,[r2,#CombinePatch_SessionModeOffset]
    cmp r1,#CombinePatch_ModeOffline
    moveq r0,#1
    popeq {r1-r3,pc}
    pop {r1-r3,lr}
    b 0x001103CC
    .pool

// In offline mode skip allocating the stock remote connection job. In download
// mode replay the three overwritten allocation instructions before resuming it.
// 离线模式跳过原版远端连接任务的分配；下载模式会重放三条被覆盖的分配指令，再继续原版。
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

// State ID 15 combines remote entitlement and campaign/online-gift checks.
// Both modes complete it locally, disabling free_campaign and online gifts
// while supplying the runtime entitlement fields needed downstream. This is
// separate from the native local mileage calculation in states 12 and 13.
// state 15 组合了远端使用权以及活动／联网礼物检查。两种模式都让它在本地
// 完成，以禁用 free_campaign 与在线礼物，同时提供后续所需的运行时使用权字段。
// 它与 state 12/13 的原版本地里程计算相互独立。
CombinePatch_OptionalRewardBypassUpdate:
    b OfflinePatch_OptionalRewardBypassUpdate

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

// Hook before the native conditional branch, matching the proven Step 1
// download patch. Ordinary Bank uses the download message only in download
// mode; offline mode uses its local-data message. Preserve the initializer's
// remaining UI setup and return at +0x2C.
// 在原版条件分支前挂钩，与已验证的第一步下载补丁保持一致。普通 Bank 仅在下载
// 模式使用下载文本；离线模式使用本地数据文本。保留初始化器其余 UI 设置并从
// +0x2C 返回。
CombinePatch_BankDataSyncInitialize:
    mov r3,r1
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeDownload
    bne @@offline
    cmp r3,#0
    moveq r1,#CombinePatch_DownloadProgressMessage
    movne r1,#0x0E
    b @@show
@@offline:
    mov r1,#CombinePatch_OfflineBankConnectMessage
@@show:
    sub sp,sp,#8
    str r1,[sp]
    bl MessageUi_Begin
    ldr r1,[sp]
    add sp,sp,#8
    ldr r0,[r4,#0x38]
    b BankDataSyncState_Initialize + 0x2C
    .pool

// Keep the stock Internet and save messages in download mode. Only the offline
// route selects the appended local-data messages.
// 下载模式保留原版联网与保存文本；只有离线路线选择追加的本地数据文本。
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
    cmp r12,#CombinePatch_ModeDownload
    moveq r1,#0x0E
    movne r1,#CombinePatch_OfflineBankConnectMessage
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
    push {r4-r6,lr}
    b PostSelectionConnectionState_Update + 4
    .pool

CombinePatch_DisconnectUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    b CombinePatch_DisconnectWithLanguageSave
@@native:
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

// These three sites preserve stock first-use creation in download mode while
// retaining the local first-use initializer and checked write in offline mode.
// 三个位置在下载模式保留原版首次创建，在离线模式保留本地首次初始化与检查写入。
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
    mov r0,#8
    b BankCreateState_Update + 0x238

CombinePatch_SelectPostSelectionState:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#17
    movne r0,#18
    pop {r4,pc}
    .pool

// After an ordinary Bank download, offline mode keeps the native local mileage
// state. Download mode has already captured the complete Bank file and follows
// the stock no-save return state without entering mileage or the Bank Box.
// 普通 Bank 下载完成后，离线模式保留原版本地里程状态；下载模式已经捕获完整
// Bank 文件，直接进入原版“不保存并返回”状态，不进入里程或 Bank 盒子。
CombinePatch_RewardResult:
    ldr r1,=CombinePatch_ModeStorage
    ldrb r1,[r1,#CombinePatch_SessionModeOffset]
    cmp r1,#CombinePatch_ModeDownload
    moveq r0,#19
    movne r0,#12
    pop {r4,pc}
    .pool

CombinePatch_HomeResult:
    b CombinePatch_RedirectHomeToLanguage

CombinePatch_Result21:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeDownload
    beq BankFlow_SelectNextState + 0x370
    b BankFlow_SelectNextState + 0x3B4
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

// Download mode must not inherit any of the offline save substitutions. The
// native branches below replay exactly the instructions overwritten at each
// hook and then resume the original save state.
// 下载模式不能继承任何离线保存替换。下面的原版分支会重放各钩子处被覆盖的指令，
// 然后回到原版保存状态。
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

// Support code and Mover/eShop are disabled in both modes. HOME is deliberately
// left native here so its existing Bank-flow hook can redirect it to language.
// 两种模式都禁用支持代码与 Mover/eShop。HOME 在这里故意保持原版，使现有 Bank
// 流程钩子能够把它重定向至语言选择。
CombinePatch_MenuSelectionCallback:
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

// Preserve all callback inputs and flags, stage the complete returned object,
// then atomically commit it. A failed local capture never changes the stock
// callback result.
// 保留所有回调输入和标志，暂存完整返回对象后原子提交。本地捕获失败不改变原版
// 回调结果。
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
    bne @@restore
    mov r0,#0
    mov r1,r7
    ldr r2,=BANK_FILE_SIZE
    mov r3,#0
    bl OfflinePatch_Stage
    cmp r0,#0
    beq @@restore
    mov r0,#0
    mov r1,#0
    mov r2,#0
    bl OfflinePatch_Commit
    cmp r0,#0
    beq @@restore
    ldr r1,=CombinePatch_ModeStorage
    mov r0,#1
    strb r0,[r1,#1]
@@restore:
    ldr r12,[sp]
    msr cpsr_f,r12
    add sp,sp,#8
    pop {r0-r3,r12,lr}
    ldr r8,=0x000BB528
    bx lr
    .pool

// Import the C implementation of the redirected Turtle-record backend into
// the remaining verified HOME code cave. Hook sites above remain assembly-only.
// 将 Turtle 记录重定向后端的 C 实现导入剩余的已验证 HOME 代码空位；上方
// hook 点仍仅保留汇编。
TurtleRedirect_PayloadBegin:
    .importobj "../build/turtle_redirect.o"
TurtleRedirect_PayloadEnd:
.endarea

// Both patched routes complete this optional-reward state through the local
// bypass. Its original body is therefore unreachable and is the verified
// contiguous home for the shared FS, Bankdata, offline-flow, local-mileage,
// and local-ticket objects.
// 两条补丁路由都会通过本地跳过逻辑完成这个可选奖励状态。因此原函数体不可达，是
// 共用 FS、Bankdata、离线流程、本地里程输入与本地票据对象经验证的连续容器。
.org CombinePatch_PayloadStart
.area CombinePatch_PayloadEndLimit-CombinePatch_PayloadStart
CombinePatch_PayloadBegin:
FsHelpers_PayloadBegin:
    .importobj "../build/fs_helpers.o"
FsHelpers_PayloadEnd:
BankdataRedirect_PayloadBegin:
    .importobj "../build/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:
OfflineFlow_PayloadBegin:
    .importobj "../build/offline_flow.o"
OfflineFlow_PayloadEnd:
LocalMileage_PayloadBegin:
    .importobj "../build/local_mileage.o"
LocalMileage_PayloadEnd:
LocalTicket_PayloadBegin:
    .importobj "../build/local_ticket.o"
LocalTicket_PayloadEnd:
CombinePatch_PayloadEnd:
.endarea

.close
