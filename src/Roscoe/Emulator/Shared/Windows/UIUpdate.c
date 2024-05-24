#include <stdio.h>
#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UIUpdate.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIProgress.h"
#include "Shared/SharedUpdate.h"

static UIUpdateShutdownCallback sg_pfShutdownCallback = NULL;

void UIUpdateRegisterShutdownCallback( UIUpdateShutdownCallback pfCallback )
{
	sg_pfShutdownCallback = pfCallback;
}

typedef struct
{
	SSWPackage* psUpdateApp;
	SSWPackage* psThisUpdate;
	HWND eProgressDialog;
} SUpdateInstructions;

static DWORD WINAPI UIUpdateThread( LPVOID pvParam )
{
	SUpdateInstructions* psInstructions = (SUpdateInstructions*) pvParam;
	EStatus eStatus;
	SEditControlErr sUIStatus;

	BASSERT( psInstructions );

	UIErrorEditCtrlInitSuccess( &sUIStatus );

	// Get the update binaries from the server
	eStatus = PackageUpdateDownload( g_eGlobalConnectionHandle,
									 psInstructions->psUpdateApp,
									 psInstructions->psThisUpdate,
									 NULL,
									 UIProgressUpdate );

	SYSLOG_SERVER_RESULT( "PackageUpdateDownload", eStatus );

	if( eStatus != ESTATUS_OK )
	{
		UIErrorEditCtrlSetEStatus( &sUIStatus, eStatus );
		UIProgressClose();
		goto errorExit;
	}

	// If attempting to update this app, must attempt to log out the user and then allow exit for update
	if( psInstructions->psThisUpdate )
	{
		UIUpdateShutdownCallback pfCallback = NULL;

		// Ignore the result, nothing to do if it fails
		(void) UserLogout( g_eGlobalConnectionHandle,
						   &g_psLoggedInUser );

		// Call the shutdown callback if one has been assigned
		pfCallback = sg_pfShutdownCallback;
		if( pfCallback )
		{
			pfCallback();
		}

		// Run the updater on the new binary and exit
		PackageUpdateExecute( psInstructions->psThisUpdate );
	}
	else
	{
		// If there's no psThisUpdate, then this implies we're just doing a BundleUpdate update, so we 
		// need to explicitly close the progress dialog since there won't be an app exit.
		UIProgressClose();
	}

errorExit:

	UIErrorHandle( psInstructions->eProgressDialog, &sUIStatus );

	return(0);
}

// Textual representation of EPackageState (must always match order of enum!)
static const char* sg_peUpdateStates[] =
{
	"No update available",				// EPKG_NONE
	"No update needed",					// EPKG_SAME
	"Update available",					// EPKG_READY
	"Forced update available",			// EPKG_FORCED
};

