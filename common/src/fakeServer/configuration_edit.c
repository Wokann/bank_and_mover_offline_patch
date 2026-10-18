#include "fakeServer/configuration.h"
#include "fakeServer/account_configuration.h"
#include "fakeServer/transaction.h"
#include "fs_system/protected_file.h"

/* This general editor only maintains ticket data. Player-supplied extensions
   are not editable here; the explicit BanUnicodeChinese update has its own
   narrow publisher in custom_settings.c. */
/* 通用编辑器仅处理票务；筛选表只有 custom_settings.c 中指定字段的专用写入。 */
typedef struct ConfigStream {
    const FsStorageIo *io;
    uint32_t input,output,size,position,used,available,written,verify,limit;
    uint8_t buffer[4096];
    FsHashState inputHash,outputHash;
    char raw[FS_CONFIG_LINE_SIZE+2],parsed[FS_CONFIG_LINE_SIZE+2];
} ConfigStream;

static const char sealPrefix[]="# fakeServer-complete=";

static int bytesEqual(const void *a,const void *b,uint32_t size)
{
    const uint8_t *left=a,*right=b;
    for (uint32_t i=0;i<size;i++) if (left[i]!=right[i]) return 0;
    return 1;
}

static int equal(const char *a,const char *b)
{
    if (!a || !b) return a==b;
    while (*a && *a==*b) { a++; b++; }
    return *a==*b;
}

static uint32_t length(const char *text)
{
    uint32_t n=0;
    if (text) while (text[n]) n++;
    return n;
}

static int textValid(const char *text,uint32_t maximum,int name)
{
    if (!text || !*text) return 0;
    uint32_t n=0;
    for (;text[n];n++)
        if (n>=maximum || (uint8_t)text[n]<32 || text[n]=='\x7f' ||
            (name && (text[n]=='=' || text[n]=='[' || text[n]==']' || text[n]==' '))) return 0;
    return 1;
}

static int emit(ConfigStream *stream,const void *body,uint32_t size)
{
    if (stream->written>stream->limit || size>stream->limit-stream->written) return 0;
    if (stream->output) {
        if (stream->verify) {
            uint8_t actual[256];
            for (uint32_t offset=0;offset<size;) {
                uint32_t amount=size-offset;if (amount>sizeof(actual)) amount=sizeof(actual);
                if (!stream->io->read(stream->output,stream->written+offset,actual,amount)) return 0;
                for (uint32_t i=0;i<amount;i++) if (actual[i]!=((const uint8_t *)body)[offset+i]) return 0;
                offset+=amount;
            }
        } else if (!stream->io->write(stream->output,stream->written,body,size)) return 0;
    }
    stream->written+=size;
    FsHash_Update(&stream->outputHash,body,size);
    return 1;
}

static int emitText(ConfigStream *s,const char *text) { return emit(s,text,length(text)); }

static int readLine(ConfigStream *stream,uint32_t *size)
{
    uint32_t used=0;
    while(stream->position<stream->size) {
        if(stream->used==stream->available) {
            stream->available=stream->size-stream->position;
            if(stream->available>sizeof(stream->buffer))stream->available=sizeof(stream->buffer);
            if(!stream->io->read(stream->input,stream->position,stream->buffer,stream->available))return FS_IO_ERROR;
            stream->used=0;
        }
        uint8_t byte=stream->buffer[stream->used++];stream->position++;
        if(!byte)return FS_INVALID;
        if(used>=FS_CONFIG_LINE_SIZE)return FS_LIMIT;
        stream->raw[used++]=(char)byte;
        if(byte=='\n')break;
    }
    stream->raw[used]=0;*size=used;
    FsHash_Update(&stream->inputHash,stream->raw,used);
    return FS_OK;
}

