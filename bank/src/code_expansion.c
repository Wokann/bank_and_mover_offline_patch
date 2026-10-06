#include "code_expansion.h"

s32 CodeExpansion_Enable(u32 address,u32 size)
{
    register u32 r0 __asm__("r0")=0;
    register u32 r1 __asm__("r1")=CODE_EXPANSION_PROCESS_HANDLE;
    u32 process;
    u32 isAzahar;
    s32 result;

    /* SVC 0x70 needs a real handle, not the current-process pseudo-handle. */
    /* SVC 0x70 需要真实句柄，不能直接使用当前进程的伪句柄。 */
    __asm__ volatile("svc 0x27":"+r"(r0),"+r"(r1)::
        "r2","r3","r12","memory","cc");
    if ((s32)r0) return (s32)r0;
    process=r1;

    {
        register u32 r2 __asm__("r2")=0;
        r0=0;
        r1=CODE_EXPANSION_EMULATOR_INFO;
        __asm__ volatile("svc 0x2A":"+r"(r0),"+r"(r1),"+r"(r2)::
            "r3","r12","memory","cc");
        isAzahar=r0==0 && r1==CODE_EXPANSION_AZAHAR_ID && r2==0;
    }

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
    if (isAzahar && (u32)result==process) result=0;

    r0=process;
    __asm__ volatile("svc 0x23":"+r"(r0)::
        "r1","r2","r3","r12","memory","cc");
    return result?result:(s32)r0;
}
