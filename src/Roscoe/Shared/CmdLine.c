#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "Shared/Shared.h"
#include "Shared/CmdLine.h"
#include "Platform/Platform.h"

// Linked list of options and their values
typedef struct SCmdLineOptionList
{
	char *peOption;			// Textual option
	UINT32 u32OptionLen;	// Length of option text

	char *peValue;			// And its value (if appropriate)
	UINT32 u32ValueLen;		// Length of value text

	const SCmdLineOption *psCmdLineOptionDef;
	struct SCmdLineOptionList *psNextLink;
} SCmdLineOptionList;

// Linked list of user provided options
static SCmdLineOptionList *sg_psOptionList;

// Current command line options
static const SCmdLineOption *sg_psCmdLineRef;

// Find a cmd line option by name in the master command list
static const SCmdLineOption *CmdLineFindOption(char *peOptionText,
											   const SCmdLineOption *psCmdLineOptions)
{
	while (psCmdLineOptions->peCmdOption)
	{
		if (Sharedstrcasecmp(psCmdLineOptions->peCmdOption,
							 peOptionText) == 0)
		{
			return(psCmdLineOptions);
		}

		++psCmdLineOptions;
	}

	// Not found
	return(NULL);
}

// Find a provided command line option in the list
static SCmdLineOptionList *CmdLineFindOptionInList(char *peOptionName,
												   SCmdLineOptionList **ppsPriorOptionLink)
{
	SCmdLineOptionList *psOptionList = sg_psOptionList;

	if (ppsPriorOptionLink)
	{
		*ppsPriorOptionLink = NULL;
	}

	while (psOptionList)
	{
		if (Sharedstrcasecmp(psOptionList->peOption,
							 peOptionName) == 0)
		{
			break;
		}

		if (ppsPriorOptionLink)
		{
			*ppsPriorOptionLink = psOptionList;
		}

		psOptionList = psOptionList->psNextLink;
	}

	return(psOptionList);
}

// Process the command line - argc/argv 
static EStatus CmdLineProcess(int argc,
							  char **argv,
							  const SCmdLineOption *psCmdLineOptionHead,
							  char *peProgramName)
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32Loop = 0;
	SCmdLineOptionList *psList = NULL;
	const SCmdLineOption *psCmdLineOptions = psCmdLineOptionHead;
	char eString[150];

	sg_psCmdLineRef = psCmdLineOptionHead;

	// Loop through all provided options and find their definition peer
	while (u32Loop < (UINT32) argc)
	{
		// Allocate a list item
		if (psList)
		{
			// Not the first of the list
			MEMALLOC(psList->psNextLink, sizeof(*psList->psNextLink));
			psList = psList->psNextLink;
		}
		else
		{
			// First of the list
			MEMALLOC(psList, sizeof(*psList));
			sg_psOptionList = psList;
		}

		psList->psCmdLineOptionDef = CmdLineFindOption(argv[u32Loop],
													   psCmdLineOptions);
		psList->peOption = strdupHeap(argv[u32Loop]);
		if (NULL == psList->peOption)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}

		if (NULL == psList->psCmdLineOptionDef)
		{
			snprintf(eString, sizeof(eString) - 1, "Unknown command line option '%s'", argv[u32Loop]);
			PlatformCriticalError(eString);
			return(ESTATUS_UNKNOWN_COMMAND);
		}

		psList->u32OptionLen = (UINT32) strlen(psList->peOption);

		// See if this requires a value. If so, make sure we have one
		if (psList->psCmdLineOptionDef->bCmdNeedsValue)
		{
			++u32Loop;
			if (u32Loop >= (UINT32) argc)
			{
				snprintf(eString, sizeof(eString) - 1, "Command line option '%s' requires a value", argv[u32Loop]);
				PlatformCriticalError(eString);
				return(ESTATUS_UNKNOWN_COMMAND);
			}

			// We have an option!
			psList->peValue = strdupHeap(argv[u32Loop]);
			if (NULL == psList->peValue)
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				goto errorExit;
			}

			psList->u32ValueLen = (UINT32) strlen(psList->peValue);
		}

		u32Loop++;
	}

	// Now run through the list and see if there are an options that are
	// required that we don't have in our list. Also make sure any option
	// that requires a value actually has one
	psCmdLineOptions = psCmdLineOptionHead;

	while (psCmdLineOptions->peCmdOption)
	{
		if (psCmdLineOptions->bCmdRequired)
		{
			// Let's see if this option is present. If not, complain.
			if (FALSE == CmdLineOption((char *) psCmdLineOptions->peCmdOption))
			{
				snprintf(eString, sizeof(eString) - 1, "Command line option '%s' missing", psCmdLineOptions->peCmdOption);
				PlatformCriticalError(eString);
				return(ESTATUS_UNKNOWN_COMMAND);
			}
		}

		psCmdLineOptions++;
	}

