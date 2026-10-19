.3ds
.arm
.relativeinclude on

.include "../include/symbol.inc"

// 0: require native online tickets; 1: use the server API only for a confirmed
// missing Azahar ticket interface, with a read-only fixed free window.
// 0：联网必须走真实票务；1：仅确认 Azahar 缺票务接口时自动使用伪服务器。
// 联网回退固定长期免费且不提供模拟购买；离线票务才读取本地配置。
.definelabel ONLINE_FAKE_SERVER_TICKET_FALLBACK, 1

.definelabel OfflinePatch_VersionStorageSize,   0x40
.definelabel OfflinePatch_VersionStorageStart,  TextMappedEnd - OfflinePatch_VersionStorageSize
.definelabel CombinePatch_CodeStart, CodeExpansion_PayloadStart
.definelabel FakeServer_ContextSize, 0x104000
.definelabel FakeServer_RuntimeSize, 0x296000
.definelabel CombinePatch_CodeEnd, CodeExpansion_PayloadStart + CodeExpansion_PayloadSize - FakeServer_RuntimeSize
.definelabel CodeExpansion_ExecutableSize, CombinePatch_CodeEnd - CombinePatch_CodeStart
.definelabel CombinePatch_OfflinePayloadEnd, CombinePatch_CodeEnd
.definelabel CombinePatch_ModeStorage,         OfflinePatch_RuntimeStorageStart + 4
.definelabel RuntimeEnvironment_StorageStart,  OfflinePatch_RuntimeStorageStart - 0x10
.definelabel RuntimeEnvironment_StorageEnd,    OfflinePatch_RuntimeStorageStart
.definelabel CombinePatch_ModeOffline,         0
.definelabel CombinePatch_ModeOnline,          1
.definelabel CombinePatch_SessionModeOffset,   0
.definelabel CombinePatch_SelectedModeOffset,  1
.definelabel CombinePatch_TitleRPreviousOffset,2
.definelabel CombinePatch_TitleOfflineMessage, 67
.definelabel CombinePatch_TitleOnlineMessage,  68
.definelabel CombinePatch_TitleModePane,       43
.definelabel CombinePatch_InitialConnectMessage,69
.definelabel CombinePatch_BankConnectMessage,  70
.definelabel CombinePatch_SaveMessage,         71
.definelabel CombinePatch_DisconnectMessage,   72
.definelabel CombinePatch_MissingBankMessage,  74

.open INPUT_CODE, OUTPUT_CODE, 0x00100000

// Optional, offline-only name restoration around intact native converters.
// 可选离线姓名适配；原转换体不改写，仅在返回后恢复所选汉化名字。
.org 0x00241574
    b MoverNames_ConvertGen5
.org 0x0024E48C
    b MoverNames_ConvertVcA
.org 0x0024F428
    b MoverNames_ConvertVcB
.org 0x002508B4
    b MoverNames_ConvertVcC
.org 0x00245F14
    bl MoverNames_ConvertGen5CheckedEntry

// VC-only construction and gender correction callsites. Default/online paths
// call the original functions; no Gen5 or other conversion code is replaced.
// 仅包装 VC 构造及性别覆盖调用；默认／在线原样透传，Gen5 不受影响。
.org 0x0024F6B0
    bl MoverLegacy_CreateVcB
.org 0x0024F8F4
    bl MoverLegacy_SetGender
.org 0x0024F90C
    bl MoverLegacy_SetGender
.org 0x0024F470
    bl MoverLegacy_Decode
.org 0x0024F968
    bl MoverLegacy_Decode
.org 0x0024FAE4
    bl MoverLegacy_Decode

// Only the four v1.5 conversion paths read per-user locale; stock startup,
// UI language selection and every online call retain their original CFG path.
// 仅四条 v1.5 转换路径读取用户地区，启动、界面语言和联网仍走原 CFG。
.org 0x00241B0C
    bl MoverConsole_Location
.org 0x00241B3C
    bl MoverConsole_Region
.org 0x0024E9A8
    bl MoverConsole_Location
.org 0x0024E9D8
    bl MoverConsole_Region
.org 0x0024F794
    bl MoverConsole_Location
.org 0x0024F7C4
    bl MoverConsole_Region
.org 0x00250DC4
    bl MoverConsole_Location
.org 0x00250DF4
    bl MoverConsole_Region

