#include "runtime_environment.h"

static int detectAzahar(void)
{
    register u32 r0 __asm__("r0")=0;
    register u32 r1 __asm__("r1")=CODE_EXPANSION_EMULATOR_INFO;
    register u32 r2 __asm__("r2")=0;
    __asm__ volatile("svc 0x2A":"+r"(r0),"+r"(r1),"+r"(r2)::
        "r3","r12","memory","cc");
    return r0==0 && r1==CODE_EXPANSION_AZAHAR_ID && r2==0;
}

s32 CodeExpansion_Enable(u32 address,u32 size)
{
    volatile RuntimeEnvironmentView *environment=RuntimeEnvironment_Storage;
    register u32 r0 __asm__("r0")=0;
    register u32 r1 __asm__("r1")=CODE_EXPANSION_PROCESS_HANDLE;
    u32 process;
    s32 result;

    environment->environment=detectAzahar()?RUNTIME_ENVIRONMENT_AZAHAR:
        RUNTIME_ENVIRONMENT_NATIVE_OR_UNKNOWN;
    environment->processMemory=RUNTIME_CAPABILITY_UNKNOWN;
    environment->ticket=RUNTIME_CAPABILITY_UNKNOWN;
    environment->ticketResult=0;

    /* SVC 0x70 needs a real handle, not the current-process pseudo-handle. */
    /* SVC 0x70 需要真实句柄，不能直接使用当前进程的伪句柄。 */
    r0=0;
    r1=CODE_EXPANSION_PROCESS_HANDLE;
    __asm__ volatile("svc 0x27":"+r"(r0),"+r"(r1)::
        "r2","r3","r12","memory","cc");
    if ((s32)r0) {
        environment->processMemory=RUNTIME_CAPABILITY_ERROR;
        return (s32)r0;
    }
    process=r1;

    {
        register u32 r2 __asm__("r2")=address;
        register u32 r3 __asm__("r3")=size;
        register u32 r4 __asm__("r4")=CODE_EXPANSION_PROTECT;
        register u32 r5 __asm__("r5")=CODE_EXPANSION_READ_WRITE_EXECUTE;
        r0=process;
        r1=address;
        __asm__ volatile("svc 0x70":"+r"(r0),"+r"(r1),"+r"(r2),
            "+r"(r3),"+r"(r4),"+r"(r5)::"r12","memory","cc");
        result=(s32)r0;
    }

    /* An unhandled SVC in identified Azahar leaves its input handle in r0.
       Hardware, unknown environments and every other nonzero result fail. */
    /* 已识别的 Azahar 未处理 SVC 时，r0 保留输入句柄。
       实机、未知环境及其他任何非零结果仍按失败处理。 */
    if (environment->environment==RUNTIME_ENVIRONMENT_AZAHAR && (u32)result==process) {
        environment->processMemory=RUNTIME_CAPABILITY_MISSING;
        result=0;
    } else environment->processMemory=result?RUNTIME_CAPABILITY_ERROR:
        RUNTIME_CAPABILITY_NATIVE;

    r0=process;
    __asm__ volatile("svc 0x23":"+r"(r0)::
        "r1","r2","r3","r12","memory","cc");
    return result?result:(s32)r0;
}

const volatile RuntimeEnvironmentView *RuntimeEnvironment_Get(void)
{
    return RuntimeEnvironment_Storage;
}

void RuntimeEnvironment_InitializeTicket(void)
{
    volatile RuntimeEnvironmentView *environment=RuntimeEnvironment_Storage;
    if (environment->ticket!=RUNTIME_CAPABILITY_UNKNOWN) return;
    if (environment->environment!=RUNTIME_ENVIRONMENT_AZAHAR) {
        /* Hardware/unknown environments are never probed or substituted. */
        /* 实机／未知环境不进行额外服务探测，也不允许替代真实票务。 */
        environment->ticket=RUNTIME_CAPABILITY_NATIVE;
        return;
    }
    /* Called after native services and TLS are ready, before the title pane.
       Probe a nonexistent title/ticket, not the user's actual ticket. */
    /* 原生服务与 TLS 就绪后、标题提示构建前调用；不读取用户的实际票据。 */
    static const char serviceName[8]="am:net";
    u32 handle=0,buffer=0;
    s32 result=SRV_GetServiceHandle(&handle,serviceName,
        TICKET_SERVICE_NAME_LENGTH,TICKET_SERVICE_NONBLOCKING);
    environment->ticket=RUNTIME_CAPABILITY_ERROR;
    environment->ticketResult=result;
    if (result || !handle) return;
    volatile u32 *command=fsCommandBuffer();
    command[0]=TICKET_RIGHTS_REQUEST;
    command[1]=TICKET_PROBE_BUFFER_SIZE;
    command[2]=0; command[3]=0; command[4]=0; command[5]=0;
    command[6]=(TICKET_PROBE_BUFFER_SIZE<<4)|TICKET_MAPPED_BUFFER_WRITE;
    command[7]=(u32)&buffer;
    result=fsSync(handle);
    u32 capability=RUNTIME_CAPABILITY_ERROR;
    if (!result) {
        environment->ticketResult=(s32)command[1];
        capability=command[0]==TICKET_RIGHTS_UNIMPLEMENTED_REPLY && !command[1]?
            RUNTIME_CAPABILITY_MISSING:RUNTIME_CAPABILITY_NATIVE;
    } else environment->ticketResult=result;
    result=fsCloseHandle(handle);
    if (result) environment->ticketResult=result;
    else environment->ticket=capability;
}

int RuntimeEnvironment_UseServerTicket(int offline,int allowEmulatorFallback)
{
    /* SVC support is independent of ticket support. Read the settled cache;
       an unknown/error capability is not evidence of a missing interface. */
    /* SVC 与票务能力独立；只读已缓存结果，未知／探测失败不等于缺少接口。 */
    const volatile RuntimeEnvironmentView *environment=RuntimeEnvironment_Storage;
    return offline || (allowEmulatorFallback &&
        environment->environment==RUNTIME_ENVIRONMENT_AZAHAR &&
        environment->ticket==RUNTIME_CAPABILITY_MISSING);
}