errorExit:
	return(eStatus);
}

// Single textual command line init
EStatus CmdLineInit(char *peCmdLine,
					const SCmdLineOption *psCmdLineOptions,
					char *peProgramName)
{
	EStatus eStatus;
	int argc = 0;
	char **argv = NULL;

#ifdef _WIN32
	UINT32 u32Loop;
	LPWSTR *argvW = NULL;

	sg_psCmdLineRef = psCmdLineOptions;

	// Convert Windows command line to WCHAR array
	argvW = CommandLineToArgvW(GetCommandLineW(),
							   &argc);
	BASSERT(argvW);

	// If we don't have any command line options, then just bail out
	if (argc < 2)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// ASCII version - Don't need the first
	argv = (char **) MemAlloc(sizeof(*argv) * (argc - 1));
	BASSERT(argv);

	// Copy all the WCHAR options to ASCII
	for (u32Loop = 1; u32Loop < (UINT32) argc; u32Loop++)
	{
		// Allocate some space for this string
		argv[u32Loop - 1] = MemAlloc(wcslen(argvW[u32Loop]) + 1);
		BASSERT(argv[u32Loop - 1]);
		wcstombs(argv[u32Loop - 1], argvW[u32Loop], wcslen(argvW[u32Loop]));
	}
#else
	BASSERT(0);
#endif

	eStatus = CmdLineProcess(argc - 1,
							 argv,
							 psCmdLineOptions,
							 peProgramName);

#ifdef _WIN32
	for (u32Loop = 0; u32Loop < (UINT32) (argc - 1); u32Loop++)
	{
		SafeMemFree(argv[u32Loop]);
	}

	SafeMemFree(argv);
#else
	BASSERT(0);
#endif

/*	{
		char *peLine;

		peLine = CmdLineGet();
		DebugOut("%s: Line='%s'\n", __FUNCTION__, peLine);
		SafeMemFree(peLine);

		CmdLineOptionRemove("-test");

		peLine = CmdLineGet();
		DebugOut("%s: Line='%s'\n", __FUNCTION__, peLine);
		SafeMemFree(peLine);
	} */

errorExit:
	return(eStatus);
}

// Argc/argv init
EStatus CmdLineInitArgcArgv(int argc,
							char **argv,
							const SCmdLineOption *psCmdLineOptions,
							char *peProgramName)
{
	// Skip past the program name
	return(CmdLineProcess(argc - 1,
						  argv + 1,
						  psCmdLineOptions,
						  peProgramName));
}

char *CmdLineOptionValue(char *peCmdLineOption)
{
	SCmdLineOptionList *psList;

	psList = CmdLineFindOptionInList(peCmdLineOption,
									 NULL);
	if (psList)
	{
		return(psList->peValue);
	}
	else
	{
		return(NULL);
	}
}

