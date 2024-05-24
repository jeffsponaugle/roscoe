#ifndef _UNIXTOOLS_H_
#define _UNIXTOOLS_H_

extern EStatus ErrnoToEStatus(int s32Errno);
extern EStatus POSIXToEStatus(int s32Result);

#endif