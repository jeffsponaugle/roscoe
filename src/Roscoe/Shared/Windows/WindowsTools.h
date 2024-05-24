#ifndef _WINDOWSTOOLS_H_
#define _WINDOWSTOOLS_H_

extern EStatus ErrnoToEStatus(int s32Errno);
extern EStatus WindowsWSAErrorsToEStatus(int s32WindowsError);
extern EStatus WindowsErrorsToEStatus(int s32WindowsError);

#endif