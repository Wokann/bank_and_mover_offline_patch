.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

.definelabel OfflinePatch_VersionStorageSize,   0x40
.definelabel OfflinePatch_VersionStorageStart,  TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CombinePatch_CodeStart, MoverCombine_CodeCaveStart
.definelabel CombinePatch_CodeEnd, MoverCombine_CodeCaveEnd
.definelabel CombinePatch_OfflinePayloadStart, 0x0028D1B0
.definelabel CombinePatch_OfflinePayloadEnd,   OfflinePatch_VersionStorageStart
.definelabel CombinePatch_ModeStorage,         0x00329FFC
.definelabel CombinePatch_ModeOffline,         0
.definelabel CombinePatch_ModeOriginal,        1
.definelabel CombinePatch_SessionModeOffset,   0
.definelabel CombinePatch_SelectedModeOffset,  1
.definelabel CombinePatch_TitleRPreviousOffset,2
.definelabel CombinePatch_TitleOfflineMessage, 67
.definelabel CombinePatch_TitleOriginalMessage,68
.definelabel CombinePatch_InitialConnectMessage,69
.definelabel CombinePatch_BankConnectMessage,  70
.definelabel CombinePatch_SaveMessage,         71
.definelabel CombinePatch_DisconnectMessage,   72

.open "../rom/exefs/00040000000C9C00.dec.code", "../build/00040000000C9C00.dec.code", 0x00100000

// Title mode selection. The displayed R uses the same private-use button glyph
// as the Bank patch and is independent of the native input bit.
// 标题模式选择。画面中的 R 使用与 Bank 补丁相同的专用区按键字形，并与原生
// 输入位相互独立。
.org MoverTitleUi_HomeMessageCall
    bl CombinePatch_TitleTextInitialize
.org MoverTitleState_Update
    b CombinePatch_TitleStateUpdate

// Every offline hook is routed through a mode-aware wrapper. Original mode
// replays the exact overwritten instruction or enters the untouched function.
// 所有离线钩子均经由模式包装函数分派。原版模式会精确重放被覆盖指令，或进入
// 未修改函数的后续位置。
.org MoverNetworkState_Update
    b CombinePatch_NetworkUpdate
.org MoverNetwork_AvailabilityCall
    bl CombinePatch_NetworkAvailability
.org MoverNetwork_SkipRemoteJob
    b CombinePatch_NetworkSkipRemoteJob
.org MoverNetworkState_Initialize + 0x68
    bl CombinePatch_SelectInitialConnectMessage
.org MoverTicketState_Update
    b CombinePatch_TicketUpdate
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
.org MoverDisconnectState_Update
    b CombinePatch_DisconnectUpdate
.org MoverDisconnectState_Initialize + 0x54
    bl CombinePatch_SelectDisconnectMessage
.org MoverDisconnect_SkipRemoteJob
    b CombinePatch_DisconnectSkipRemoteJob
.org MoverFlow_RemoteCheckSuccessState
    beq CombinePatch_RemoteCheckSuccessState

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
    ldr r12,=CombinePatch_ModeStorage
    mov r3,#CombinePatch_ModeOffline
    strb r3,[r12,#CombinePatch_SessionModeOffset]
    strb r3,[r12,#CombinePatch_SelectedModeOffset]
    strb r3,[r12,#CombinePatch_TitleRPreviousOffset]
    mov r3,#CombinePatch_TitleOfflineMessage
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
    cmp r3,#CombinePatch_ModeOriginal
    moveq r3,#CombinePatch_TitleOriginalMessage
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
    beq OfflinePatch_NetworkUpdate
    push {r4-r6,lr}
    b MoverNetworkState_Update + 4

CombinePatch_TicketUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_TicketUpdate
    push {r3-r9,lr}
    b MoverTicketState_Update + 4

CombinePatch_RemoteCheckUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_RemoteCheckUpdate
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
    beq OfflinePatch_NoTransferUpdate
    push {r4-r6,lr}
    b MoverNoTransferState_Update + 4

CombinePatch_DisconnectUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_DisconnectUpdate
    push {r4,lr}
    b MoverDisconnectState_Update + 4

CombinePatch_Gen5Validation:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq MoverGetPokemon_Gen5LocalContinuation
    ldr r0,=0x003293A8
    b MoverGetPokemon_SkipGen5RemoteValidation + 4

CombinePatch_Gen12Validation:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq MoverGetPokemon_Gen12LocalContinuation
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
    beq OfflinePatch_Stage
    b 0x0023E9EC

CombinePatch_SaveWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@original
    mov r0,r4
    bl OfflinePatch_SaveDisplayDelayUpdate
    b MoverSave_UpdateEpilogue
@@original:
    ldrb r1,[r4,#0x44]
    b MoverSave_StageWaitState + 4

CombinePatch_Commit:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_Commit
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
    beq OfflinePatch_Rollback
    b 0x0019B7D0

CombinePatch_AfterRollback:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#0x11
    movne r0,#0x0D
    b 0x0024A4C0

// Select appended offline lines without replacing any stock message. Original
// mode keeps the unmodified message IDs and therefore the original resources.
// 选择追加的离线文本，不替换任何原版消息。原版模式保持原消息编号，因此继续
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

// Place the shared filesystem module in the audited code cave. Keeping this
// module out-of-line lets the Bankdata load and transaction modules reuse one
// implementation without duplicating their low-level IPC helpers.
// 将共用文件系统模块放入已审计的代码空位。该模块保持独立后，Bankdata
// 载入与事务模块可共用同一套底层 IPC 辅助实现，无需重复。
FsHelpers_PayloadBegin:
    .importobj "../build/fs_helpers.o"
FsHelpers_PayloadEnd:

CombinePatch_CodeUsedEnd:
.endarea

.org CombinePatch_OfflinePayloadStart
.area CombinePatch_OfflinePayloadEnd-CombinePatch_OfflinePayloadStart
OfflinePatch_EligibilityEntry:
    ldr r1,[r0,#0x10]
    cmp r1,#7
    bhs @@resumeNative
    b OfflinePatch_EligibilityUpdate
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

// Keep the remaining feature modules consecutive before 0x0028DD00. The
// immediately following dedicated area is reserved for the external
// Transporter Redirect Patch and must remain identical to the stock image.
// 其余功能模块在 0x0028DD00 之前连续排列。紧随其后的独立区域
// 专门保留给外部 Transporter Redirect Patch，必须与原版镜像保持一致。
BankdataRedirect_PayloadBegin:
    .importobj "../build/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:

LocalTicket_PayloadBegin:
    .importobj "../build/local_ticket.o"
LocalTicket_PayloadEnd:

OfflineFlow_PayloadBegin:
    .importobj "../build/offline_flow.o"
OfflineFlow_PayloadEnd:

PatchPaths_PayloadBegin:
    .importobj "../build/patch_paths.o"
PatchPaths_PayloadEnd:
CombinePatch_OfflinePayloadUsedEnd:
.endarea

// Protect only the external patch's aligned 0xE0-byte payload allocation.
// This area intentionally emits no bytes. 0x0028DDE0..0x0028DFC0 remains
// available for future payloads maintained by this project.
// 仅保护外部补丁对齐后的 0xE0 字节载荷区。该区域刻意不输出任何
// 字节；0x0028DDE0..0x0028DFC0 仍可供本项目以后的载荷使用。
.org 0x0028DD00
.area 0xE0
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
