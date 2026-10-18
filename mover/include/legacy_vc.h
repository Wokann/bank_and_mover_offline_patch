#ifndef MOVER_LEGACY_VC_H
#define MOVER_LEGACY_VC_H
#include "server_validation.h"
void *MoverLegacy_Create(void *,u32,void *settings,u32,u32 dvs);
void MoverLegacy_SetGender(void *,u32);
u32 MoverLegacy_Decode(void *,void *,const u8 *,u32,u32,u32,u32,u32);
u16 MoverLegacy_Character(u32,u32);
void *MoverLegacy_OriginalCreate(void *,u32,void *,u32) __attribute__((long_call));
void MoverLegacy_OriginalSetGender(void *,u32) __attribute__((long_call));
u32 MoverLegacy_OriginalDecode(void *,void *,const u8 *,u32,u32,u32,u32,u32) __attribute__((long_call));
void MoverLegacy_AssignText(void *,const u16 *) __attribute__((long_call));
void MoverLegacy_NormalizeKana(void *,void *) __attribute__((long_call));
#endif
