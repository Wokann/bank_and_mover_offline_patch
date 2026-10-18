#include "fakeServer/fake_server_client.h"
#include "fakeServer/console_profile.h"
#include "patch_types.h"

extern const volatile u8 CombinePatch_ModeStorage[];

/* v1.5 call-site adapters: native conversion and encryption stay untouched.
   No file access, scheduling, lock or callback is introduced in conversion. */
/* v1.5 调用点适配；原转换与加密保持不变，不在转换中读文件、等待锁或投递回调。 */
static int profile(FsConsoleProfile *out)
{ return !CombinePatch_ModeStorage[0] && FakeServerClient_ConsoleProfile(out); }

__attribute__((target("arm")))
void MoverConsole_Location(u8 *out)
{
    FsConsoleProfile value;
    int available=profile(&value);
    if(!available || value.values[FS_CONSOLE_COUNTRY]>255 || value.values[FS_CONSOLE_STATE]>255)
        ((void (*)(u8 *))0x001e2594)(out);
    else out[0]=out[1]=0;
    if(available) {
        if(value.values[FS_CONSOLE_COUNTRY]<=255)out[3]=(u8)value.values[FS_CONSOLE_COUNTRY];
        if(value.values[FS_CONSOLE_STATE]<=255)out[2]=(u8)value.values[FS_CONSOLE_STATE];
    }
}

__attribute__((target("arm")))
u32 MoverConsole_Region(void)
{
    FsConsoleProfile value;
    if(profile(&value) && value.values[FS_CONSOLE_REGION]<=6)return value.values[FS_CONSOLE_REGION];
    return ((u32 (*)(void))0x00109dbc)();
}
