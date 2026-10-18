#ifndef MOVER_CHINESE_NAMES_H
#define MOVER_CHINESE_NAMES_H
#include "server_validation.h"

/* 1: custom conversion, 0: native-only text, -1: incomplete/unknown sequence.
   1：完成自定义转码；0：无需覆盖原转换；-1：不完整或未知编码。 */
int MoverNames_DecodeVc(const u8 *,u32 bytes,u32 alphabet,u16 out[13]);
MoverVcCandidateView *MoverNames_ConvertGen5(void *,MoverGen5RecordView *);
MoverVcCandidateView *MoverNames_ConvertGen5Checked(void *,MoverGen5RecordView *,u32 decision);
void MoverNames_ConvertVcA(void *,u32,void *,const void *,const u8 *,const u8 *,u32,u32,const void *);
u32 MoverNames_ConvertVcB(void *,u32,void *,const void *,u32,const u8 *,const u8 *,u32,u32);
void MoverNames_ConvertVcC(void *,u32,void *,const void *,const u8 *,const u8 *,u32,u32,const void *);

MoverVcCandidateView *MoverNames_OriginalGen5(void *,MoverGen5RecordView *) __attribute__((long_call));
void MoverNames_OriginalVcA(void *,u32,void *,const void *,const u8 *,const u8 *,u32,u32,const void *) __attribute__((long_call));
u32 MoverNames_OriginalVcB(void *,u32,void *,const void *,u32,const u8 *,const u8 *,u32,u32) __attribute__((long_call));
void MoverNames_OriginalVcC(void *,u32,void *,const void *,const u8 *,const u8 *,u32,u32,const void *) __attribute__((long_call));
#endif
