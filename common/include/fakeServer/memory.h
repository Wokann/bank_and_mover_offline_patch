#ifndef COMMON_FAKE_SERVER_MEMORY_H
#define COMMON_FAKE_SERVER_MEMORY_H
#include "types.h"

typedef struct FsMemory {
    void *(*allocate)(uint32_t);
    void (*release)(void *);
} FsMemory;

/* Growth is an allocation strategy, not a size limit. If spare capacity
   cannot be allocated, try the exact requirement before returning failure. */
/* 增长策略不是大小上限；预留余量分配失败时，再尝试实际所需大小。 */
int FsMemory_Reserve(const FsMemory *,void **data,uint32_t *capacity,
    uint32_t needed,uint32_t used,uint32_t width);
/* A small initial stack buffer may grow; it is not a line-length cap. */
/* 初始栈缓冲可以扩容，不作为行长上限。 */
int FsMemory_Text(const FsMemory *,char **text,uint32_t *capacity,
    const char *initial,uint32_t used,uint32_t needed);
#endif