// Get the latest version info from the server and decide whether or not to update
void UIUpdateCheckForUpdate( HWND eHWND )
{
	SEditControlErr sUIStatus;
	EStatus eStatus;

	// Updater is the small program that performs the update after this app exits
	SSWPackage* psUpdaterVersion = NULL;
	SSWPackage* psThisAppVersion = NULL;
	EPackageState eUpdateDirections;

	bool bDoUpdate = false;

	UIErrorEditCtrlInitSuccess( &sUIStatus );

	// Contact the server for the latest version
	eStatus = PackageUpdateCheck( g_eGlobalConnectionHandle,
								  &psUpdaterVersion,
								  &psThisAppVersion,
								  &eUpdateDirections );
	SYSLOG_SERVER_RESULT( "PackageUpdateCheck", eStatus );

	if ((NULL == psUpdaterVersion) &&
		(NULL == psThisAppVersion))
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	if( eStatus != ESTATUS_OK )
	{
		UIErrorEditCtrlSetEStatus( &sUIStatus, eStatus );
		goto errorExit;
	}

	if( eUpdateDirections >= (sizeof(sg_peUpdateStates)/sizeof(sg_peUpdateStates[0])) )
	{
		SyslogFunc( "Unrecognized update status for this session: %u\n", eUpdateDirections );
	}
	else
	{
		SyslogFunc( "%s for this session\n", sg_peUpdateStates[eUpdateDirections] );
	}

	if( psThisAppVersion )
	{
		if( psThisAppVersion->ePackageState >= (sizeof(sg_peUpdateStates)/sizeof(sg_peUpdateStates[0])) )
		{
			SyslogFunc( "--> Unrecognized update status for this application: %u\n", psThisAppVersion->ePackageState );
		}
		else
		{
			SyslogFunc( "--> %s for this application\n", sg_peUpdateStates[psThisAppVersion->ePackageState] );
		}
	}

	if( psUpdaterVersion )
	{
		if( psUpdaterVersion->ePackageState >= (sizeof(sg_peUpdateStates)/sizeof(sg_peUpdateStates[0])) )
		{
			SyslogFunc( "--> Unrecognized update status for the updater: %u\n", psUpdaterVersion->ePackageState );
		}
		else
		{
			SyslogFunc( "--> %s for the updater\n", sg_peUpdateStates[psUpdaterVersion->ePackageState] );
		}
	}

	// Handle user messaging
	{
		WCHAR eApplicationName[WINDOWS_WCHAR_MAX];
		WCHAR eUpdateName[WINDOWS_WCHAR_MAX];
		WCHAR eFormatStr[WINDOWS_WCHAR_MAX];
		WCHAR eMsgBoxTxt[WINDOWS_WCHAR_MAX];
		WCHAR* peMsgCurrent = &eMsgBoxTxt[0];
		WCHAR eStatement[WINDOWS_WCHAR_MAX];
		SSWPackage* psMsgItem = psThisAppVersion;
		int32_t s32Result;

		eApplicationName[0] = 0;
		ResourceGetString( ESTRING_APP_TITLE, 
						   &eApplicationName[0], 
						   ARRAYSIZE(eApplicationName));

		eFormatStr[0] = 0;
		ResourceGetString( ESTRING_UPDATE_COMPONENT_TEMPLATE, 
						   &eFormatStr[0], 
						   ARRAYSIZE(eFormatStr));

		if( NULL == psMsgItem )
		{
			psMsgItem = psUpdaterVersion;
		}

		// Convert the updated item to a wide string
		if( psMsgItem->peExecutableFilename )
		{
			(void) MultiByteToWideChar( CP_UTF8, 
										0, 
										(LPCSTR)(psMsgItem->peExecutableFilename), 
										-1, 
										&eUpdateName[0], 
										ARRAYSIZE(eUpdateName));
		}
		else
		{
			eUpdateName[0] = 0;
		}

		// Compose a string with the name of the program and the version
		s32Result = _snwprintf_s( &eMsgBoxTxt[0], 
								  sizeof(eMsgBoxTxt)/sizeof(eMsgBoxTxt[0]), 
								  _TRUNCATE, 
								  &eFormatStr[0], 
								  &eUpdateName[0],
								  psMsgItem->u8Major,
								  psMsgItem->u8Minor,
								  psMsgItem->u16BuildNumber );

		if( s32Result > 0 )
		{
			peMsgCurrent += s32Result;
		}
		else
		{
			eMsgBoxTxt[0] = 0;
			(void) wcsncat_s( &eMsgBoxTxt[0], 
							  sizeof(eMsgBoxTxt)/sizeof(eMsgBoxTxt[0]), 
							  L"<unknown>", 
							  _TRUNCATE );
		}

		// Add on punctuation and spacing
		(void) wcsncat_s( &eMsgBoxTxt[0], 
						  sizeof(eMsgBoxTxt)/sizeof(eMsgBoxTxt[0]), 
						  L".  ", 
						  _TRUNCATE );

		// If the update is forced, alert the user to the following update
		if( EPKG_FORCED == eUpdateDirections )
		{
			eStatement[0] = 0;
			ResourceGetString( ESTRING_UPDATE_FORCED, 
							   &eStatement[0], 
							   ARRAYSIZE(eStatement));

			(void) wcsncat_s( &eMsgBoxTxt[0], 
							  sizeof(eMsgBoxTxt)/sizeof(eMsgBoxTxt[0]), 
							  &eStatement[0], 
							  _TRUNCATE );

			// Prompt the user
			(void) MessageBox( eHWND,
							   &eMsgBoxTxt[0],
							   &eApplicationName[0],
							   MB_ICONEXCLAMATION | MB_OK);

			// Regardless of user interaction, perform the update
			bDoUpdate = true;
		}
		// If not forced but still available, ask the user whether or not to update
		else if( EPKG_READY == eUpdateDirections )
		{
			eStatement[0] = 0;
			ResourceGetString( ESTRING_UPDATE_OPTIONAL, 
							   &eStatement[0], 
							   ARRAYSIZE(eStatement));

			(void) wcsncat_s( &eMsgBoxTxt[0], 
							  ARRAYSIZE(eMsgBoxTxt), 
							  &eStatement[0], 
							  _TRUNCATE );

			// Prompt the user
			s32Result = MessageBox( eHWND,
									&eMsgBoxTxt[0],
									&eApplicationName[0],
									MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1 );


			// If the user responded with a YES, do the update
			if( IDYES == s32Result )
			{
				bDoUpdate = true;
			}
			// Else if denied, log a message for debug
			else
			{
				Syslog("User refused update\n");
			}
		}
		else
		{
			// No update, just ignore it
		}
	}

	if( bDoUpdate )
	{
		SUpdateInstructions* psUpdateInstructions;

		psUpdateInstructions = MemAlloc( sizeof(*psUpdateInstructions) );
		if( NULL == psUpdateInstructions )
		{
			Syslog("Failed to allocate instructions for update\n");
			goto errorExit;
		}

		// Create a progress dialog
		psUpdateInstructions->eProgressDialog = UIProgressCreate( eHWND, 
																  ESTRING_UPDATE_VERSION );
		if( NULL == psUpdateInstructions->eProgressDialog )
		{
			Syslog("Failed to create progress dialog: %d\n", errno);
			MemFree( psUpdateInstructions );
			goto errorExit;
		}

		// Assign the updates to do
		psUpdateInstructions->psUpdateApp = psUpdaterVersion;
		psUpdateInstructions->psThisUpdate = psThisAppVersion;

		// Create an update thread
		if( NULL == CreateThread(NULL,
								 0,
								 UIUpdateThread,
								 (LPVOID)psUpdateInstructions,
								 0,
								 NULL) )
		{
			Syslog("Failed to create update thread: %d\n", errno);
			MemFree( psUpdateInstructions );
			goto errorExit;
		}
	}

errorExit:
	// Handle any error result
	UIErrorHandle( eHWND,
				   &sUIStatus );
}

