#ifndef COMMON_FAKE_SERVER_MOVER_CHECKS_H
#define COMMON_FAKE_SERVER_MOVER_CHECKS_H

#include "types.h"
#include "word_pattern.h"
#include "custom_settings.h"

enum FsMoverPolicyLimit {
    FS_MOVER_NAME_UNITS=13, FS_MOVER_RULE_NAME_SIZE=32
};
enum FsMoverReplyLimit {
    FS_MOVER_SLOT_COUNT=30, FS_MOVER_REPLY_TAIL_SIZE=0x100,
    FS_MOVER_REPLY_SIZE=3+FS_MOVER_SLOT_COUNT*4+FS_MOVER_REPLY_TAIL_SIZE
};
enum FsMoverSource { FS_MOVER_GEN5=1, FS_MOVER_VC=2, FS_MOVER_ALL=3 };
/* Block rules use NGWord; exceptions are exact nickname/OT values. Optional
   region selectors share tag bits in the transient UTF-16 word pool. */
/* 禁词统一使用 NGWord，例外只精确匹配昵称或 OT；地区复用内存词池的标记空位。 */
enum {
    FS_MOVER_WORD_TAG=20, FS_MOVER_ALLOW_NICKNAME_TAG=24, FS_MOVER_ALLOW_OT_TAG=28,
    FS_MOVER_WORD_KIND_MASK=28, FS_MOVER_WORD_REGION_SHIFT=5
};
enum FsMoverRuleAction { FS_MOVER_REJECT=1, FS_MOVER_REQUIRE=2 };
typedef struct FsMoverExpression FsMoverExpression;

/* Native observations only. The service never mutates a game record. */
/* 只接收原生观察值；服务不会直接改写游戏记录。 */
typedef struct FsMoverDetails {
    uint32_t source;
    uint16_t moves[4];
    uint16_t nickname[FS_MOVER_NAME_UNITS];
    uint16_t trainer[FS_MOVER_NAME_UNITS];
    uint16_t tid,sid;
    uint8_t form,level,shiny,language,origin,nicknamed,metLevel,fateful;
} FsMoverDetails;

typedef struct FsMoverRule {
    char name[FS_MOVER_RULE_NAME_SIZE];
    FsMoverExpression *condition;
    uint8_t enabled,action,sources,defined;
} FsMoverRule;
/* Aligned variable-length RAM records; no packed 8-bit length field. */
/* 内存变长记录按四字节对齐，不再把词长塞进八位标记。 */
typedef struct FsMoverWord { uint32_t length,tag; uint16_t text[]; } FsMoverWord;
static inline uint32_t FsMover_WordBytes(uint32_t length)
{ return 8u+2u*((length+1u)&~1u); }
typedef struct FsMoverPolicy {
    /* The text determines the rule count; capacity is only a RAM allocation,
       not an on-disk format or a fixed limit on the number of rule sections. */
    /* 规则数由文本决定；capacity 仅是内存分配量，不是文件格式或固定段数上限。 */
    FsMoverRule *rules;
    void *words;
    uint32_t ruleCount,ruleCapacity,wordCount,wordBytes,wordCapacity;
    uint32_t wordSkipped[2],wordSkippedLine[2];
    uint32_t customOptions;
    FsWordWorkspace wordWork;
} FsMoverPolicy;
static inline FsMoverWord *FsMover_WordAt(const FsMoverPolicy *policy,uint32_t offset)
{ return (FsMoverWord *)((uint8_t *)policy->words+offset); }

/* Load-only acceleration. Never persisted or kept in the service context. */
/* 仅加载期间的索引，不落盘，也不常驻服务上下文。 */
typedef struct FsMoverWordIndex {
    void *slots;
    uint32_t capacity,count;
} FsMoverWordIndex;

int FsMover_DecodeText(const char *,uint16_t *,uint32_t capacity);
int FsMover_AddWord(FsMoverPolicy *,const char *,uint32_t sources);
void FsMover_ReleaseWords(FsMoverPolicy *);
void FsMover_ReleaseWordIndex(const FsMoverPolicy *,FsMoverWordIndex *);
int FsMover_IndexWords(FsMoverPolicy *,FsMoverWordIndex *);
int FsMover_AddIndexedWord(FsMoverPolicy *,FsMoverWordIndex *,const char *,uint32_t sources);
int FsMover_AddIndexedNameRule(FsMoverPolicy *,FsMoverWordIndex *,const char *,
    uint32_t sources,uint32_t tag);
int FsMover_WordSection(const char *,uint32_t *tag);
int FsMover_WordHeader(const char *);
int FsMover_ValidateWords(const FsMoverPolicy *);
int FsMover_CheckRules(const FsMoverPolicy *,uint16_t species,const FsMoverDetails *);
/* Native result code, or a negative storage/memory error. */
/* 返回原生姓名结果码，或负数内存错误。 */
int FsMover_NameResult(FsMoverPolicy *,const FsMoverDetails *,uint32_t consoleRegion);
int FsMover_HasHan(const uint16_t *name,uint32_t units);

#endif
