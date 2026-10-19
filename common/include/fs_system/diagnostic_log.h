#ifndef COMMON_FS_DIAGNOSTIC_LOG_H
#define COMMON_FS_DIAGNOSTIC_LOG_H

#include <stdint.h>

#ifndef FS_LOCAL_IO_TRACE
#define FS_LOCAL_IO_TRACE 1
#endif

/* Shared server.conf [debug] enablelog defaults off and is read once at
   process startup. Disabled logging records nothing and leaves old logs alone.
   No paths, names, tokens or bodies are stored; sink failure does not change results. */
/* 进程启动时读取一次共享 server.conf 的 [debug] enablelog，默认关闭；
   关闭时不记录也不改旧日志。不记录路径、姓名、令牌或正文，失败不改变业务返回值。 */
#if FS_LOCAL_IO_TRACE
void FsDiag_Startup(void) __attribute__((target("arm")));
void FsDiag_Record(const char *stage,int32_t result,uint32_t a,uint32_t b,uint32_t c)
    __attribute__((target("arm")));
void FsDiag_Flush(void) __attribute__((target("arm")));
uint32_t FsDiag_Path(uint32_t type,const void *data,uint32_t size);
void FsDiag_Checkpoint(const char *stage,int32_t result,uint32_t a,uint32_t b,uint32_t c)
    __attribute__((target("arm")));
#else
#define FsDiag_Startup() ((void)0)
#define FsDiag_Record(...) ((void)0)
#define FsDiag_Flush() ((void)0)
#define FsDiag_Path(...) 0u
#define FsDiag_Checkpoint(...) ((void)0)
#endif

#endif