.org MoverStartup_ConstructorCall
    bl CodeExpansion_Startup
.org 0x0023F874
    b CombinePatch_RemoteUnbind

// Bank's native language is authoritative in both modes. Read it before stock
// startup, then save native Bank -> SD during the animated title return.
// 两个模式均以 Bank 原档语言为准；启动前只读，确认后在转圈期间先保存 Bank 原档再同步 SD。
.org MoverStartup_InitializeCall
    b CombinePatch_StartupLanguageGate
.org MoverStartup_Destruct
    b CombinePatch_StartupLanguageDestroy
.org MoverLanguageSelection_SettingsCall
    bl CombinePatch_SaveBankLanguage
.org MoverDisconnectState_Exit
    b CombinePatch_LanguageReturnExit
.org MoverDisconnectState_Exit + 0xC
    bl CombinePatch_DisconnectMinimumWait
.org MoverDisconnectState_Destroy
    b CombinePatch_LanguageReturnDestroy

// Protect every native NDS save, including receipt and token-cleanup saves.
// 保护每次原版 NDS 保存，包括事务凭据写入和令牌清理保存。
.org StateLocalSave_Begin
    b CombinePatch_SourceSaveBegin
.org StateLocalSave_Poll
    b CombinePatch_SourceSavePoll

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
.org MoverNetwork_AvailabilityCall
    bl CombinePatch_NetworkAvailability
.org MoverNetworkState_Initialize + 0x54
    blne CombinePatch_ConnectionConstruct
.org MoverDisconnectState_Initialize + 0x70
    blne CombinePatch_ConnectionConstruct
.org 0x0019A068
    b CombinePatch_ConnectionPoll
.org 0x00240B24
    b CombinePatch_ConnectionUnbind
.org NetworkMonitor_Update
    b CombinePatch_NetworkMonitorUpdate
.org NetworkMonitor_WorkerAccountLoad
    bl CombinePatch_NetworkMonitorWorkerAccount
.org MoverNetworkState_Initialize + 0x68
    bl CombinePatch_SelectInitialConnectMessage
.org MoverTicketState_TicketInitializeCall
    bl CombinePatch_TicketInitialize
.org TicketJob_Poll
    b CombinePatch_TicketJobPoll
.org 0x0023E384
    bl CombinePatch_TicketExpiryRequest
.org 0x0023E4B0
    bl CombinePatch_TicketExpiryRequest
.org 0x00249600
    b CombinePatch_TicketStatePoll
.org 0x002403B8
    b CombinePatch_ValidationUnbind
.org MoverTicketState_UiPrepare
    b CombinePatch_TicketUiPrepare
.org MoverTicketState_FadeBeginCall
    bl CombinePatch_TicketUiFade
.org MoverTicketState_JobUiPrepare
    b CombinePatch_TicketJobUiPrepare
.org 0x0023D418
    bl CombinePatch_TicketProviderBind
.org 0x0023E66C
    bl CombinePatch_TicketProviderUnbind
.org 0x0015B89C
    bl CombinePatch_TicketSend
.org 0x0015BB84
    bl CombinePatch_TicketSend
.org 0x0015B684
    bl CombinePatch_TicketSend
.org 0x0015BA08
    bl CombinePatch_TicketSend
.org 0x0015B794
    bl CombinePatch_TicketSend
.org MoverTicketState_CampaignRequestCall
    bl CombinePatch_TicketCampaignRequest
.org MoverRemoteCheckState_Update
    b CombinePatch_RemoteCheckUpdate
.org MoverRemote_QueryProvider
    b CombinePatch_QueryProvider
.org 0x0023F788
    b CombinePatch_DownloadProvider
.org 0x0023F944
    b CombinePatch_DownloadResponse
.org 0x0023EEC8
    b CombinePatch_MetadataUpdateProvider
.org MoverSourceRecoveryState_Update
    b CombinePatch_SourceRecoveryUpdate
.org MoverSourceRecovery_BankMessageIdLoad
    bl CombinePatch_SelectBankConnectMessage
.org MoverTransactionRecoveryState_Update
    b CombinePatch_TransactionRecoveryUpdate
.org MoverTransferEligibilityState_Update
    b CombinePatch_EligibilityUpdate
.org MoverTransferCommitState_Update
    b CombinePatch_SaveStateUpdate
.org MoverGetPokemonState_Update
    b CombinePatch_GetPokemonUpdate
