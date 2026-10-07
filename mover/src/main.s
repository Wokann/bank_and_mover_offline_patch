.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

// 0: require native online tickets (public build); 1: auto-fallback only when
// identified Azahar lacks the ticket interface (test build). Offline stays local.
// 0：联网必须使用真实票务接口（公开版）；1：仅确认 Azahar 缺接口时自动使用
// 本地结果（测试版）。离线模式始终使用本地结果。
.definelabel ONLINE_TICKET_CHECK_BYPASS, 0

.definelabel OfflinePatch_VersionStorageSize,   0x40
.definelabel OfflinePatch_VersionStorageStart,  TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CombinePatch_CodeStart, CodeExpansion_PayloadStart
.definelabel CombinePatch_CodeEnd, CodeExpansion_PayloadStart + CodeExpansion_PayloadSize
.definelabel CombinePatch_OfflinePayloadEnd, CombinePatch_CodeEnd
.definelabel CombinePatch_ModeStorage,         OfflinePatch_RuntimeStorageStart + 4
.definelabel CombinePatch_ModeOffline,         0
.definelabel CombinePatch_ModeOnline,          1
.definelabel CombinePatch_SessionModeOffset,   0
.definelabel CombinePatch_SelectedModeOffset,  1
.definelabel CombinePatch_TitleRPreviousOffset,2
.definelabel CombinePatch_TitleOfflineMessage, 67
.definelabel CombinePatch_TitleOnlineMessage,  68
.definelabel CombinePatch_InitialConnectMessage,69
.definelabel CombinePatch_BankConnectMessage,  70
.definelabel CombinePatch_SaveMessage,         71
.definelabel CombinePatch_DisconnectMessage,   72

.open INPUT_CODE, OUTPUT_CODE, 0x00100000

.org MoverStartup_ConstructorCall
    bl CodeExpansion_Startup

// Title mode selection. The displayed R uses the same private-use button glyph
// as the Bank patch and is independent of the native input bit.
// 标题模式选择。画面中的 R 使用与 Bank 补丁相同的专用区按键字形，并与原生
// 输入位相互独立。
.org MoverTitleUi_HomeMessageCall
    bl CombinePatch_TitleTextInitialize
.org MoverTitleState_Update
    b CombinePatch_TitleStateUpdate

// Every offline hook is routed through a mode-aware wrapper. Online mode
// replays the exact overwritten instruction or enters the untouched function.
// 所有离线钩子均经由模式包装函数分派。在线模式会精确重放被覆盖指令，或进入
// 未修改函数的后续位置。
.org MoverNetworkState_Update
    b CombinePatch_NetworkUpdate
.org MoverNetwork_AvailabilityCall
    bl CombinePatch_NetworkAvailability
.org MoverNetwork_SkipRemoteJob
    b CombinePatch_NetworkSkipRemoteJob
.org MoverNetworkState_Initialize + 0x68
    bl CombinePatch_SelectInitialConnectMessage
.org MoverTicketState_TicketInitializeCall
    bl CombinePatch_TicketInitialize
.org MoverTicketState_TicketPollCall
    bl CombinePatch_TicketPoll
.org MoverTicketState_CampaignRequestCall
    bl CombinePatch_TicketCampaignRequest
.org MoverTicketState_TicketUnbindCall
    bl CombinePatch_TicketUnbind
.org MoverRemoteCheckState_Update
    b CombinePatch_RemoteCheckUpdate
.org MoverTransferEligibilityState_Update
    b CombinePatch_EligibilityUpdate
.org MoverGetPokemonState_Update
    b CombinePatch_GetPokemonUpdate
.org MoverGetPokemon_BankMessageIdLoad
    bl CombinePatch_SelectBankConnectMessage
.org MoverGetPokemon_SkipGen5RemoteValidation
    b CombinePatch_Gen5Validation
.org MoverGetPokemon_SkipGen12RemoteValidation
    b CombinePatch_Gen12Validation
.org MoverNoTransferState_Update
    b CombinePatch_NoTransferUpdate
.org MoverNoTransfer_BankMessageIdLoad
    bl CombinePatch_SelectBankConnectMessage
.org MoverDisconnectState_Update
    b CombinePatch_DisconnectUpdate
.org MoverDisconnectState_Initialize + 0x54
    bl CombinePatch_SelectDisconnectMessage
