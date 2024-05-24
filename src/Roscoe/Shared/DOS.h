#ifndef _DOS_H_
#define _DOS_H_

#include "Shared/lex.h"

typedef struct SMonitorCommands
{
	char *peCommandString;
	char *peDescription;
	EStatus (*Handler)(SLex *psLex,
					   const struct SMonitorCommands *psMonitorCommand,
					   uint32_t *pu32Address);
} SMonitorCommands;


extern EStatus DOSInit(void);
extern EStatus DOSGetDrivePath(char *peDrivePath, size_t eMaxLength);
extern EStatus DOSMkdir(SLex *psLex,
						const struct SMonitorCommands *psMonitorCommand,
						uint32_t *pu32Address);
extern EStatus DOSRmdir(SLex *psLex,
						const struct SMonitorCommands *psMonitorCommand,
						uint32_t *pu32Address);
extern EStatus DOSChdir(SLex *psLex,
						const struct SMonitorCommands *psMonitorCommand,
						uint32_t *pu32Address);
extern EStatus DOSFormat(SLex *psLex,
						 const struct SMonitorCommands *psMonitorCommand,
						 uint32_t *pu32Address);
extern EStatus DOSDir(SLex *psLex,
					  const struct SMonitorCommands *psMonitorCommand,
					  uint32_t *pu32Address);
extern EStatus DOSDelete(SLex *psLex,
						 const SMonitorCommands *psMonitorCommand,
						 uint32_t *pu32AddressPointer);


#endif
