#ifndef BANK_OFFLINE_PATCH_UNLOCK_MODE_H
#define BANK_OFFLINE_PATCH_UNLOCK_MODE_H

#include "patch_types.h"

/* Return the first server-provided unlock candidate in the same eight-digit
   form used by the stock validation callback. */
/* 按原版验证回调使用的八位格式返回服务器提供的第一个解锁候选值。 */
u32 UnlockMode_TryGetFirstCandidateCode(
    const u32 *candidateBegin, const u32 *candidateEnd, u32 *displayCode);

#endif