static char *trim(char *text)
{
    while (*text==' ' || *text=='\t') text++;
    char *end=text+length(text);
    while (end>text && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r' || end[-1]=='\n')) end--;
    *end=0;
    return text;
}

static int writeEntry(ConfigStream *s,const FsConfigEdit *edit)
{
    return emitText(s,edit->key) && emitText(s,"=") &&
        emitText(s,edit->value) && emitText(s,";\n");
}

static int flushSection(ConfigStream *s,const char *section,const FsConfigEdit *edits,
    uint32_t count,uint32_t handled[2])
{
    for (uint32_t i=0;i<count;i++)
        if (!(handled[i>>5]&(1u<<(i&31))) && equal(section,edits[i].section)) {
            if (edits[i].action==FS_CONFIG_SET && !writeEntry(s,edits+i)) return 0;
            handled[i>>5]|=1u<<(i&31);
        }
    return 1;
}

static int transform(ConfigStream *s,const FsConfigEdit *edits,uint32_t count)
{
    char section[FS_CONFIG_SECTION_SIZE]={0};
    uint32_t handled[2]={0,0},removed=0,lastNewline=1;
    FsHash_Initialize(&s->inputHash); FsHash_Initialize(&s->outputHash);
    while (s->position<s->size) {
        uint32_t size=0;
        int status=readLine(s,&size);
        if (status!=FS_OK) return status;
        char *raw=s->raw,*parsed=s->parsed;
        for(uint32_t i=0;i<=size;i++)parsed[i]=raw[i];
        char *text=trim(parsed);
        if (s->position==size && size>=3 && (uint8_t)text[0]==0xef &&
            (uint8_t)text[1]==0xbb && (uint8_t)text[2]==0xbf) text=trim(text+3);
        if (size>=sizeof(sealPrefix)-1 && bytesEqual(raw,sealPrefix,sizeof(sealPrefix)-1)) continue;
        if (*text=='[') {
            char *end=text+1;
            while (*end && *end!=']') end++;
            if (!*end || *trim(end+1)) return FS_INVALID;
            if (!lastNewline && !emitText(s,"\n")) return FS_IO_ERROR;
            if (!flushSection(s,section,edits,count,handled)) return FS_IO_ERROR;
            *end=0; text=trim(text+1);
            if (!textValid(text,sizeof(section)-1,1)) return FS_INVALID;
            for (uint32_t i=0;i<=length(text);i++) section[i]=text[i];
            removed=0;
            for (uint32_t i=0;i<count;i++)
                if (equal(section,edits[i].section) && edits[i].action==FS_CONFIG_REMOVE_SECTION)
                    { removed=1; handled[i>>5]|=1u<<(i&31); }
        } else if (!removed && *text && *text!='#' && *text!=';') {
            char *end=text;
            while (*end && *end!='=') end++;
            if(!*end)return FS_INVALID;
            *end=0;text=trim(text);
            int changed=0;
            for (uint32_t i=0;i<count;i++)
                if (equal(section,edits[i].section) && equal(text,edits[i].key)) {
                    if (handled[i>>5]&(1u<<(i&31))) return FS_INVALID;
                    if (edits[i].action==FS_CONFIG_SET && !writeEntry(s,edits+i)) return FS_IO_ERROR;
                    handled[i>>5]|=1u<<(i&31); changed=1;
                }
            if (changed) { lastNewline=1; continue; }
        }
        if (!removed && !emit(s,raw,size)) return FS_IO_ERROR;
        if (!removed) lastNewline=!size || raw[size-1]=='\n';
    }
    if (!lastNewline && !emitText(s,"\n")) return FS_IO_ERROR;
    if (!flushSection(s,section,edits,count,handled)) return FS_IO_ERROR;
    for (uint32_t i=0;i<count;i++) if (!(handled[i>>5]&(1u<<(i&31)))) {
        if (edits[i].action==FS_CONFIG_SET) {
            if (!emitText(s,"[") || !emitText(s,edits[i].section) || !emitText(s,"]\n") ||
                !flushSection(s,edits[i].section,edits,count,handled)) return FS_IO_ERROR;
        } else handled[i>>5]|=1u<<(i&31);
    }
    uint8_t hash[FS_TRANSACTION_HASH_SIZE]; char seal[sizeof(sealPrefix)+2*FS_TRANSACTION_HASH_SIZE];
    FsHash_Finish(&s->outputHash,hash);
    for (uint32_t i=0;i<sizeof(sealPrefix)-1;i++) seal[i]=sealPrefix[i];
    static const char hex[]="0123456789abcdef";
    for (uint32_t i=0;i<FS_TRANSACTION_HASH_SIZE;i++) {
        seal[sizeof(sealPrefix)-1+i*2]=hex[hash[i]>>4];
        seal[sizeof(sealPrefix)+i*2]=hex[hash[i]&15];
    }
    seal[sizeof(seal)-1]='\n';
    return emit(s,seal,sizeof(seal))?FS_OK:FS_IO_ERROR;
}

int FsConfiguration_VerifyTemporary(const FsStorageIo *io,const char *path)
{
    uint32_t handle=0,size=0;
    int status=io->openRead(path,&handle,&size);
    if (!status) return FS_MISSING;
    if (status<0) return FS_IO_ERROR;
    char seal[sizeof(sealPrefix)+2*FS_TRANSACTION_HASH_SIZE]; uint8_t chunk[4096],hash[FS_TRANSACTION_HASH_SIZE]; FsHashState digest;
    int result=size>=sizeof(seal)?FS_OK:FS_INVALID;
    FsHash_Initialize(&digest);
    for (uint32_t offset=0;result==FS_OK && offset<size-sizeof(seal);) {
        uint32_t amount=size-sizeof(seal)-offset;
        if (amount>sizeof(chunk)) amount=sizeof(chunk);
        if (!io->read(handle,offset,chunk,amount)) { result=FS_IO_ERROR; break; }
        FsHash_Update(&digest,chunk,amount); offset+=amount;
    }
    if (result==FS_OK && !io->read(handle,size-sizeof(seal),seal,sizeof(seal))) result=FS_IO_ERROR;
    if (!io->close(handle)) result=FS_IO_ERROR;
    if (result!=FS_OK) return result;
    if (!bytesEqual(seal,sealPrefix,sizeof(sealPrefix)-1) || seal[sizeof(seal)-1]!='\n') return FS_INVALID;
    FsHash_Finish(&digest,hash);
    static const char hex[]="0123456789abcdef";
    for (uint32_t i=0;i<FS_TRANSACTION_HASH_SIZE;i++)
        if (seal[sizeof(sealPrefix)-1+i*2]!=hex[hash[i]>>4] ||
            seal[sizeof(sealPrefix)+i*2]!=hex[hash[i]&15]) return FS_INVALID;
    return FS_OK;
}

int FsConfiguration_Edit(const FsStorageIo *io,FsConfiguration *workspace,uint32_t section,
    const FsConfigEdit *edits,uint32_t count,FsConfigError *error)
{
    if(section>=FS_CONFIG_MOVER_NDS && section<=FS_CONFIG_WORDS_VC)
        return error?error->result=FS_UNSUPPORTED:FS_INVALID;
    if(io && section==FS_CONFIG_TICKET && (io->capabilities&FS_STORAGE_ACCOUNT_LAYOUT))
        return FsAccountConfiguration_Edit(io,workspace,edits,count,error);
    if (!io || !workspace || !edits || !count || count>FS_CONFIG_EDIT_LIMIT || !error ||
        !FsConfiguration_Path(section,0)) return FS_INVALID;
    const char *path=FsConfiguration_Path(section,0),*temp=FsConfiguration_Path(section,1);
    char backup[256];
    if(!FsFile_BackupPath(path,backup,sizeof(backup)))return FS_INVALID;
    uint32_t limit=FS_CONFIG_FILE_LIMIT;
    error->path=path; error->line=0; error->result=FS_INVALID;
    FsConfigEdit normalized[FS_CONFIG_EDIT_LIMIT]; uint32_t normalizedCount=0;
    for (uint32_t i=0;i<count;i++) {
        if (section==FS_CONFIG_TICKET && equal(edits[i].section,"ticket") &&
            edits[i].action==FS_CONFIG_REMOVE_SECTION) {
            if (normalizedCount+FS_TICKET_SETTING_FIELDS>FS_CONFIG_EDIT_LIMIT) return error->result=FS_LIMIT;
            for (uint32_t position=0;position<FS_TICKET_SETTING_FIELDS;position++) {
                uint32_t j=FsConfiguration_TicketOrder(position);
                const char *key=FsConfiguration_TicketKey(j);
                normalized[normalizedCount++]=(FsConfigEdit){FsConfiguration_TicketRequired(j)?FS_CONFIG_SET:FS_CONFIG_REMOVE_KEY,
                    "ticket",key,FsConfiguration_TicketRequired(j)?FsConfiguration_TicketDefault(key):0};
            }
        } else {
            if (normalizedCount==FS_CONFIG_EDIT_LIMIT) return error->result=FS_LIMIT;
            normalized[normalizedCount]=edits[i];
            if (section==FS_CONFIG_TICKET && equal(edits[i].section,"ticket") &&
                edits[i].action==FS_CONFIG_REMOVE_KEY) {
                const char *value=FsConfiguration_TicketDefault(edits[i].key);
                if (!value) return FS_INVALID;
                int field=FsConfiguration_TicketField(edits[i].key);
                if (field>=0 && FsConfiguration_TicketRequired((uint32_t)field)) {
                    normalized[normalizedCount].action=FS_CONFIG_SET;
                    normalized[normalizedCount].value=value;
                }
            }
            normalizedCount++;
        }
    }
    edits=normalized; count=normalizedCount;
    for (uint32_t i=0;i<count;i++) {
        const FsConfigEdit *e=edits+i;
        int valueValid=e->value && (!*e->value || (textValid(e->value,FS_CONFIG_LINE_SIZE-1,0) &&
            length(e->key)+length(e->value)+1<FS_CONFIG_LINE_SIZE));
        if (e->action>FS_CONFIG_REMOVE_SECTION ||
            !textValid(e->section,FS_CONFIG_SECTION_SIZE-1,1) ||
            (e->action!=FS_CONFIG_REMOVE_SECTION && !textValid(e->key,63,1)) ||
            (e->action==FS_CONFIG_SET && !valueValid)) return FS_INVALID;
        for (uint32_t j=0;j<i;j++)
            if (equal(e->section,edits[j].section) && (e->action==FS_CONFIG_REMOVE_SECTION ||
                edits[j].action==FS_CONFIG_REMOVE_SECTION || equal(e->key,edits[j].key))) return FS_INVALID;
    }
    uint32_t input=0,output=0,size=0,tempSize=0;
    int present=io->openRead(path,&input,&size);
    if (present<0) return error->result=FS_IO_ERROR;
    if (size>limit) {
        if (present && !io->close(input)) return error->result=FS_IO_ERROR;
        return error->result=FS_LIMIT;
    }
    if (!present) {
        int staged=io->openRead(temp,&output,&tempSize);
        if (staged) {
            if (staged==1 && !io->close(output)) return error->result=FS_IO_ERROR;
            return error->result=staged<0?FS_IO_ERROR:FS_LOCKED;
        }
        if (section==FS_CONFIG_TICKET) return error->result=FS_MISSING;
    }
    ConfigStream stream={.io=io,.input=input,.size=size,.limit=limit};
    int result=transform(&stream,edits,count);
    uint32_t expected=stream.written;
    uint8_t originalHash[FS_TRANSACTION_HASH_SIZE],currentHash[FS_TRANSACTION_HASH_SIZE]; FsHash_Finish(&stream.inputHash,originalHash);
    if (result==FS_OK && (!io->remove(temp) || !io->openWrite(temp,expected,&output))) result=FS_IO_ERROR;
    if (result==FS_OK) {
        stream=(ConfigStream){.io=io,.input=input,.output=output,.size=size,.limit=limit};
        result=transform(&stream,edits,count);
    }
    if (output && !io->close(output)) result=FS_IO_ERROR;
    if (present && !io->close(input)) result=FS_IO_ERROR;
    if (result!=FS_OK) return error->result=result;
    result=FsConfiguration_ValidateFile(io,workspace,section,temp,error);
    if (result!=FS_OK) return result;
    int current=io->openRead(path,&input,&size);
    if (current!=present || size!=stream.size) {
        if (current==1) io->close(input);
        return error->result=current<0?FS_IO_ERROR:FS_LOCKED;
    }
    if (io->openRead(temp,&output,&tempSize)!=1) {
        if (current) io->close(input);
        return error->result=FS_IO_ERROR;
    }
    stream=(ConfigStream){.io=io,.input=input,.output=output,.size=size,.verify=1,.limit=limit};
    result=tempSize==expected?transform(&stream,edits,count):FS_IO_ERROR;
    FsHash_Finish(&stream.inputHash,currentHash);
    if (result==FS_OK && !bytesEqual(originalHash,currentHash,FS_TRANSACTION_HASH_SIZE)) result=FS_LOCKED;
    if (!io->close(output)) result=FS_IO_ERROR;
    if (current && !io->close(input)) result=FS_IO_ERROR;
    error->path=path;
    if (result==FS_OK) result=FsFile_PublishStaged(io,path,temp,backup,present);
    return error->result=result;
}
