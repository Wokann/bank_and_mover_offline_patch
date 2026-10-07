#ifndef BANK_OFFLINE_PATCH_TURTLE_REDIRECT_H
#define BANK_OFFLINE_PATCH_TURTLE_REDIRECT_H

#include "patch_types.h"

enum TurtleRecordConstant {
    TURTLE_RECORD_KNOWN_FIELDS_SIZE = 0x2C,
    TURTLE_RECORD_SERIALIZED_OBJECT_SIZE = 0x30,
    TURTLE_RECORD_FILE_SIZE = 0x200,
    TURTLE_RECORD_INTEGRITY_MARKER = 0x46454544
};

/* Reverse-engineered known fields at the start of the logical data:/turtle
   record. The stock object owns validation and checksum generation; the patch
   treats these bytes as opaque storage data. */
/* 逻辑 data:/turtle 记录开头经逆向确认的已知字段。有效性校验和校验值生成仍由
   原版对象负责；补丁只把这些字节当作不透明存储数据。 */
#pragma pack(push, 1)
typedef struct TurtleRecordKnownFields {
    u64 remoteDataId;                    /* 0x00 */
    u64 pendingTransactionToken;         /* 0x08 */
    u32 currentDataVersion;              /* 0x10 */
    u32 updateVersion;                   /* 0x14 */
    u32 transactionDataLength;           /* 0x18 */
    u32 transactionState;                /* 0x1C */
    u32 integrityMarker;                  /* 0x20 */
    u32 checksum;                         /* 0x24 */
    u16 languageId;                      /* 0x28 */
    u16 useJapaneseKanji;                /* 0x2A */
} TurtleRecordKnownFields;
#pragma pack(pop)

/* Return values expected by the stock Turtle-storage callers. */
/* 原版 Turtle 存储调用方所期望的返回值。 */
enum TurtleBackendResult {
    TURTLE_BACKEND_SUCCESS = 0,
    TURTLE_BACKEND_MISSING = 1,
    TURTLE_BACKEND_INVALID = 2,
    TURTLE_BACKEND_IO_ERROR = 6
};

enum TurtlePathStatus {
    TURTLE_PATH_UNUSABLE = -1,
    TURTLE_PATH_MISSING = 0,
    TURTLE_PATH_VALID = 1
};

enum TurtleStorageConstant {
    TURTLE_OBJECT_VALID = 1,
    TURTLE_APPLICATION_CONDITION_ID = 4,
    TURTLE_MODE_OFFLINE = 0
};

typedef s32 (*TurtleStorage_LoadAtOnceFn)(void *,void *,void *);
typedef s32 (*TurtleStorage_CheckArchiveStatusFn)(void *,void *);
typedef void (*TurtleStorage_FinishSaveFn)(void *,u32,u32,u32);
typedef void (*ApplicationConditionFn)(u32);
typedef void (*TurtleRecord_SetTransactionStateFn)(void *,u8);
typedef void *(*ObjectCreateBufferFn)(void *,u32);
typedef u32 (*ObjectGetSizeFn)(void *);
typedef void *(*ObjectGetDataFn)(void *);
typedef s32 (*ObjectValidateFn)(void *,void *);
typedef void (*ObjectPrepareFn)(void *);
typedef s32 (*GameData_LoadTurtleFn)(void *,void *);
typedef void (*GameData_ClearTurtleFn)(void *);
typedef u32 (*TurtleRecord_GetSettingFn)(void *);
typedef void (*Language_ApplySettingsFn)(u32,void *,u32);

/* Partial views of the native logical-record object, serialized buffer, and
   storage callback objects. */
/* 原版逻辑记录对象、序列化缓冲区与存储回调对象的局部视图。 */
typedef struct TurtleBufferVtableView {
    void *reserved00;
    void *reserved04;
    ObjectGetDataFn getData;
    ObjectGetSizeFn getSize;
} TurtleBufferVtableView;

typedef struct TurtleBufferView {
    TurtleBufferVtableView *vtable;
} TurtleBufferView;

typedef struct TurtleObjectVtableView {
    void *reserved00;
    void *reserved04;
    void *reserved08;
    ObjectCreateBufferFn createBuffer;
    void *reserved10;
    void *reserved14;
    ObjectValidateFn validate;
    ObjectPrepareFn finalize;
    void *reserved20;
    ObjectPrepareFn prepare;
} TurtleObjectVtableView;

typedef struct TurtleObjectView {
    TurtleObjectVtableView *vtable;
} TurtleObjectView;

typedef struct TurtleStorageView {
    u8 reserved00[0x18];
    u8 primaryResult;
    u8 secondaryResult;
    u8 reserved1A[0x02];
    void *object;
} TurtleStorageView;

typedef struct TurtleFormatStateView {
    u8 reserved00[0x78];
    TurtleStorageView *storage;
} TurtleFormatStateView;

typedef struct TurtleLanguageSelectionView {
    void *vtable;
    u32 languageId;
    u8 useJapaneseKanji;
} TurtleLanguageSelectionView;

typedef struct TurtleGameDataView {
    u8 reserved00[0x74];
    TurtleObjectView *record;
    TurtleStorageView *storage;
    u8 reserved7C[0x74];
    TurtleLanguageSelectionView *languageSelection;
} TurtleGameDataView;

#define TurtleStorage_LoadAtOnce \
    ((TurtleStorage_LoadAtOnceFn)0x002BB1A8u)
#define TurtleStorage_CheckArchiveStatus \
    ((TurtleStorage_CheckArchiveStatusFn)0x002BB6E8u)
#define TurtleStorage_FinishSave \
    ((TurtleStorage_FinishSaveFn)0x0022DAC0u)
#define ApplicationCondition_Add \
    ((ApplicationConditionFn)0x001D4D90u)
#define ApplicationCondition_Remove \
    ((ApplicationConditionFn)0x00229EB4u)
#define TurtleRecord_SetTransactionState \
    ((TurtleRecord_SetTransactionStateFn)0x001D4D84u)
#define GameData_LoadTurtle ((GameData_LoadTurtleFn)0x0015DBF0u)
#define GameData_ClearTurtle ((GameData_ClearTurtleFn)0x0015DC50u)
#define TurtleRecord_GetLanguage ((TurtleRecord_GetSettingFn)0x002CB914u)
#define TurtleRecord_GetKanji ((TurtleRecord_GetSettingFn)0x002CB8F4u)
#define Language_ApplySettings ((Language_ApplySettingsFn)0x0025C08Cu)

s32 TurtleRedirect_LoadBackend(void *storage,void *path,void *object);
s32 TurtleRedirect_SaveBackend(void *storage,void *path,void *object);
s32 TurtleRedirect_CheckBackend(void *storage,void *path);
s32 TurtleRedirect_FormatBackend(void *state);
s32 TurtleRedirect_FormatPoll(void *state,u8 *result);
int TurtleRedirect_ClearTransactionAndSave(void *storage,void *object);
int TurtleRedirect_PrepareSession(TurtleGameDataView *gameData,void *heap,u32 mode);

#endif
