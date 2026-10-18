#include "fs_system/console_settings.h"
#include "fs_system/fs_helpers.h"
#include "fs_system/utf_path.h"

void ConsoleSettings_GetProfile(FsConsoleProfile *out)
{
    *out=(FsConsoleProfile){{FS_CONSOLE_UNKNOWN,FS_CONSOLE_UNKNOWN,
        FS_CONSOLE_UNKNOWN,FS_CONSOLE_UNKNOWN},0,1};
    u32 handle=0;
    if (SRV_GetServiceHandle(&handle,"cfg:u",5,1) || !handle) return;
    volatile u32 *command=fsCommandBuffer();
    command[0]=0x00020000u;
    if (!fsSync(handle) && command[0]==0x00020080u && !command[1]) {
        /* CFG returns a u8; IPC padding bytes are not part of the region.
           CFG 地区为单字节，IPC 填充字节不参与取值和校验。 */
        u32 region=(u8)command[2];
        if(region<=6)out->values[FS_CONSOLE_REGION]=region;
    }
    u8 language=0xff;
    command[0]=0x00010082u;command[1]=1;command[2]=0x000A0002u;
    command[3]=0x1Cu;command[4]=(u32)&language;
    /* Real CFG returns the mapped-buffer descriptor (0x10042); Luma's
       language override may return the result-only form (0x10040). */
    if(!fsSync(handle) && (command[0]==0x00010042u || command[0]==0x00010040u) &&
        !command[1] && language<=11)
        out->values[FS_CONSOLE_LANGUAGE]=language;
    u8 location[4]={0xa5,0xa5,0xa5,0xa5},previous[4];
    for(u32 attempt=0;attempt<2;attempt++) {
        command[0]=0x00010082u;
        command[1]=sizeof(location);command[2]=0x000B0000u;
        command[3]=(sizeof(location)<<4)|0xCu;command[4]=(u32)location;
        if(fsSync(handle))break;
        if((command[0]==0x00010042u || command[0]==0x00010040u) && !command[1]) {
            u8 seed=attempt?0x5a:0xa5;
            if(location[0]!=seed || location[1]!=seed || location[2]!=seed || location[3]!=seed) {
                out->values[FS_CONSOLE_COUNTRY]=location[3];
                out->values[FS_CONSOLE_STATE]=location[2];
            }
            break;
        }
        /* Luma's address override edits the output in place and leaves the
           request header/size unchanged. A second, differently seeded buffer
           distinguishes an overridden byte from an untouched partial override,
           including legitimate country/state values equal to either sentinel. */
        if(command[0]!=0x00010082u || command[1]!=4 || command[2]!=0x000B0000u ||
            command[3]!=0x4Cu || command[4]!=(u32)location || location[0] || location[1])break;
        if(attempt) {
            if(location[3]==previous[3])out->values[FS_CONSOLE_COUNTRY]=location[3];
            if(location[2]==previous[2])out->values[FS_CONSOLE_STATE]=location[2];
        } else for(u32 i=0;i<4;i++){previous[i]=location[i];location[i]=0x5a;}
    }
    if (fsCloseHandle(handle))for(u32 i=0;i<FS_CONSOLE_FIELDS;i++)out->values[i]=FS_CONSOLE_UNKNOWN;
}
void ConsoleSettings_GetLocation(u32 *region,u32 *country)
{
    FsConsoleProfile value;ConsoleSettings_GetProfile(&value);
    *region=value.values[FS_CONSOLE_REGION];
    *country=FsConsole_CountryCode(value.values[FS_CONSOLE_COUNTRY]);
}

int ConsoleSettings_GetUserName(char *out,u32 capacity)
{
    if(!out || !capacity)return 0;
    *out=0;u32 handle=0;
    if(SRV_GetServiceHandle(&handle,"cfg:u",5,1) || !handle)return 0;
    /* System block 0xA0000 is 0x1C bytes; its name occupies ten UTF-16 units. */
    u16 block[14];for(u32 i=0;i<14;i++)block[i]=0xa5a5;
    volatile u32 *c=fsCommandBuffer();
    c[0]=0x00010082u;c[1]=sizeof(block);c[2]=0x000A0000u;
    c[3]=(sizeof(block)<<4)|0xCu;c[4]=(u32)block;
    int ok=!fsSync(handle) && !c[1];
    if(fsCloseHandle(handle))ok=0;
    if(!ok)return 0;
    u16 name[11]={0};u32 used=0,meaningful=0;
    for(;used<10 && block[used];used++) {
        u16 ch=block[used];if(ch==0xa5a5)return 0;
        if(ch>32 && ch!=127)meaningful=1;
        if(ch<33 || ch==127 || ch=='/' || ch=='\\' || ch==':' || ch=='*' ||
            ch=='?' || ch=='"' || ch=='<' || ch=='>' || ch=='|' || ch=='#' || ch=='=')ch='_';
        name[used]=ch;
    }
    if(!used || !meaningful)return 0;
    if(name[used-1]=='.')name[used-1]='_';
    return FsUtf16_ToUtf8(name,used,out,capacity) && *out;
}
