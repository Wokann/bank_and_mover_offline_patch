#ifndef COMMON_CONSOLE_IDENTITY_H
#define COMMON_CONSOLE_IDENTITY_H
#include "fakeServer/user_profiles.h"
/* Local ACT/FRD account name, falling back to CFG's console profile name.
   Returns 0 only when neither source supplies a valid identity. No login. */
int ConsoleIdentity_Read(FsUserIdentity *);
#endif