.org MoverDisconnect_SkipRemoteJob
    b CombinePatch_DisconnectSkipRemoteJob
.org MoverFlow_RemoteCheckSuccessState
    beq CombinePatch_RemoteCheckSuccessState

// Shared source discovery keeps the stock list and save-validation state, but
// supplies SD-backed Gen 5 candidates at the three native card I/O edges in
// both session modes. Physical-card winners continue through exact prologue
// trampolines into the untouched implementations.
// 公共来源发现保留原版列表与存档校验状态，并在两种会话模式中通过三个原生卡带 I/O
// 边界提供 SD 上的第五世代候选。实体卡带胜出项经精确序言跳板进入未修改实现。
.org MoverSourceList_UpdateVtableSlot
    .word NdsSources_ListUpdate + 1
.org MoverSourceSelection_IdCompare
    bl CombinePatch_SelectNdsSource
.org MoverSourceList_GameTitleCall
    bl CombinePatch_SelectNdsGameTitle
.org MoverVcSource_IncrementCount
    bl CombinePatch_VcSourceAdded
.org MoverNds_ReadSave
    b CombinePatch_NdsReadSaveEntry
.org MoverNds_ReadGameCode
    b CombinePatch_NdsReadGameCodeEntry
.org MoverNds_WriteSave
    b CombinePatch_NdsWriteSaveEntry

.org MoverSave_SkipRemoteJob
    b CombinePatch_SaveSkipRemoteJob
.org MoverSave_StageCall
    bl CombinePatch_Stage
.org MoverSave_StageWaitState
    b CombinePatch_SaveWait
.org MoverSave_CommitCall
    bl CombinePatch_Commit
.org MoverSave_AfterCommitState
    bne CombinePatch_AfterCommit
.org MoverSave_RollbackCall
    bl CombinePatch_Rollback
.org MoverSave_AfterRollbackState
    bne CombinePatch_AfterRollback
.org 0x0024A6F0
    bl CombinePatch_SelectSaveMessage

.org CombinePatch_CodeStart
.area CombinePatch_CodeEnd-CombinePatch_CodeStart

