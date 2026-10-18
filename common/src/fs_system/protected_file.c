#include "fs_system/protected_file.h"
#include <string.h>

/* All current ASCII configuration paths use the adapter's 256-byte path ABI.
   No file-size, word-count or line-length limit is imposed by this layer. */
enum { FILE_PATH_CAPACITY=256 };

int FsFile_BackupPath(const char *path,char *out,uint32_t capacity)
{
    if(!path || !out)return 0;
    uint32_t length=0,dot=UINT32_MAX;
    for(;path[length];length++) {
        if(length>=capacity)return 0;
        if(path[length]=='/')dot=UINT32_MAX;
        else if(path[length]=='.')dot=length;
    }
    if(dot==UINT32_MAX || !dot || dot>UINT32_MAX-5 || dot+5>capacity)return 0;
    memcpy(out,path,dot);memcpy(out+dot,".bak",5);return 1;
}

int FsFile_OpenPublished(const FsStorageIo *io,const char *path,const char *backup,
    uint32_t *handle,uint32_t *size,int *fromBackup)
{
    *handle=0;*size=0;if(fromBackup)*fromBackup=0;
    int found=io->openRead(path,handle,size);
    if(found || !backup)return found;
    found=io->openRead(backup,handle,size);
    if(found==1 && fromBackup)*fromBackup=1;
    return found;
}

int FsFile_PublishStaged(const FsStorageIo *io,const char *path,const char *temp,
    const char *backup,int mainPresent)
{
    if(io->capabilities&FS_STORAGE_READ_ONLY)return FS_IO_ERROR;
    if(!strcmp(path,temp) || !strcmp(path,backup) || !strcmp(temp,backup))return FS_INVALID;
    if(mainPresent && (!io->remove(backup) || !io->rename(path,backup)))return FS_IO_ERROR;
    return io->rename(temp,path)?FS_OK:FS_IO_ERROR;
}

int FsFile_Complete(const FsStorageIo *io,const char *path,const char *temp,
    const char *backup,uint32_t policy,int acknowledged)
{
    if(policy==FS_FILE_KEEP_BACKUP)return FS_OK;
    if(policy!=FS_FILE_RETIRE_AFTER_ACK || acknowledged!=1)return FS_LOCKED;
    if(io->capabilities&FS_STORAGE_READ_ONLY)return FS_IO_ERROR;
    if(!strcmp(path,temp) || !strcmp(path,backup) || !strcmp(temp,backup))return FS_INVALID;
    uint32_t handle=0,size=0;
    if(io->openRead(path,&handle,&size)!=1)return FS_IO_ERROR;
    if(!io->close(handle))return FS_IO_ERROR;
    return io->remove(temp) && io->remove(backup)?FS_OK:FS_IO_ERROR;
}

int FsText_Read(const FsStorageIo *io,const char *path,char **text,uint32_t *length)
{
    char backup[FILE_PATH_CAPACITY];
    uint32_t handle=0,size=0;*text=0;*length=0;
    if(!FsFile_BackupPath(path,backup,sizeof(backup)))return FS_INVALID;
    int found=FsFile_OpenPublished(io,path,backup,&handle,&size,0);
    if(found!=1)return found<0?FS_IO_ERROR:FS_MISSING;
    if(size==UINT32_MAX){io->close(handle);return FS_NO_MEMORY;}
    char *data=io->allocate(size+1);
    if(!data){io->close(handle);return FS_NO_MEMORY;}
    int ok=!size || io->read(handle,0,data,size);
    if(!io->close(handle))ok=0;
    if(!ok){io->release(data);return FS_IO_ERROR;}
    data[size]=0;*text=data;*length=size;return FS_OK;
}

int FsText_Publish(const FsStorageIo *io,const char *path,const char *temp,
    const char *text,uint32_t size)
{
    char backup[FILE_PATH_CAPACITY];
    if(!FsFile_BackupPath(path,backup,sizeof(backup)))return FS_INVALID;
    if(!strcmp(path,temp) || !strcmp(path,backup) || !strcmp(temp,backup))return FS_INVALID;
    uint32_t handle=0,actual=0;uint8_t buffer[512];
    int found=io->openRead(path,&handle,&actual),same=found==1 && actual==size;
    if(found<0)return FS_IO_ERROR;
    for(uint32_t pos=0;same && pos<size;) {
        uint32_t n=size-pos;if(n>sizeof(buffer))n=sizeof(buffer);
        if(!io->read(handle,pos,buffer,n)){io->close(handle);return FS_IO_ERROR;}
        same=!memcmp(buffer,text+pos,n);pos+=n;
    }
    if(found==1 && !io->close(handle))return FS_IO_ERROR;
    if(same)return FS_OK;
    if(io->capabilities&FS_STORAGE_READ_ONLY)return FS_IO_ERROR;
    if(!io->remove(temp) || !io->openWrite(temp,size,&handle))return FS_IO_ERROR;
    int ok=!size || io->write(handle,0,text,size);
    if(!io->close(handle))ok=0;
    if(!ok || io->openRead(temp,&handle,&actual)!=1)return FS_IO_ERROR;
    ok=actual==size;
    for(uint32_t pos=0;ok && pos<size;) {
        uint32_t n=size-pos;if(n>sizeof(buffer))n=sizeof(buffer);
        ok=io->read(handle,pos,buffer,n) && !memcmp(buffer,text+pos,n);pos+=n;
    }
    if(!io->close(handle))ok=0;
    if(!ok)return FS_IO_ERROR;
    return FsFile_PublishStaged(io,path,temp,backup,found==1);
}
