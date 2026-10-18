#ifndef COMMON_FAKE_SERVER_WORD_PATTERN_H
#define COMMON_FAKE_SERVER_WORD_PATTERN_H
#include "memory.h"

/* One input UTF-16 unit can expand to two lowercase units (U+0130). */
/* 一个输入 UTF-16 单元最多展开为两个小写单元（U+0130）。 */
enum { FS_WORD_MATCH_UNITS=26 };

/* Patterns, nodes and nesting use actual heap requirements. Only the native
   name/view field is bounded; no exponential backtracking or system regex. */
/* 表达式、节点及嵌套按实际需要分配；只保留原生名字字段边界，不做指数回溯。 */
typedef struct FsWordWorkspace {
    FsMemory memory;
    void *nodes,*frames,*ends,*decoded;
    uint32_t nodeCapacity,frameCapacity,endCapacity,decodedCapacity;
} FsWordWorkspace;
void FsWordPattern_Release(FsWordWorkspace *);
/* FS_OK / FS_INVALID / FS_NO_MEMORY, not a boolean. */
/* 返回有效／非法／内存不足状态，不是布尔值。 */
int FsWordPattern_Valid(FsWordWorkspace *,const uint16_t *pattern,uint32_t length);
/* Outside NGWord markers constrain all alternatives at original space-delimited
   word edges. Inner assertions use the whole-view start/end, not word edges. */
/* 最外侧 NGWord 标记约束所有分支，按原名字空格词边界判断；内部断言只认视图首尾。 */
/* 1 match, 0 no match; negative values are real errors. */
/* 1 命中，0 未命中，负数为真实错误。 */
int FsWordPattern_MatchBoundaries(FsWordWorkspace *,const uint16_t *pattern,uint32_t length,
    const uint16_t *name,uint32_t units,uint32_t begins,uint32_t ends);
#endif
