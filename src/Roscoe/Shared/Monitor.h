#ifndef _MONITOR_H_
#define _MONITOR_H_

#include "BIOS/OS.h"
#include "Shared/lex.h"

extern EStatus MonitorStart(void);

typedef struct SMonitorCommands
{
	char *peCommandString;
	char *peDescription;
	EStatus (*Handler)(SLex *psLex,
					   const struct SMonitorCommands *psMonitorCommand,
					   uint32_t *pu32Address);
} SMonitorCommands;

#endif