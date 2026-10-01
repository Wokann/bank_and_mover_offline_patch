#include "unlock_mode.h"

#define UNLOCK_CODE_MODULUS 100000000u
#define UNLOCK_CODE_RECIPROCAL 0x55E63B89u

__attribute__((used,noinline,section(".text.unlock.0_candidate")))
u32 UnlockMode_TryGetFirstCandidateCode(
    const u32 *candidateBegin, const u32 *candidateEnd, u32 *displayCode)
{
    if (candidateBegin == 0 || candidateEnd == 0 || candidateBegin >= candidateEnd ||
        displayCode == 0) {
        return 0;
    }

    /* The stock input callback accepts each returned value modulo 100,000,000.
       Its binary implements the constant division with reciprocal multiply,
       so use the same arithmetic and let the UI add leading zeroes. */
    /* 原版输入回调按模 100,000,000 验证每个返回值。其二进制通过倒数乘法实现
       常数除法，因此这里使用相同算法，前导零仍交给界面补齐。 */
    u32 candidate=*candidateBegin;
    u64 reciprocalProduct=(u64)candidate*UNLOCK_CODE_RECIPROCAL;
    u32 quotient=(u32)(reciprocalProduct>>32)>>25;
    *displayCode=candidate-quotient*UNLOCK_CODE_MODULUS;
    return 1;
}
