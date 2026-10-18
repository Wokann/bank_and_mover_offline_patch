#ifndef COMMON_OFFLINE_PATCH_RUNTIME_ENVIRONMENT_H
#define COMMON_OFFLINE_PATCH_RUNTIME_ENVIRONMENT_H

#include "patch_types.h"
#include "fs_system/fs_helpers.h"

typedef struct RuntimeEnvironmentView {
    u32 environment;
    u32 processMemory;
    u32 ticket;
    s32 ticketResult;
} RuntimeEnvironmentView;

#define RuntimeEnvironment_Storage ((volatile RuntimeEnvironmentView *)RUNTIME_ENVIRONMENT_STORAGE_ADDRESS)

enum RuntimeEnvironmentKind {
    RUNTIME_ENVIRONMENT_NATIVE_OR_UNKNOWN = 1,
    RUNTIME_ENVIRONMENT_AZAHAR = 2
};
enum RuntimeCapability {
    RUNTIME_CAPABILITY_UNKNOWN = 0,
    RUNTIME_CAPABILITY_NATIVE = 1,
    RUNTIME_CAPABILITY_MISSING = 2,
    RUNTIME_CAPABILITY_ERROR = 3
};
enum TicketProbeConstant {
    TICKET_SERVICE_NAME_LENGTH = 6,
    TICKET_SERVICE_NONBLOCKING = 1,
    TICKET_RIGHTS_REQUEST = 0x08210142,
    TICKET_RIGHTS_UNIMPLEMENTED_REPLY = 0x08210040,
    TICKET_PROBE_BUFFER_SIZE = 4,
    TICKET_MAPPED_BUFFER_WRITE = 0x0C
};

enum CodeExpansionConstant {
    CODE_EXPANSION_PROCESS_HANDLE = 0xFFFF8001u,
    CODE_EXPANSION_EMULATOR_INFO = 0x20000,
    CODE_EXPANSION_AZAHAR_ID = 2,
    CODE_EXPANSION_PROTECT = 6,
    CODE_EXPANSION_READ_WRITE_EXECUTE = 7
};

/* Enable the added pages; only identified Azahar may retain an unhandled SVC. */
/* 启用新增页；仅已识别的 Azahar 可兼容未处理的 SVC。 */
s32 CodeExpansion_Enable(u32 address,u32 size);
const volatile RuntimeEnvironmentView *RuntimeEnvironment_Get(void);
void RuntimeEnvironment_InitializeTicket(void);
int RuntimeEnvironment_UseServerTicket(int offline,int allowEmulatorFallback);

#endif