CombinePatch_TitleTextInitialize:
    // Preserve the chosen mode across title returns; fresh-launch storage is zero.
    // 返回标题时保留所选模式；首次启动时存储区为零，默认离线。
    ldr r12,=CombinePatch_ModeStorage
    mov r3,#CombinePatch_ModeOffline
    strb r3,[r12,#CombinePatch_SessionModeOffset]
    strb r3,[r12,#CombinePatch_TitleRPreviousOffset]
    ldrb r3,[r12,#CombinePatch_SelectedModeOffset]
    cmp r3,#CombinePatch_ModeOnline
    moveq r3,#CombinePatch_TitleOnlineMessage
    movne r3,#CombinePatch_TitleOfflineMessage
    b MoverUi_SetMessageLine

// Reproduce the complete stock six-instruction title update. The chosen mode
// becomes the immutable session mode only on the frame that exits the title.
// While the title remains active, sample Mover's own HID ring buffer directly;
// child UI callbacks do not receive R on this screen.
// 重现原版标题更新的全部六条指令。只有标题退出的那一帧，当前选择才会成为
// 本次流程中不再变化的会话模式。标题仍活动时直接读取 Mover 自己的 HID 环形
// 缓冲区；此画面的子 UI 回调不会接收 R。
CombinePatch_TitleStateUpdate:
    push {r4,lr}
    mov r4,r0
    ldrb r1,[r4,#0x3C]
    cmp r1,#0
    ldreqb r0,[r4,#0x3D]
    cmpeq r0,#0
    bne @@latch
    mov r0,r4
    bl CombinePatch_TitleProcessModeToggle
    mov r0,#0
    pop {r4,pc}
@@latch:
    ldr r1,=CombinePatch_ModeStorage
    ldrb r2,[r1,#CombinePatch_SelectedModeOffset]
    strb r2,[r1,#CombinePatch_SessionModeOffset]
    mov r0,#1
    pop {r4,pc}
    .pool

// Toggle on an R rising edge and immediately rebind the existing title HOME
// text pane. The key mask is the native 3DS R bit (0x100); the displayed R uses
// the localized font's private-use button glyph.
// 仅在 R 键上升沿切换，并立即重新绑定标题界面现有的 HOME 文本窗格。按键掩码
// 使用 3DS 原生 R 位 0x100；画面中的 R 使用本地化字体的专用区按键字形。
CombinePatch_TitleProcessModeToggle:
    push {r4,r5,r6,lr}
    mov r4,r0
    mov r5,#0
    ldr r0,=MoverHidManager
    ldr r0,[r0,#4]
    cmp r0,#0
    beq @@sample
    ldr r1,[r0,#0x10]
    cmp r1,#7
    bhi @@sample
    add r1,r0,r1,lsl #4
    ldr r1,[r1,#0x28]
    tst r1,#0x100
    movne r5,#1
@@sample:
    ldr r6,=CombinePatch_ModeStorage
    ldrb r1,[r6,#CombinePatch_TitleRPreviousOffset]
    strb r5,[r6,#CombinePatch_TitleRPreviousOffset]
    cmp r5,#0
    beq @@done
    cmp r1,#0
    bne @@done
    ldrb r3,[r6,#CombinePatch_SelectedModeOffset]
    eor r3,r3,#1
    strb r3,[r6,#CombinePatch_SelectedModeOffset]
    ldr r0,[r4,#0x38]
    cmp r0,#0
    beq @@done
    cmp r3,#CombinePatch_ModeOnline
    moveq r3,#CombinePatch_TitleOnlineMessage
    movne r3,#CombinePatch_TitleOfflineMessage
    mov r1,#1
    mov r2,#0x10
    bl MoverUi_SetMessageLine
@@done:
    pop {r4,r5,r6,pc}
    .pool

CombinePatch_NetworkAvailability:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#1
    bxeq lr
    b 0x0010F6CC

CombinePatch_NetworkSkipRemoteJob:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,#0
    str r0,[r4,#0x38]
    b MoverNetworkState_Initialize + 0x5C
@@original:
    ldr r1,[r4,#0x0C]
    b MoverNetwork_SkipRemoteJob + 4
    .pool

CombinePatch_NetworkUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_NetworkUpdate + 1
    bxeq r12
    push {r4-r6,lr}
    b MoverNetworkState_Update + 4

CombinePatch_RemoteCheckUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_RemoteCheckUpdate + 1
    bxeq r12
    push {r4-r8,lr}
    b MoverRemoteCheckState_Update + 4

CombinePatch_EligibilityUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_EligibilityEntry
    push {r4-r6,lr}
    b MoverTransferEligibilityState_Update + 4

CombinePatch_GetPokemonUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_GetPokemonEntry
    push {r4-r11,lr}
    b MoverGetPokemonState_Update + 4

CombinePatch_NoTransferUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_NoTransferUpdate + 1
    bxeq r12
    push {r4-r6,lr}
    b MoverNoTransferState_Update + 4

CombinePatch_DisconnectUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_DisconnectUpdate + 1
    bxeq r12
    push {r4,lr}
    b MoverDisconnectState_Update + 4

CombinePatch_Gen5Validation:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,r5
    ldr r12,=OfflinePatch_PrepareGen5Validation + 1
    blx r12
    cmp r0,#0
    beq MoverGetPokemon_UpdateWaitingReturn
    b MoverGetPokemon_Gen5LocalContinuation
@@original:
    ldr r0,=0x003293A8
    b MoverGetPokemon_SkipGen5RemoteValidation + 4

CombinePatch_Gen12Validation:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,r5
    ldr r12,=OfflinePatch_PrepareGen12Validation + 1
    blx r12
    b MoverGetPokemon_Gen12LocalContinuation
@@original:
    ldr r0,=0x003293A8
    b MoverGetPokemon_SkipGen12RemoteValidation + 4

CombinePatch_DisconnectSkipRemoteJob:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,#0
    str r0,[r4,#0x38]
    b MoverDisconnectState_Initialize + 0x78
@@original:
    ldr r1,[r4,#0x0C]
    b MoverDisconnect_SkipRemoteJob + 4

CombinePatch_RemoteCheckSuccessState:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#11
    movne r0,#14
    b 0x00242C4C

CombinePatch_SaveSkipRemoteJob:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,#1
    b 0x0024A4C0
@@original:
    ldr r0,=0x003293A8
    b MoverSave_SkipRemoteJob + 4

CombinePatch_Stage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_Stage + 1
    bxeq r12
    b 0x0023E9EC

CombinePatch_SaveWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,r4
    ldr r12,=OfflinePatch_SaveDisplayDelayUpdate + 1
    blx r12
    b MoverSave_UpdateEpilogue
@@original:
    ldrb r1,[r4,#0x44]
    b MoverSave_StageWaitState + 4

CombinePatch_Commit:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_Commit + 1
    bxeq r12
    b 0x0019B91C

CombinePatch_AfterCommit:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#0x0E
    movne r0,#0x0B
    b 0x0024A4C0

CombinePatch_Rollback:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=OfflinePatch_Rollback + 1
    bxeq r12
    b 0x0019B7D0

CombinePatch_AfterRollback:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#0x11
    movne r0,#0x0D
    b 0x0024A4C0

// Select appended offline lines without replacing any stock message. Online
// mode keeps the unmodified message IDs and therefore the original resources.
// 选择追加的离线文本，不替换任何原版消息。在线模式保持原消息编号，因此继续
// 使用原始资源。
CombinePatch_SelectInitialConnectMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_InitialConnectMessage
    movne r1,#13
    bx lr

CombinePatch_SelectBankConnectMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_BankConnectMessage
    movne r1,#15
    bx lr

CombinePatch_SelectSaveMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_SaveMessage
    movne r1,#9
    bx lr

CombinePatch_SelectDisconnectMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r1,#CombinePatch_DisconnectMessage
    movne r1,#14
    bx lr
    .pool

// Select once at job initialization; poll, campaign and cleanup use the stored
// backend. Tail calls preserve the original BL return address and aligned stack.
// 作业初始化时仅选择一次；轮询、活动和清理沿用保存的后端。
// 尾调用保留原 BL 返回地址与栈对齐。
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
CombinePatch_TicketCampaignRequest:
    ldr r12,=LocalTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne CombinePatch_TicketCampaignRequestLocal
    b MoverCampaign_Request
CombinePatch_TicketCampaignRequestLocal:
    mov r0,r4
    b LocalTicket_CampaignResult
CombinePatch_TicketUnbind:
    ldr r12,=LocalTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne CombinePatch_TicketUnbindLocal
    b TicketJob_Unbind
CombinePatch_TicketUnbindLocal:
    // No network client was bound; the native destructor still follows.
    // 未绑定网络客户端；返回后仍执行原版析构。
    mov r0,#1
    bx lr
    .pool
CombinePatch_TicketWrappersEnd:
CombinePatch_OnlineTicketFallback:
    .byte ONLINE_TICKET_CHECK_BYPASS
    .align 4

// Preserve the selected source ID and restore the original comparison flags
// after pinning the associated SD path.
// 固定相应 SD 路径后保留所选来源 ID，并恢复原比较指令的标志位。
CombinePatch_SelectNdsSource:
    push {r1-r3,lr}
    ldr r12,=NdsSources_SelectListId + 1
    blx r12
    pop {r1-r3,lr}
    cmp r0,#5
    bx lr

// The native caller keeps the source-list state in r4. Replace only r3's
// title message ID, then tail-call the stock pane/font formatting function.
// 原调用点将来源列表状态保存在 r4。仅替换 r3 中的游戏名消息编号，再尾调用原版
// 文本框／字体格式化函数。
CombinePatch_SelectNdsGameTitle:
    push {r0-r2,lr}
    mov r0,r4
    mov r1,r3
    ldr r12,=NdsSources_TitleMessage + 1
    blx r12
    mov r3,r0
    pop {r0-r2,lr}
    b MoverUi_SetMessageLine

// The stock 40-entry array assumes at most one Gen 5 cartridge plus 39 VC
// sources. Shared SD discovery can contribute four Gen 5 entries, so end the
// native VC scan as soon as the combined count reaches 40 in either mode.
// 原版 40 项数组按“最多一个第五世代卡带＋39 个 VC 来源”设计。公共 SD 来源发现可加入
// 四个第五世代来源，因此两种模式都会在总数达到 40 时结束原生 VC 扫描。
CombinePatch_VcSourceAdded:
    strh r0,[r6,#0x58]
    ldrh r1,[r6,#0x5A]
    add r1,r1,r0
    cmp r1,#40
    movhs r7,#39
    bx lr
    .pool

// Exact entry trampolines for the three stock card operations.
// 三个原版卡带操作的精确入口跳板。
NdsSources_OriginalReadSave:
    push {r3,lr}
    b MoverNds_ReadSave + 4
NdsSources_OriginalReadGameCode:
    push {r4-r7,lr}
    b MoverNds_ReadGameCode + 4
NdsSources_OriginalWriteSave:
    push {r0-r3,r4-r11,lr}
    b MoverNds_WriteSave + 4

// ARM entry veneers switch to the compact Thumb implementations without
// changing the three stock call sites' ABI.
// ARM 入口跳板切换到精简的 Thumb 实现，同时保持三个原版调用点的 ABI 不变。
CombinePatch_NdsReadSaveEntry:
    ldr r12,=NdsSources_ReadSave + 1
    bx r12
CombinePatch_NdsReadGameCodeEntry:
    ldr r12,=NdsSources_ReadGameCode + 1
    bx r12
CombinePatch_NdsWriteSaveEntry:
    ldr r12,=NdsSources_WriteSave + 1
    bx r12
    .pool

// Bankdata and SD sources share one filesystem implementation. TLS/SVC
// primitives remain ARM; the checked file operations use Thumb.
// Bankdata 与 SD 来源共用一份文件系统实现。TLS／SVC 原语保留 ARM，其余带检查
// 的文件操作采用 Thumb。
FsHelpers_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/fs_helpers.o"
FsHelpers_PayloadEnd:

NdsSources_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/nds_sources.o"
NdsSources_PayloadEnd:

CombinePatch_CodeUsedEnd:
.endarea

// Luma installs its LayeredFS payload at the declared text end. Leave 0x130
// bytes untouched, then align this project's tail to 0x10 bytes.
// Luma 从声明的 text 末尾安装 LayeredFS 荷载。保留 0x130 字节不写入，再将
// 本项目尾部荷载按 0x10 字节对齐。
.org TextActualEnd
.area 0x130
.endarea

// The loader remains inside original executable padding. It preserves the
// native startup call's registers and LR, then resumes the constructor walker.
// 加载器仅使用原可执行填充。保留启动调用的寄存器与 LR 后继续原版构造函数遍历。
.org 0x0028D2E0
.area OfflinePatch_VersionStorageStart - 0x0028D2E0
CodeExpansion_Startup:
    push {r0-r12,lr}
    ldr r0,=CodeExpansion_PayloadStart
    ldr r1,=CodeExpansion_PayloadSize
    bl CodeExpansion_Enable
    cmp r0,#0
    bne @@failure
    pop {r0-r12,lr}
    ldr pc,=MoverStartup_RunConstructors + 1
@@failure:
    mov r0,#0
    swi 0x3C
    b @@failure
    .pool
CodeExpansion_LoaderBegin:
    .importobj BUILD_DIRECTORY + "/code_expansion.o"
CodeExpansion_LoaderEnd:
.endarea

.org CombinePatch_CodeUsedEnd
CombinePatch_OfflinePayloadStart:
.area CombinePatch_OfflinePayloadEnd-CombinePatch_OfflinePayloadStart
OfflinePatch_EligibilityEntry:
    ldr r1,[r0,#0x10]
    cmp r1,#7
    bhs @@resumeNative
    ldr r12,=OfflinePatch_EligibilityUpdate + 1
    bx r12
@@resumeNative:
    push {r4-r6,lr}
    b MoverTransferEligibilityState_Update + 4

OfflinePatch_GetPokemonEntry:
    push {r0,lr}
    mov r1,#125
    lsl r1,r1,#4
    bl StateTimer_HasElapsed
    cmp r0,#0
    pop {r0,lr}
    moveq r0,#0
    bxeq lr
    push {r4-r11,lr}
    b MoverGetPokemonState_Update + 4
    .pool

// Keep all remaining feature modules consecutive inside the added pages.
// 其余功能模块在新增页内连续排列。
LocalTicket_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/local_ticket.o"
LocalTicket_PayloadEnd:

BankdataRedirect_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:

OfflineFlow_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/offline_flow.o"
OfflineFlow_PayloadEnd:

PatchPaths_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/patch_paths.o"
PatchPaths_PayloadEnd:

LocalValidation_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/local_validation.o"
LocalValidation_PayloadEnd:
NdsFs_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/nds_fs.o"
NdsFs_PayloadEnd:
CombinePatch_OfflinePayloadUsedEnd:
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
