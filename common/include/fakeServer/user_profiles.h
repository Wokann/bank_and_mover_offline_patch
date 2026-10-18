#ifndef COMMON_FAKE_SERVER_USER_PROFILES_H
#define COMMON_FAKE_SERVER_USER_PROFILES_H

#include "types.h"
#include "ticket.h"
#include "fs_system/protected_file.h"

/* 8 prefix + 16 UTF-16 name units + NUL round up to 32 character slots.
   UTF-8 storage needs up to 57 bytes for that generated ID, rounded to 64. */
enum { FS_USER_ID_UNITS=32, FS_USER_NAME_UNITS=16, FS_USER_NAME_SIZE=64,
    FS_USER_ID_SIZE=64, FS_USER_PATH_SIZE=256, FS_USER_RANDOM_DIGITS=12,
    FS_USER_LOCAL=0, FS_USER_NNID=1, FS_USER_PNID=2,
    FS_USER_READY=1, FS_USER_UNINITIALIZED=2 };
typedef struct FsUserEntry { uint32_t number; char id[FS_USER_ID_SIZE]; } FsUserEntry;
typedef struct FsClientConfig {
    FsUserEntry *users;
    uint32_t count,capacity,activeUser;
} FsClientConfig;
typedef struct FsUserIdentity { uint32_t kind; char accountName[FS_USER_NAME_SIZE]; } FsUserIdentity;
typedef struct FsUserProfile {
    uint32_t ready,number;
    char id[FS_USER_ID_SIZE];
} FsUserProfile;

/* A title-page snapshot, not a server identity or a new account. Zero activeUser
   means auto; number/id are the account auto actually resolves to. */
typedef struct FsUserMenu {
    uint32_t count,activeUser,number,changed;
    char id[FS_USER_ID_SIZE];
} FsUserMenu;
typedef int (*FsUserVisitor)(void *,const char *);
typedef struct FsProfileIo {
    const FsStorageIo *files; /* Absolute SD paths, not the selected-user adapter. */
    int (*directories)(const char *,FsUserVisitor,void *); /* 1/0/-1 */
    int (*mkdir)(const char *);
    int (*identify)(FsUserIdentity *);
    int (*random)(void *,uint32_t);
} FsProfileIo;

int FsUser_ValidId(const char *);
int FsUser_ValidAccountName(const char *);
int FsClient_Load(const FsStorageIo *,FsClientConfig *);
int FsClient_Save(const FsStorageIo *,const FsClientConfig *);
int FsClient_LoadFile(const FsStorageIo *,const char *,FsClientConfig *);
int FsClient_SaveFile(const FsStorageIo *,const char *,const char *,const FsClientConfig *);
void FsClient_Release(const FsStorageIo *,FsClientConfig *);
int FsUser_Select(const FsProfileIo *,FsUserProfile *);
/* Resolve without provisioning, registration writes or legacy migration.
   A missing profile is a memory-only view, preserving Bank's first migration. */
/* 只读选择，不创建目录、写注册表或迁移旧档；缺少用户时保留内存视图，留给 Bank 首次迁移。 */
int FsUser_SelectExisting(const FsProfileIo *,FsUserProfile *);
/* Read a title-lifetime catalogue once. Caller releases it with FsClient_Release.
   menu.activeUser is persisted selection; number/id describe auto's resolution.
   Committing re-reads client.conf, not user directories, preserving other values
   and rejecting a selected number/ID that was remapped since the snapshot. */
int FsUser_Catalogue(const FsProfileIo *,FsClientConfig *,FsUserMenu *);
int FsUser_CommitMenu(const FsProfileIo *,const FsUserEntry *,uint32_t count,
    uint32_t activeUser,FsUserMenu *);
int FsUser_Path(const FsUserProfile *,uint32_t client,const char *name,char out[FS_USER_PATH_SIZE]);
#define FS_CLIENT_CONFIG "/3ds/Bank/client/client.conf"
#define FS_CLIENT_TEMP "/3ds/Bank/client/client.tmp"
#define FS_CLIENT_BACKUP "/3ds/Bank/client/client.bak"
#define FS_CLIENT_USERS "/3ds/Bank/client/users"
#define FS_SERVER_USERS "/3ds/Bank/fakeServers/users"

#endif
