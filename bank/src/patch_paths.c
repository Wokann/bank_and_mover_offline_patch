#include "patch_paths.h"

/* Path storage is compiled as its own object for the verified tail-code area. */
/* 路径数据单独编译成对象，并放入已验证的代码尾部区域。 */
const char emptyPath[1] = {0};
const char directory3ds[] = "/3ds";
const char directoryBank[] = "/3ds/Bank";
const char bankPath[] = "/3ds/Bank/bankdata.bin";
const char tempPath[] = "/3ds/Bank/bankdata.tmp";
const char backupPath[] = "/3ds/Bank/bankdata.bak";
const char brokenBankPath[] = "/3ds/Bank/bankdata.bin.break";
const char brokenBackupPath[] = "/3ds/Bank/bankdata.bak.break";
const char turtleSavePath[] = "/3ds/Bank/sav.bin";
const char turtleTempPath[] = "/3ds/Bank/sav.tmp";