.org MoverGetPokemon_BankMessageIdLoad
    bl CombinePatch_SelectBankConnectMessage
.org MoverValidation_HttpLaunch
    bl CombinePatch_ValidationHttpLaunch
.org MoverNoTransferState_Update
    b CombinePatch_NoTransferUpdate
.org 0x0019BB34
    b CombinePatch_RemoteBind
.org 0x0023EBE0
    b CombinePatch_MetadataProvider
.org 0x0023EAAC
    b CombinePatch_StageProvider
.org 0x0019B998
    b CombinePatch_CommitProvider
.org 0x0019B84C
    b CombinePatch_RollbackProvider
.org MoverNoTransfer_BankMessageIdLoad
    bl CombinePatch_SelectBankConnectMessage
.org MoverDisconnectState_Initialize + 0x54
    bl CombinePatch_SelectDisconnectMessage
.org MoverDisconnectState_Initialize + 0x90
    bl CombinePatch_SelectClosingMessage
.org MoverDisconnectState_Update
    b CombinePatch_StorageNoticeDisconnectUpdate

// Shared source discovery keeps the stock list and save-validation state, but
// supplies SD-backed Gen 5 candidates at the three native card I/O edges in
// both session modes. Physical-card winners continue through exact prologue
// trampolines into the untouched implementations.
// 公共来源发现保留原版列表与存档校验状态，并在两种会话模式中通过三个原生卡带 I/O
// 边界提供 SD 上的第五世代候选。实体卡带胜出项经精确序言跳板进入未修改实现。
.org MoverSourceList_UpdateVtableSlot
    .word NdsSources_ListUpdate + 1
.org MoverSourceSelection_Update
    b CombinePatch_NdsSelectionUpdateEntry
.org MoverSourceSelection_ConfirmFadeCall
    bl CombinePatch_SelectionConfirmFade
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
    ldr r12,=BankdataRedirect_TitleInitialize + 1
    bx r12

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
    bne @@check
    mov r0,r4
    bl CombinePatch_TitleProcessModeToggle
