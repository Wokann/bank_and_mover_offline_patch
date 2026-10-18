#ifndef COMMON_FAKE_SERVER_MOVER_EXPRESSION_H
#define COMMON_FAKE_SERVER_MOVER_EXPRESSION_H

#include "mover_checks.h"

/* Compile configuration text, never C code. Text is a mutable load-only buffer.
   The program and its reusable evaluation workspace have a single owner. */
/* 只解析配置表达式，不编译／执行 C；text 为加载时临时缓冲。
   表达式和复用求值空间由当前策略单独持有。 */
int FsMoverExpression_Parse(const FsStorageIo *,char *text,FsMoverExpression **out);
/* Full condition and its species-only applicability are evaluated together.
   Non-species comparisons are unconstrained in the applicability calculation. */
/* 同时计算完整条件和仅由种族比较确定的适用范围；范围计算不约束其他字段。 */
int FsMoverExpression_Match(FsMoverExpression *,uint16_t,const FsMoverDetails *,int *applies);

#endif
