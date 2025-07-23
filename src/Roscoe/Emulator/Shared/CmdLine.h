#ifndef _CMDLINE_H_
#define _CMDLINE_H_

// Structural definition of command line options
typedef struct SCmdLineOption
{
	const char *peCmdOption;	// Command line option itself
	const char *peHelpText; 	// Command line option's description
	bool bCmdRequired; 	 		// true If it's a required command line option
	bool bCmdNeedsValue;		// true If it needs a value
} SCmdLineOption;

// Single textual command line init
extern EStatus CmdLineInit(char *peCmdLine,
						   const SCmdLineOption *psCmdLineOptions,
						   char *peProgramName);

// Argc/argv init
extern EStatus CmdLineInitArgcArgv(int argc,
								   char **argv,
								   const SCmdLineOption *psCmdLineOptions,
								   char *peProgramName);

extern char *CmdLineOptionValue(char *peCmdLineOption);
extern char *CmdLineGet(void);
extern bool CmdLineOption(char *peCmdLineOption);
extern void CmdLineOptionAdd(char *peCmdLineOptionName,
							 char *peCmdLineOptionValue,
							 bool bCheckOption);
extern void CmdLineOptionRemove(char *peCmdLineOptionName);
extern void CmdLineDumpOptions(const SCmdLineOption *psOptions);

#endif