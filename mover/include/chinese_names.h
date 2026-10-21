#ifndef MOVER_CHINESE_NAMES_H
#define MOVER_CHINESE_NAMES_H
#include "server_validation.h"

/* 1: custom conversion, 0: native-only text, -1: incomplete/unknown sequence.
   1：完成自定义转码；0：无需覆盖原转换；-1：不完整或未知编码。 */
int MoverNames_DecodeVc(const u8 *,u32 bytes,u32 alphabet,u16 out[13]);
int MoverNames_Gen5Default(void *out,const u16 *nickname,u32 decision);
MoverVcCandidateView *MoverNames_ConvertGen5(void *,MoverGen5RecordView *);
MoverVcCandidateView *MoverNames_ConvertGen5Checked(void *,MoverGen5RecordView *,u32 decision);
void MoverNames_SetVcNickname(void *,void *,const u8 *,u32 alphabet);
void MoverNames_SetVcTrainer(void *,void *,const u8 *,u32 alphabet);

extern const u16 MoverGen5_NameCharacters[];
MoverVcCandidateView *MoverNames_OriginalGen5(void *,MoverGen5RecordView *,u32,u32,u32 decision) __attribute__((long_call));
void MoverNames_AssignText(void *,const u16 *) __attribute__((long_call));
void MoverNames_OriginalSetNickname(void *,const void *) __attribute__((long_call));
void MoverNames_OriginalSetTrainer(void *,const void *) __attribute__((long_call));
#endif