char *CmdLineGet(void)
{
	UINT32 u32Length = 0;
	SCmdLineOptionList *psOptionList = sg_psOptionList;
	char *peLine = NULL;

	while (psOptionList)
	{
		u32Length += psOptionList->u32OptionLen + 1;
		u32Length += psOptionList->u32ValueLen + 1;
		psOptionList = psOptionList->psNextLink;
	}

	peLine = MemAlloc(u32Length + 1);
	if (NULL == peLine)
	{
		goto errorExit;
	}

	psOptionList = sg_psOptionList;
	while (psOptionList)
	{
		strcat(peLine, psOptionList->peOption);
		if (psOptionList->peValue)
		{
			strcat(peLine, " ");
			strcat(peLine, psOptionList->peValue);
		}

		// Need to add another space if there are options in there
		if (psOptionList->psNextLink)
		{
			strcat(peLine, " ");
		}

		psOptionList = psOptionList->psNextLink;
	}

errorExit:
	return(peLine);
}

BOOL CmdLineOption(char *peCmdLineOption)
{
	if (CmdLineFindOptionInList(peCmdLineOption,
								NULL))
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}

void CmdLineDumpOptions(const SCmdLineOption *psOptions)
{
	DebugOut("Command line options:\n");

	while (psOptions->peCmdOption)
	{
		DebugOut("  %-12s %s\n", psOptions->peCmdOption, psOptions->peHelpText);
		++psOptions;
	}
}


void CmdLineOptionAdd(char *peCmdLineOptionName,
					  char *peCmdLineOptionValue,
					  BOOL bCheckOption)
{
	SCmdLineOptionList *psOptionList = sg_psOptionList;
	char eString[150];

	while ((psOptionList) &&
		   (psOptionList->psNextLink))
	{
		psOptionList = psOptionList->psNextLink;
	}

	if (NULL == psOptionList)
	{
		psOptionList = MemAlloc(sizeof(*psOptionList));
		if (NULL == psOptionList)
		{
			return;
		}

		sg_psOptionList = psOptionList;
	}
	else
	{
		psOptionList->psNextLink = MemAlloc(sizeof(*psOptionList->psNextLink));
		if (NULL == psOptionList)
		{
			return;
		}

		psOptionList = psOptionList->psNextLink;
	}

	psOptionList->peOption = strdupHeap(peCmdLineOptionName);
	psOptionList->u32OptionLen = (UINT32) strlen(psOptionList->peOption);

	if (peCmdLineOptionValue)
	{
		psOptionList->peValue = strdupHeap(peCmdLineOptionValue);
		psOptionList->u32ValueLen = (UINT32) strlen(psOptionList->peValue);
	}

	if (bCheckOption)
	{
		psOptionList->psCmdLineOptionDef = CmdLineFindOption(psOptionList->peOption,
															 sg_psCmdLineRef);

		if (NULL == psOptionList->psCmdLineOptionDef)
		{
			snprintf(eString, sizeof(eString) - 1, "Unknown command line option '%s'", psOptionList->peValue);
			PlatformCriticalError(eString);
		}

		if (psOptionList->psCmdLineOptionDef->bCmdNeedsValue)
		{
			if (NULL == peCmdLineOptionValue)
			{
				snprintf(eString, sizeof(eString) - 1, "Command line option '%s' missing a value", psOptionList->peOption);
				PlatformCriticalError(eString);
			}
		}
		else
		{
			if (peCmdLineOptionValue)
			{
				snprintf(eString, sizeof(eString) - 1, "Command line option '%s' does not take a value", psOptionList->peOption);
				PlatformCriticalError(eString);
			}
		}
	}
}

void CmdLineOptionRemove(char *peCmdLineOptionName)
{
	SCmdLineOptionList *psPrior = NULL;
	SCmdLineOptionList *psCurrent = NULL;

	psCurrent = CmdLineFindOptionInList(peCmdLineOptionName,
										&psPrior);
	if (NULL == psCurrent)
	{
		// Option doesn't exist
		return;
	}

	SafeMemFree(psCurrent->peOption);
	SafeMemFree(psCurrent->peValue);

	if (psPrior)
	{
		BASSERT(psPrior->psNextLink == psCurrent);
		psPrior->psNextLink = psCurrent->psNextLink;
	}
	else
	{
		BASSERT(psCurrent == sg_psOptionList);
		sg_psOptionList = psCurrent->psNextLink;
	}

	MemFree(psCurrent);
}

