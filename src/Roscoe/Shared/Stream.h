#ifndef _STREAM_H_
#define _STREAM_H_

extern EStatus StreamSetConsoleSerialInterruptMode(bool bInterruptMode);
extern EStatus StreamFlush(void);
extern void StreamSetAutoCR(bool bAutoCR);
extern EStatus FatFSToEStatus(uint8_t u8FatFSErrorCode);
extern unsigned char FatFSToErrno(uint8_t u8FatFSErrorCode);

#endif
