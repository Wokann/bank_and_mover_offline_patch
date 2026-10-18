#ifndef COMMON_TITLE_USERS_H
#define COMMON_TITLE_USERS_H
#include <stdint.h>
void TitleUsers_Initialize(void *view);
/* Title policy enables Bank Offline/Download and Mover Offline only.
   Poll even on a title-exit frame. Nonzero means keep the title state alive
   until account selection is durably published and its old cache is retired. */
int TitleUsers_Update(void *state,uint32_t mode);
#endif