@@check:
    mov r0,r4
    ldr r1,=CombinePatch_ModeStorage
    ldrb r1,[r1,#CombinePatch_SelectedModeOffset]
    bl TitleUsers_Update
    cmp r0,#0
    bne @@pending
    ldrb r1,[r4,#0x3C]
    cmp r1,#0
    ldreqb r0,[r4,#0x3D]
    cmpeq r0,#0
    bne @@latch
    b @@pending
@@pending:
    mov r0,#0
    pop {r4,pc}
@@latch:
    ldr r1,=CombinePatch_ModeStorage
    ldrb r2,[r1,#CombinePatch_SelectedModeOffset]
    strb r2,[r1,#CombinePatch_SessionModeOffset]
    mov r0,#1
    pop {r4,pc}
    .pool

// Toggle on an R rising edge and rebind only the independent mode pane.
// The key mask is the native 3DS R bit (0x100); the displayed R uses
// the localized font's private-use button glyph.
// 仅在 R 键上升沿切换，并只更新独立模式窗格，不再占用 HOME。按键掩码
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
    mov r2,#CombinePatch_TitleModePane
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

CombinePatch_ConnectionConstruct:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne 0x001E3114
    ldr r2,=0x001E7528
    ldr r12,=NativeResponse_CreateConnection + 1
    bx r12
    .pool

CombinePatch_ConnectionPoll:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=NativeResponse_PollConnection + 1
    bxeq r12
    push {r4-r6,lr}
    b 0x0019A06C
    .pool

CombinePatch_ConnectionUnbind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bxeq lr
    push {r4,lr}
    b 0x00240B28
    .pool

// Only real-network modes monitor AC/NEX sessions. Local SDK jobs, and the
// native worker's stop/lock/sleep/exit lifecycle, remain active in Offline.
// 仅真实联网模式监控 AC/NEX 会话；离线保留本地 SDK 作业及原线程停止／锁／休眠／退出。
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

CombinePatch_RemoteCheckUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_RemoteCheckUpdate + 1
    bxeq r12
BankdataRedirect_OriginalRemoteCheckUpdate:
    push {r4-r8,lr}
    b MoverRemoteCheckState_Update + 4

CombinePatch_QueryProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    // Accept only the job constructed by the native request. The next update
    // supplies its service reply through the original SDK completion routine.
    // 仅接受原请求已构造的作业；下一次更新通过原 SDK 完成接口交付服务响应。
    push {r4,lr}
    mov r0,r4
    mov r1,r5
    mov r2,#4
    ldr r12,=NativeResponse_AcceptOperation + 1
    blx r12
    pop {r4,lr}
    b MoverRemote_QueryProviderResult
@@native:
    ldr r0,[r4,#0x10]
    b MoverRemote_QueryProvider + 4
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
    ldr r12,=NativeResponse_AcceptOperation + 1
    blx r12
    pop {r4,lr}
    b 0x0023F7A0
@@native:
    ldr r0,[r4,#0x10]
    b 0x0023F78C
    .pool

CombinePatch_DownloadResponse:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne BankdataRedirect_OriginalDownloadResponse
    ldr r2,=BankdataRedirect_OriginalDownloadResponse
    ldr r12,=NativeResponse_DownloadResponse + 1
    bx r12
BankdataRedirect_OriginalDownloadResponse:
    push {r4-r6,lr}
    b 0x0023F948
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
    ldr r12,=NativeResponse_AcceptOperation + 1
    blx r12
    pop {r4,lr}
    b 0x0023EEE0
@@native:
    ldr r0,[r4,#0x10]
    b 0x0023EECC
    .pool

CombinePatch_EligibilityUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_EligibilityUpdate + 1
    bxeq r12
BankdataRedirect_OriginalEligibilityUpdate:
    push {r4-r6,lr}
    b MoverTransferEligibilityState_Update + 4

CombinePatch_GetPokemonUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    beq OfflinePatch_GetPokemonEntry
    push {r4-r11,lr}
    b MoverGetPokemonState_Update + 4

CombinePatch_ValidationUnbind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=ServerValidation_UnbindClient + 1
    bxeq r12
ServerValidation_OriginalUnbindClient:
    push {r4-r6,lr}
    b 0x002403BC

CombinePatch_NoTransferUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_NoSaveUpdate + 1
    bxeq r12
BankdataRedirect_OriginalNoSaveUpdate:
    push {r4-r6,lr}
    b MoverNoTransferState_Update + 4

CombinePatch_RemoteBind:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_BindRemote + 1
    bxeq r12
BankdataRedirect_OriginalBindRemote:
    push {r4-r6,lr}
    b 0x0019BB38
    .pool

CombinePatch_RemoteUnbind:
    push {r0-r3,r12,lr}
    ldr r12,=NativeResponse_CancelRemote + 1
    blx r12
    pop {r0-r3,r12,lr}
    push {r4-r6,lr}
    b 0x0023F878
    .pool

CombinePatch_MetadataProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r6
    mov r2,r5
    ldr r12,=NativeResponse_AcceptMetadata + 1
    blx r12
    pop {r4,lr}
    b 0x0023EBFC
@@native:
    ldr r0,[r4,#0x10]
    b 0x0023EBE4
    .pool

CombinePatch_ValidationHttpLaunch:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=ServerValidation_Start + 1
    bxeq r12
    b 0x0018A1A0

CombinePatch_SourceRecoveryUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_SourceRecoveryUpdate + 1
    bxeq r12
BankdataRedirect_OriginalSourceRecoveryUpdate:
    push {r4-r6,lr}
    b MoverSourceRecoveryState_Update + 4

CombinePatch_TransactionRecoveryUpdate:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    ldreq r12,=BankdataRedirect_TransactionRecoveryUpdate + 1
    bxeq r12
BankdataRedirect_OriginalTransactionRecoveryUpdate:
    push {r4-r7,lr}
    b MoverTransactionRecoveryState_Update + 4
    .pool

CombinePatch_SaveStateUpdate:
    ldr r12,=BankdataRedirect_SaveUpdate + 1
    bx r12
BankdataRedirect_OriginalSaveUpdate:
    push {r4-r6,lr}
    b MoverTransferCommitState_Update + 4

CombinePatch_StageProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r4
    mov r1,r6
    mov r2,#8
    ldr r12,=NativeResponse_AcceptOperation + 1
    blx r12
    pop {r4,lr}
    b 0x0023EAC4
@@native:
    ldr r0,[r4,#0x10]
    b 0x0023EAB0
    .pool

CombinePatch_CommitProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r6
    mov r1,r4
    mov r2,#1
    mov r3,r7
    ldr r12,=BankdataRedirect_TransactionProvider + 1
    blx r12
    pop {r4,lr}
    b 0x0019B9B4
@@native:
    ldr r0,[r4,#0x10]
    b 0x0019B99C
    .pool

CombinePatch_RollbackProvider:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne @@native
    push {r4,lr}
    mov r0,r6
    mov r1,r4
    mov r2,#0
    mov r3,r7
    ldr r12,=BankdataRedirect_TransactionProvider + 1
    blx r12
    pop {r4,lr}
    b 0x0019B868
@@native:
    ldr r0,[r4,#0x10]
    b 0x0019B850
    .pool

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

CombinePatch_StorageNoticeDisconnectUpdate:
    push {r0,lr}
    bl MoverStorageNotice_Poll
    cmp r0,#0
    pop {r0,lr}
    moveq r0,#0
    bxeq lr
    push {r4,lr}
    b MoverDisconnectState_Update + 4

CombinePatch_SelectDisconnectMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    movne r1,#14
    bxne lr
    push {r0,r2,r3,lr}
    ldr r12,=NativeResponse_TakeMissingBankNotice + 1
    blx r12
    cmp r0,#0
    movne r1,#CombinePatch_MissingBankMessage
    moveq r1,#CombinePatch_DisconnectMessage
    pop {r0,r2,r3,pc}

// A failed connection may already be closed before the native exit pane.
// 失败后连接可能已关闭；原退出窗格同样可显示缺银行说明，其他退出仍用原文。
CombinePatch_SelectClosingMessage:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    movne r1,#33
    bxne lr
    push {r0,r2,r3,lr}
    ldr r12,=NativeResponse_TakeMissingBankNotice + 1
    blx r12
    cmp r0,#0
    movne r1,#CombinePatch_MissingBankMessage
    moveq r1,#33
    pop {r0,r2,r3,pc}
    .pool

// Reuse the native elapsed-time gate without blocking animation. Only the
// missing-Bank notice extends its minimum; Online keeps the original argument.
// 沿用原版逐帧计时，不阻塞动画；仅缺银行说明延长最低时长，在线保留原参数。
CombinePatch_DisconnectMinimumWait:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12]
    cmp r12,#CombinePatch_ModeOffline
    bne 0x0019B758
    push {r0,r2,r3,lr}
    ldr r12,=NativeResponse_DisconnectMinimumDelay + 1
    blx r12
    mov r1,r0
    pop {r0,r2,r3,lr}
    b 0x0019B758
    .pool

// Select once at job initialization; poll, campaign and cleanup use the stored
// backend. Tail calls preserve the original BL return address and aligned stack.
// 作业初始化时仅选择一次；轮询、活动和清理沿用保存的后端。
// 尾调用保留原 BL 返回地址与栈对齐。
// Offline retains the connection pane at both applet-only preparation sites;
// allocation, ticket results, callback and destruction still run natively.
// 离线保留两处小程序专用准备位置的连接提示；分配、票务结果、回调、析构仍走原版。
CombinePatch_TicketUiPrepare:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq MoverTicketState_UiAdvance
    ldr r5,[r4,#0x38]
    b MoverTicketState_UiPrepare + 4
CombinePatch_TicketUiFade:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    moveq r0,#1
    bxeq lr
    b Ui_SetAppletFade
CombinePatch_TicketJobUiPrepare:
    ldr r12,=CombinePatch_ModeStorage
    ldrb r12,[r12,#CombinePatch_SessionModeOffset]
    cmp r12,#CombinePatch_ModeOffline
    beq MoverTicketState_JobInitialize
    ldr r5,[r4,#0x38]
    b MoverTicketState_JobUiPrepare + 4
CombinePatch_TicketInitialize:
    ldr r1,[r4,#0x28]
    ldr r12,=CombinePatch_ModeStorage
    ldrb r2,[r12,#CombinePatch_SessionModeOffset]
    ldr r12,=CombinePatch_OnlineTicketFallback
    ldrb r3,[r12]
    b ServerTicket_InitializeSelected
CombinePatch_TicketSend:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_Send
    b 0x001D17E0
CombinePatch_TicketJobPoll:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_Poll
ServerTicket_OriginalPoll:
    push {r4,r5,r6,lr}
    b TicketJob_Poll + 4
CombinePatch_TicketExpiryRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    movne r3,#0
    b 0x0019D8C8
CombinePatch_TicketStatePoll:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne ServerTicket_UpdateState
ServerTicket_OriginalState:
    push {r3,r4,r5,r6,r7,r8,r9,lr}
    b 0x00249604
CombinePatch_TicketCampaignRequest:
    ldr r12,=ServerTicket_BackendStorage
    ldrb r12,[r12]
    cmp r12,#0
    bne CombinePatch_TicketCampaignRequestLocal
    b MoverCampaign_Request
CombinePatch_TicketCampaignRequestLocal:
    mov r0,r4
    b ServerTicket_CampaignResult
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
NdsSources_OriginalSaveBegin:
    push {r4-r6,lr}
    b StateLocalSave_Begin + 4
NdsSources_OriginalSavePoll:
    push {r3-r5,lr}
    b StateLocalSave_Poll + 4
NdsSources_OriginalSelectionUpdate:
    push {r4-r8,lr}
    b MoverSourceSelection_Update + 4
CombinePatch_NdsSelectionUpdateEntry:
    ldr r12,=NdsSources_SelectionUpdate + 1
    bx r12
// Always advance the native UI fade, then wait only at its completion boundary.
// 原淡出始终运行；仅完成边界等待来源摘要。r4 是原选择状态，原 ABI 保持不变。
CombinePatch_SelectionConfirmFade:
    push {r4,lr}
    bl MoverSourceSelection_PollFade
    cmp r0,#0
    popeq {r4,pc}
    mov r0,r4
    ldr r12,=NdsSources_SelectionReady + 1
    blx r12
    pop {r4,pc}
CombinePatch_SourceSaveBegin:
    ldr r12,=NdsSources_SaveBegin + 1
    bx r12
CombinePatch_SourceSavePoll:
    ldr r12,=NdsSources_SavePoll + 1
    bx r12

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
    .importobj BUILD_DIRECTORY + "/common/fs_system/fs_helpers.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/diagnostic_log.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/storage_notice.o"
    .importobj BUILD_DIRECTORY + "/storage_notice.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/memory.o"
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
    ldr r1,=CodeExpansion_ExecutableSize
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
    .importobj BUILD_DIRECTORY + "/common/fs_system/runtime_environment.o"
CodeExpansion_LoaderEnd:
.endarea

.org CombinePatch_CodeUsedEnd
CombinePatch_OfflinePayloadStart:
.area CombinePatch_OfflinePayloadEnd-CombinePatch_OfflinePayloadStart
OfflinePatch_GetPokemonEntry:
    // Disable the extra two-second display delay while evaluating local-server latency.
    // 测试本地服务器耗时时，停用额外的两秒显示延时；原版状态机计时不改。
    // push {r0,lr}
    // mov r1,#125
    // lsl r1,r1,#4
    // bl StateTimer_HasElapsed
    // cmp r0,#0
    // pop {r0,lr}
    // moveq r0,#0
    // bxeq lr
    push {r0,lr}
    ldr r12,=ServerValidation_Poll + 1
    blx r12
    pop {r0,lr}
    push {r4-r11,lr}
    b MoverGetPokemonState_Update + 4
    .pool

// Keep all remaining feature modules consecutive inside the added pages.
// 其余功能模块在新增页内连续排列。
CombinePatch_StartupLanguageGate:
    push {r0,lr}
    ldr r1,=MoverStartup_GameDataOwner
    ldr r1,[r1]
    ldr r1,[r1,#0x1C]
    ldr r12,=LanguageSettings_PrepareStartup + 1
    blx r12
    cmp r0,#0
    pop {r0,lr}
    beq MoverStartup_UpdateWaiting
    bl MoverStartup_Initialize
    b MoverStartup_InitializeCall + 4
CombinePatch_StartupLanguageDestroy:
    push {r0,lr}
    ldr r12,=LanguageSettings_CancelStartup + 1
    blx r12
    pop {r0,lr}
    ldr r1,=0x002E6438
    b MoverStartup_Destruct + 4
CombinePatch_SaveBankLanguage:
    ldr r12,=LanguageSettings_StoreSelection + 1
    bx r12
// Native language update/destruction remain untouched. Only title-return Exit
// waits for SD, after Init has started the spinner. Never call SDK teardown
// while holding the service mutex; every busy/queued/release wait yields.
// 原语言更新／析构保持原样；仅在返回标题 Exit 等待 SD，此前 Init 已启动转圈。
// 不持服务锁执行 SDK 清理；锁忙、排队与释放等待均逐帧返回。
CombinePatch_LanguageReturnExit:
    push {r4,lr}
    mov r4,r0
    ldr r12,=LanguageSettings_PollSynchronization + 1
    blx r12
    cmp r0,#0
    beq CombinePatch_LanguageReturnWait
    mov r0,r4
    bl MoverStorageNotice_Poll
    cmp r0,#0
CombinePatch_LanguageReturnWait:
    mov r0,r4
    pop {r4,lr}
    moveq r0,#0
    bxeq lr
    push {r4-r8,lr}
    b MoverDisconnectState_Exit + 4
CombinePatch_LanguageReturnDestroy:
    push {r0,lr}
    ldr r12,=LanguageSettings_CancelSynchronization + 1
    blx r12
    ldr r12,=NativeResponse_ClearMissingBankNotice + 1
    blx r12
    pop {r0,lr}
    ldr r1,=0x002E679C
    b MoverDisconnectState_Destroy + 4
    .pool

ServerTicket_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/server_ticket.o"
ServerTicket_PayloadEnd:
SystemTime_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/system_time.o"
SystemTime_PayloadEnd:
ConsoleSettings_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/console_settings.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/console_identity.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/utf_path.o"
    .importobj BUILD_DIRECTORY + "/common/fs_system/title_users.o"
ConsoleSettings_PayloadEnd:

BankdataRedirect_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/bankdata_redirect.o"
BankdataRedirect_PayloadEnd:


PatchPaths_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/patch_paths.o"
PatchPaths_PayloadEnd:

ServerValidation_PayloadBegin:
    .align 4
// The original accepted-slot loop owns r5=state, r8=slot. Pass its exact
// server decision without global per-slot state or changes to the PK5 input.
// 原接纳循环持有 r5=状态、r8=槽号；只传本槽判定，不加全局状态、不改来源记录。
MoverNames_ConvertGen5CheckedEntry:
    ldr r2,[r5,#0x28]
    add r2,r2,#0x48
    ldr r2,[r2,r8,lsl #2]
    b MoverNames_ConvertGen5Checked
MoverLegacy_CreateVcB:
    push {r4,lr}
    ldrb r12,[r4,#0x1C]
    ldrb r4,[r4,#0x1B]
    orr r4,r4,r12,lsl #8
    sub sp,sp,#8
    str r4,[sp]
    bl MoverLegacy_Create
    add sp,sp,#8
    pop {r4,pc}
.definelabel MoverLegacy_OriginalCreate,0x0019CD3C
.definelabel MoverLegacy_OriginalSetGender,0x00198CD0
.definelabel MoverLegacy_OriginalDecode,0x00198D60
.definelabel MoverLegacy_AssignText,0x0019C93C
.definelabel MoverLegacy_NormalizeKana,0x0024DC88
MoverNames_OriginalGen5:
    push {r0,r1,r4-r11,lr}
    b 0x00241578
MoverNames_OriginalVcA:
    push {r0-r11,lr}
    b 0x0024E490
MoverNames_OriginalVcB:
    push {r0-r11,lr}
    b 0x0024F42C
MoverNames_OriginalVcC:
    push {r0-r11,lr}
    b 0x002508B8
    .importobj BUILD_DIRECTORY + "/server_validation.o"
    .importobj BUILD_DIRECTORY + "/console_profile.o"
    .importobj BUILD_DIRECTORY + "/chinese_names.o"
    .importobj BUILD_DIRECTORY + "/legacy_vc.o"
ServerValidation_PayloadEnd:
NdsFs_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/common/fs_system/fs_utf16.o"
NdsFs_PayloadEnd:
LanguageSettings_PayloadBegin:
    .importobj BUILD_DIRECTORY + "/language_settings.o"
    .importobj BUILD_DIRECTORY + "/bank_native_settings.o"
LanguageSettings_PayloadEnd:
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
    .importobj BUILD_DIRECTORY + "/common/fs_system/source_image.o"
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
CombinePatch_OfflinePayloadUsedEnd:
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
LocalSource_ContextStart:
.area 0x1000, 0
.endarea
LocalSource_ImageStart:
.area 0x80000, 0
.endarea
LocalSource_ImageEnd:
FakeServer_RuntimeEnd:

.close
