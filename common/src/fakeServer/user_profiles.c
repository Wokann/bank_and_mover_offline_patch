#include "fakeServer/user_profiles.h"
#include "fakeServer/memory.h"
#include "fakeServer/numbers.h"
#include "fakeServer/conf_text.h"
#include "fs_system/utf_path.h"
#include "fs_system/diagnostic_log.h"
#include <string.h>

static int number(const char *text,uint32_t *out)
{ return FsParseU32(text,0x7fffffffu,out) && *out; }
static int sameId(const char *a,const char *b)
{
    for(;;a++,b++) {
        uint8_t x=(uint8_t)*a,y=(uint8_t)*b;
        if(x>='A' && x<='Z')x=(uint8_t)(x+32);
        if(y>='A' && y<='Z')y=(uint8_t)(y+32);
        if(x!=y)return 0;
        if(!x)return 1;
    }
}
static char *trim(char *text)
{
    while (*text==' ' || *text=='\t' || *text=='\r') text++;
    char *end=text+strlen(text);
    while (end>text && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r')) *--end=0;
    return text;
}
static char *decimal(char *text,uint32_t value)
{
    char digits[10];uint32_t count=0;
    do { uint32_t digit;value=FsDivideU32(value,10,&digit);digits[count++]=(char)('0'+digit); } while(value);
    while(count) *text++=digits[--count];
    *text=0;return text;
}
int FsUser_ValidAccountName(const char *text)
{
    if(!text)return 0;
    uint32_t i=0;
    for(;i<17 && text[i];i++) {
        uint8_t c=(uint8_t)text[i];
        if(!((c>='0' && c<='9') || (c>='A' && c<='Z') ||
            (c>='a' && c<='z') || c=='_' || c=='-' || c=='.'))return 0;
    }
    return i>=6 && i<17 && text[0]!='.' && text[i-1]!='.';
}
int FsUser_ValidId(const char *id)
{
    if (!id) return 0;
    uint32_t prefix=!strncmp(id,"nnid_",5) || !strncmp(id,"pnid_",5)?5:
        !strncmp(id,"localid_",8)?8:0;
    if (!prefix || !id[prefix]) return 0;
    uint32_t length=0;while(length<FS_USER_ID_SIZE && id[length])length++;
    if(length>=FS_USER_ID_SIZE || id[length-1]=='.' || id[length-1]==' ')return 0;
    const char *cursor=id+prefix,*end=id+length;
    while(cursor<end) {
        uint32_t c;if(!FsUtf8_Next(&cursor,end,&c))return 0;
        if(c<0x80 && !((c>='0' && c<='9') || (c>='A' && c<='Z') ||
            (c>='a' && c<='z') || c=='_' || c=='-' || c=='.'))return 0;
        if(c>=0x7f && (prefix!=8 || c<0xa0))return 0;
    }
    return 1;
}
static int addUser(const FsStorageIo *io,FsClientConfig *config,uint32_t slot,const char *id)
{
    if (!FsUser_ValidId(id)) return FS_INVALID;
    uint32_t maximum=0;
    for (uint32_t i=0;i<config->count;i++) {
        if (config->users[i].number>maximum) maximum=config->users[i].number;
        if (sameId(config->users[i].id,id)) return FS_OK;
        if (slot && config->users[i].number==slot) return FS_INVALID;
    }
    if (!slot) { if(maximum==0x7fffffffu)return FS_LIMIT;slot=maximum+1; }
    FsMemory memory={io->allocate,io->release};
    int result=FsMemory_Reserve(&memory,(void **)&config->users,&config->capacity,
        config->count+1,config->count,sizeof(*config->users));
    if (result!=FS_OK) return result;
    uint32_t at=config->count++;
    while(at && config->users[at-1].number>slot) { config->users[at]=config->users[at-1];at--; }
    config->users[at].number=slot;strcpy(config->users[at].id,id);
    return FS_OK;
}
void FsClient_Release(const FsStorageIo *io,FsClientConfig *config)
{ if(config->users)io->release(config->users);*config=(FsClientConfig){0}; }
int FsClient_LoadFile(const FsStorageIo *io,const char *path,FsClientConfig *config)
{
    *config=(FsClientConfig){0};
    char *text=0;uint32_t size=0;int result=FsText_Read(io,path,&text,&size);
    if (result==FS_MISSING) return FS_OK;
    if (result!=FS_OK) return result;
    uint32_t section=0;char *cursor=text;
    if(size>=3 && (uint8_t)cursor[0]==0xef && (uint8_t)cursor[1]==0xbb && (uint8_t)cursor[2]==0xbf)cursor+=3;
    while(cursor<text+size) {
        char *line=cursor;int damaged=0;
        while(cursor<text+size && *cursor!='\n'){if(!*cursor)damaged=1;cursor++;}
        if(cursor<text+size)*cursor++=0;
        if(damaged){if(*trim(line)=='[')section=0;continue;}
        char *comment=strchr(line,'#');if(comment)*comment=0;line=trim(line);
        if(!strcmp(line,"[global]")){section=1;continue;}
        if(!strcmp(line,"[users]")){section=2;continue;}
        if(*line=='['){section=0;continue;}
        char *value=strchr(line,'=');if(!value)continue;*value++=0;line=trim(line);value=trim(value);
        FsConf_Value(value);
        if(section==2) {
            uint32_t slot=0;if(!number(line,&slot))continue;
            result=addUser(io,config,slot,value);
            if(result==FS_NO_MEMORY || result==FS_LIMIT)break;
            result=FS_OK;
        } else if(section==1) {
            if(!strcmp(line,"activeUser")) {
                uint32_t slot=0;
                if(!strcmp(value,"auto"))config->activeUser=0;
                else if(number(value,&slot))config->activeUser=slot;
            }
        }
    }
    io->release(text);
    if(result!=FS_OK){FsClient_Release(io,config);return result;}
    if(config->activeUser) {
        int found=0;for(uint32_t i=0;i<config->count;i++)if(config->users[i].number==config->activeUser)found=1;
        if(!found)config->activeUser=0;
    }
    char temp[FS_USER_PATH_SIZE];
    if(!FsFile_BackupPath(path,temp,sizeof(temp)))result=FS_INVALID;
    else {
        strcpy(temp+strlen(temp)-4,".tmp");
        result=FsClient_SaveFile(io,path,temp,config);
    }
    if(result!=FS_OK)FsClient_Release(io,config);
    return result;
}
int FsClient_SaveFile(const FsStorageIo *io,const char *path,const char *temp,const FsClientConfig *config)
{
    if(config->count>(UINT32_MAX-256)/(FS_USER_ID_SIZE+16))return FS_NO_MEMORY;
    uint32_t capacity=256+config->count*(FS_USER_ID_SIZE+16);
    char *text=io->allocate(capacity);if(!text)return FS_NO_MEMORY;
    char *out=text;strcpy(out,"[global]\nactiveUser=");out+=strlen(out);
    if(config->activeUser)out=decimal(out,config->activeUser);
    else {strcpy(out,"auto");out+=4;}
    strcpy(out,";\n\n[users]\n");out+=strlen(out);
    for(uint32_t i=0;i<config->count;i++) {
        out=decimal(out,config->users[i].number);*out++='=';
        strcpy(out,config->users[i].id);out+=strlen(out);*out++=';';*out++='\n';
    }
    int result=FsText_Publish(io,path,temp,text,(uint32_t)(out-text));
    io->release(text);return result;
}
int FsClient_Load(const FsStorageIo *io,FsClientConfig *config)
{ return FsClient_LoadFile(io,FS_CLIENT_CONFIG,config); }
int FsClient_Save(const FsStorageIo *io,const FsClientConfig *config)
{ return FsClient_SaveFile(io,FS_CLIENT_CONFIG,FS_CLIENT_TEMP,config); }
int FsUser_Path(const FsUserProfile *user,uint32_t client,const char *name,char out[FS_USER_PATH_SIZE])
{
    if(!user || !FsUser_ValidId(user->id) || !name || *name=='/' || strchr(name,'\\') || strstr(name,".."))return 0;
    const char *root=client?FS_CLIENT_USERS:FS_SERVER_USERS;
    uint32_t size=(uint32_t)(strlen(root)+strlen(user->id)+strlen(name)+3);
    if(size>FS_USER_PATH_SIZE)return 0;
    strcpy(out,root);strcat(out,"/");strcat(out,user->id);
    if(*name){strcat(out,"/");strcat(out,name);}return 1;
}
typedef struct Scan { const FsStorageIo *io;FsClientConfig *config;uint32_t any;int result; } Scan;
static int directory(void *context,const char *name)
{
    Scan *scan=context;scan->any=1;
    if(FsUser_ValidId(name))scan->result=addUser(scan->io,scan->config,0,name);
    return scan->result==FS_OK;
}
typedef struct LocalIdProbe { const char *id;uint32_t any,matched; } LocalIdProbe;
static int probeLocalId(void *context,const char *name)
{
    LocalIdProbe *probe=context;probe->any=1;
    if(sameId(name,probe->id))probe->matched=1;
    return 1;
}
static int localIdAvailable(const FsProfileIo *io,const FsClientConfig *config,
    const char *id,uint32_t *any)
{
    for(uint32_t i=0;i<config->count;i++)if(sameId(config->users[i].id,id))return 0;
    static const char *const roots[]={FS_SERVER_USERS,FS_CLIENT_USERS};
    FsUserProfile candidate={0};strcpy(candidate.id,id);
    for(uint32_t i=0;i<2;i++) {
        LocalIdProbe probe={id,0,0};
        int found=io->directories(roots[i],probeLocalId,&probe);
        if(found<0)return -1;
        if(probe.any)*any=1;
        if(probe.matched)return 0;
        if(!found)continue;
        /* A regular file also occupies the target name. Never replace it. */
        char path[FS_USER_PATH_SIZE];uint32_t handle=0,size=0;
        if(!FsUser_Path(&candidate,i,"",path))return -1;
        found=io->files->openRead(path,&handle,&size);
        if(found<0)return -1;
        if(found==1) {
            if(!io->files->close(handle))return -1;
            *any=1;return 0;
        }
    }
    return 1;
}
static int newLocalId(const FsProfileIo *io,const FsClientConfig *config,
    char id[FS_USER_ID_SIZE],uint32_t *any)
{
    /* Rejection sampling gives each decimal digit the same probability.
       Bounded retries protect the worker from a broken/constant random source. */
    for(uint32_t attempt=0;attempt<32;attempt++) {
        uint32_t used=0;strcpy(id,"localid_");
        for(uint32_t batch=0;batch<32 && used<FS_USER_RANDOM_DIGITS;batch++) {
            uint8_t random[16];
            if(io->random(random,sizeof(random))!=1)return FS_IO_ERROR;
            for(uint32_t i=0;i<sizeof(random) && used<FS_USER_RANDOM_DIGITS;i++)if(random[i]<250) {
                uint32_t digit;FsDivideU32(random[i],10,&digit);id[8+used++]=(char)('0'+digit);
            }
        }
        if(used!=FS_USER_RANDOM_DIGITS)return FS_IO_ERROR;
        id[8+used]=0;
        int available=localIdAvailable(io,config,id,any);
        if(available<0)return FS_IO_ERROR;
        if(available)return FS_OK;
    }
    return FS_IO_ERROR;
}
/* Only v0.9.1 bank/settings images are eligible. A rename never overwrites a target;
   any user directory (on either side) disables all automatic migration. */
/* 迁移 v0.9.1 银行及客户端记录；两端任一已有用户目录就不再自动迁移。 */
static int migrateLegacy(const FsProfileIo *io,const FsUserProfile *user)
{
    static const char *const names[]={"bankdata.bin","bankdata.bak","bankdata.tmp","sav.bin","sav.bak","sav.tmp"};
    char paths[6][FS_USER_PATH_SIZE],old[64];uint32_t handle=0,size=0;
    /* Preflight the entire set before moving anything. */
    for(uint32_t i=0;i<6;i++) {
        if(!FsUser_Path(user,i>=3,names[i],paths[i]))return FS_INVALID;
        int present=io->files->openRead(paths[i],&handle,&size);
        if(present==1 && !io->files->close(handle))return FS_IO_ERROR;
        if(present<0)return FS_IO_ERROR;
        if(present)return FS_OK;
    }
    for(uint32_t i=0;i<6;i++) {
        strcpy(old,"/3ds/Bank/");strcat(old,names[i]);
        int present=io->files->openRead(old,&handle,&size);
        if(present<0)return FS_IO_ERROR;
        if(!present)continue;
        if(!io->files->close(handle) || !io->files->rename(old,paths[i]))return FS_IO_ERROR;
    }
    return FS_OK;
}
static int ignoreDirectory(void *context,const char *name)
{ (void)context;(void)name;return 1; }

static int configuredUser(const FsProfileIo *io,const FsClientConfig *config,
    FsUserProfile *selected,int initialize,uint32_t *any)
{
    if(config->activeUser)for(uint32_t i=0;i<config->count;i++)
        if(config->users[i].number==config->activeUser)strcpy(selected->id,config->users[i].id);
    if(!*selected->id) {
        FsUserIdentity identity={0};
        int identified=io->identify(&identity)==1;
        if(identified && FsUser_ValidAccountName(identity.accountName) &&
            (identity.kind==FS_USER_NNID || identity.kind==FS_USER_PNID)) {
            strcpy(selected->id,identity.kind==FS_USER_NNID?"nnid_":"pnid_");
            strcat(selected->id,identity.accountName);
        } else if(identified && identity.kind==FS_USER_LOCAL && *identity.accountName) {
            char candidate[FS_USER_ID_SIZE]="localid_";
            uint32_t length=0;while(length<sizeof(identity.accountName) && identity.accountName[length])length++;
            uint16_t name[FS_USER_NAME_UNITS+1];
            if(length+8<sizeof(candidate) &&
                FsUtf8_ToUtf16(identity.accountName,name,FS_USER_NAME_UNITS+1,0))strcat(candidate,identity.accountName);
            if(FsUser_ValidId(candidate))strcpy(selected->id,candidate);
        }
        if(!*selected->id) {
            for(uint32_t i=0;i<config->count;i++)if(!strncmp(config->users[i].id,"localid_",8)) {
                strcpy(selected->id,config->users[i].id);break;
            }
            if(!*selected->id && initialize)return newLocalId(io,config,selected->id,any);
        }
    }
    return FS_OK;
}

static int describeMenu(const FsProfileIo *io,const FsClientConfig *config,FsUserMenu *menu)
{
    FsUserMenu next={.count=config->count,.activeUser=config->activeUser};
    FsUserProfile selected={0};uint32_t any=0;
    int result=configuredUser(io,config,&selected,0,&any);
    if(result!=FS_OK)return result;
    strcpy(next.id,selected.id);
    for(uint32_t i=0;i<config->count;i++)if(sameId(config->users[i].id,selected.id)) {
        next.number=config->users[i].number;strcpy(next.id,config->users[i].id);
    }
    *menu=next;return FS_OK;
}
int FsUser_Catalogue(const FsProfileIo *io,FsClientConfig *config,FsUserMenu *menu)
{
    if(!io || !io->files || !config || !menu)return FS_INVALID;
    int result=FsClient_Load(io->files,config);
    if(result!=FS_OK)return result;
    Scan scan={io->files,config,0,FS_OK};
    int left=io->directories(FS_CLIENT_USERS,directory,&scan);
    int right=io->directories(FS_SERVER_USERS,directory,&scan);
    if(left<0 || right<0 || scan.result!=FS_OK){result=FS_IO_ERROR;goto done;}
    FsClientConfig automatic=*config;automatic.activeUser=0;
    result=describeMenu(io,&automatic,menu);
    if(result==FS_OK){menu->activeUser=config->activeUser;return FS_OK;}
done:
    FsClient_Release(io->files,config);return result;
}
int FsUser_CommitMenu(const FsProfileIo *io,const FsUserEntry *entries,uint32_t count,
    uint32_t activeUser,FsUserMenu *menu)
{
    if(!io || !io->files || !menu || (!entries && count))return FS_INVALID;
    const FsUserEntry *selected=0;
    for(uint32_t i=0;i<count;i++) {
        if(!entries[i].number || entries[i].number>0x7fffffffu ||
            (i && entries[i-1].number>=entries[i].number) || !FsUser_ValidId(entries[i].id))return FS_INVALID;
        if(entries[i].number==activeUser)selected=&entries[i];
    }
    if(activeUser && !selected)return FS_INVALID;
    FsClientConfig config;int result=FsClient_Load(io->files,&config);
    if(result!=FS_OK)goto done;
    uint32_t previous=config.activeUser,previousCount=config.count;
    if(selected)for(uint32_t i=0;i<config.count;i++) {
        int numberMatches=config.users[i].number==selected->number;
        int idMatches=sameId(config.users[i].id,selected->id);
        if(numberMatches!=idMatches){result=FS_LOCKED;goto done;}
    }
    /* Add catalogue discoveries without rescanning SD or overwriting a current
       binding. Only a conflict involving the chosen user prevents entry. */
    for(uint32_t i=0;i<count;i++) {
        result=addUser(io->files,&config,entries[i].number,entries[i].id);
        if(result==FS_INVALID)continue;
        if(result!=FS_OK)goto done;
    }
    config.activeUser=activeUser;
    result=FsClient_Save(io->files,&config);
    if(result!=FS_OK)goto done;
    result=describeMenu(io,&config,menu);
    if(result==FS_OK)menu->changed=previous!=activeUser || previousCount!=config.count;
done:
    FsClient_Release(io->files,&config);return result;
}

static int selectUser(const FsProfileIo *io,FsUserProfile *user,int initialize)
{
    if(initialize && user->ready==FS_USER_UNINITIALIZED)user->ready=0;
    if(user->ready)return FS_OK;
    FsClientConfig config;int result=FsClient_Load(io->files,&config);
    FsDiag_Checkpoint("user.select.config",result,(uint32_t)initialize,0,0);
    if(result!=FS_OK)return result;
    Scan scan={io->files,&config,0,FS_OK};
    int left=io->directories(FS_CLIENT_USERS,directory,&scan);
    int right=io->directories(FS_SERVER_USERS,directory,&scan);
    FsDiag_Record("user.select.scan",scan.result,(uint32_t)left,(uint32_t)right,scan.any);
    if(left<0 || right<0 || scan.result!=FS_OK){result=FS_IO_ERROR;goto done;}
    FsUserProfile selected={0};
    result=configuredUser(io,&config,&selected,initialize,&scan.any);if(result!=FS_OK)goto done;
    if(!initialize) {
        /* Even an empty directory is established state. Never create one here:
           doing so would disable Bank's six-file first-user migration. */
        /* 空目录也算既有状态；此处不能创建目录，否则会阻止 Bank 的首次六文件迁移。 */
        int existing=0;
        if(*selected.id) {
            char path[FS_USER_PATH_SIZE];
            for(uint32_t side=0;side<2;side++) {
                if(!FsUser_Path(&selected,side,"",path)){result=FS_INVALID;goto done;}
                int found=io->directories(path,ignoreDirectory,0);
                if(found<0){result=FS_IO_ERROR;goto done;}
                existing|=found;
            }
            for(uint32_t i=0;i<config.count;i++)if(sameId(config.users[i].id,selected.id)) {
                selected.number=config.users[i].number;strcpy(selected.id,config.users[i].id);
            }
        }
        selected.ready=existing?FS_USER_READY:FS_USER_UNINITIALIZED;
        *user=selected;result=FS_OK;goto done;
    }
    result=addUser(io->files,&config,0,selected.id);if(result!=FS_OK)goto done;
    for(uint32_t i=0;i<config.count;i++)if(sameId(config.users[i].id,selected.id)) {
        selected.number=config.users[i].number;strcpy(selected.id,config.users[i].id);
    }
    char path[FS_USER_PATH_SIZE];
    static const char *const roots[]={"/3ds","/3ds/Bank","/3ds/Bank/client",FS_CLIENT_USERS,
        "/3ds/Bank/fakeServers",FS_SERVER_USERS,"/3ds/Bank/fakeServers/shared"};
    for(uint32_t i=0;i<sizeof(roots)/sizeof(*roots);i++)if(!io->mkdir(roots[i])){result=FS_IO_ERROR;goto done;}
    FsDiag_Checkpoint("user.select.roots.ready",FS_OK,selected.number,0,0);
    if(!FsUser_Path(&selected,1,"",path) || !io->mkdir(path) ||
        !FsUser_Path(&selected,0,"",path) || !io->mkdir(path)){result=FS_IO_ERROR;goto done;}
    if(!scan.any){result=migrateLegacy(io,&selected);if(result!=FS_OK)goto done;}
    result=FsClient_Save(io->files,&config);
    if(result==FS_OK){selected.ready=FS_USER_READY;*user=selected;}
done:
    FsDiag_Record("user.select.end",result,(uint32_t)initialize,0,0);
    FsClient_Release(io->files,&config);return result;
}

int FsUser_Select(const FsProfileIo *io,FsUserProfile *user)
{ return selectUser(io,user,1); }

int FsUser_SelectExisting(const FsProfileIo *io,FsUserProfile *user)
{ return selectUser(io,user,0); }
