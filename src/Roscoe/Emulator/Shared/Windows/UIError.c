#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/SharedStrings.h"

// Inits an SEditControlErr structure
static void UIErrorEditCtrl(SEditControlErr *psEditCtrlErr)
{
	BASSERT(psEditCtrlErr);

	ZERO_STRUCT(*psEditCtrlErr);
	psEditCtrlErr->eType = EEDITCTRLERR_NONE;
	psEditCtrlErr->eHResult = S_OK;
	psEditCtrlErr->eAtomID = EATOM_INIT_VALUE;
}

// Returns true if there's no pending error in psEditCtrlErr
bool UIErrorEditCtrlSuccess(SEditControlErr *psEditCtrlErr)
{
	if (psEditCtrlErr->eType != EEDITCTRLERR_NONE)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

// This will do an appropriate conversion from ASCII to whatever codepage is desired
static void UIErrorSetMessageCodepage(SEditControlErr *psEditCtrlErr,
									  int eCodePage,
									  char *peMessage)
{
	BASSERT(psEditCtrlErr);

	if (peMessage)
	{
		MultiByteToWideChar(eCodePage,
							0,
							peMessage,
							-1,
							psEditCtrlErr->eCustomErrorMsg,
							ARRAYSIZE(psEditCtrlErr->eCustomErrorMsg));
	}
	else
	{
		// NULL string - clear the message
		psEditCtrlErr->eCustomErrorMsg[0] = L'\0';
	}
}

// Sets an error message from a UTF8 source to WCHAR
void UIErrorEditCtrlSetMessageUTF8(SEditControlErr *psEditCtrlErr, 
								   char *peMessage)
{
	UIErrorSetMessageCodepage(psEditCtrlErr,
							  CP_UTF8,
							  peMessage);
}

// Sets the error type to an EStatus
void UIErrorEditCtrlSetEStatus(SEditControlErr *psEditCtrlErr, 
							   EStatus eStatus)
{
	UIErrorEditCtrl(psEditCtrlErr);

	if (ESTATUS_OK == eStatus)
	{
		// Do nothing
	}
	else
	{
		// Set the error code
		psEditCtrlErr->eType = EEDITCTRLERR_ESTATUS;
		psEditCtrlErr->eStatus = eStatus;
	}
}

// Sets the error type to an HResult
void UIErrorEditCtrlSetHResult(SEditControlErr *psEditCtrlErr, 
							   HRESULT eHresult)
{
	UIErrorEditCtrl(psEditCtrlErr);
	psEditCtrlErr->eType = EEDITCTRLERR_HRESULT;
	psEditCtrlErr->eHResult = eHresult;
}

// Sets the error type to a field error and records appropriate data
void UIErrorEditCtrlSetSingleField(SEditControlErr *psEditCtrlErr, 
								   EAtomID eAtomID, 
								   EStatus eStatus)
{
	UIErrorEditCtrl(psEditCtrlErr);
	psEditCtrlErr->eType = EEDITCTRLERR_FIELD_ERROR;
	psEditCtrlErr->eStatus = eStatus;
	psEditCtrlErr->eAtomID = eAtomID;
}

// This sets a field error based on an atom ID
void UIErrorSetFieldType(SEditControlErr *psEditCtrlErr, 
						 EAtomID eAtomID)
{
	SFieldError sFieldError;

	ZERO_STRUCT(sFieldError);
	sFieldError.eAtomID = eAtomID;
	sFieldError.eFieldError = psEditCtrlErr->eStatus;
	UIErrorEditCtrlSetFieldError(psEditCtrlErr, 
								 &sFieldError);
}

// Clear out the edit control error structure to a non-err'd state
void UIErrorEditCtrlInitSuccess(SEditControlErr *psEditCtrlErr)
{
	UIErrorEditCtrl(psEditCtrlErr);
}

// Returns true if we have a Window E_ABORT HResult
bool UIErrorAborted(SEditControlErr *psEditCtrlErr)
{
	if ((EEDITCTRLERR_HRESULT == psEditCtrlErr->eType) &&
		(E_ABORT == psEditCtrlErr->eHResult))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// This will set an error message and kick the program back to the login screen
// so that error is displayed
void UIErrorForceLogin(HWND eHWND, 
					   WCHAR *peMessage)
{
	if (NULL == peMessage)
	{
		g_eUserMessageToDisplay[0] = L'\0';
		g_bUserMessageDisplay = false;
	}
	else
	{
		wcsncpy_s(g_eUserMessageToDisplay, ARRAYSIZE(g_eUserMessageToDisplay), peMessage, _TRUNCATE);
		g_bUserMessageDisplay = true;
	}

	PostMessage(eHWND, 
				WM_RETURNTOLOGIN, 
				0, 
				0);
}

// Returns true if the EStatus error is one that should force a re-login 
bool UIErrorLoginCheck(SEditControlErr *psEditCtrlErr)
{
	if (EEDITCTRLERR_ESTATUS == psEditCtrlErr->eType)
	{
		if ((ESTATUS_SESSION_ID_INVALID == psEditCtrlErr->eStatus) ||
			(ESTATUS_USER_PERMISSIONS_FAULT == psEditCtrlErr->eStatus))
		{
			return(true);
		}
		else
		{
			return(false);
		}
	}

	return(false);
}

// This routine is the central handlers for all system errors and will convert
// them from whatever - EStatus, HRESULT, etc... into a WCHAR
WCHAR *UIErrorEditCtrlGetMessageWCHAR(SEditControlErr *psEditCtrlErr, 
									  WCHAR *peMessage, 
									  uint32_t u32MessageLength)
{
	// Make sure expected pointers are non-NULL
	BASSERT(psEditCtrlErr);
	BASSERT(peMessage);

	// NULL the target string
	*peMessage = L'\0';

	// Custom message? Use it. It has priority.
	if (psEditCtrlErr->eCustomErrorMsg[0])
	{
		wcsncpy_s(peMessage, 
				  u32MessageLength, 
				  psEditCtrlErr->eCustomErrorMsg, 
				  _TRUNCATE);
	}
	else
	if (EEDITCTRLERR_NONE == psEditCtrlErr->eType)
	{
		wcsncpy_s(peMessage,
				  u32MessageLength,
				  L"Ok",
				  _TRUNCATE);
	}
	else
	if (EEDITCTRLERR_ESTATUS == psEditCtrlErr->eType)
	{
		// Basic EStatus
		MultiByteToWideChar(CP_UTF8,
							0,
							GetErrorText(psEditCtrlErr->eStatus),
							-1,
							peMessage,
							u32MessageLength);
	}
	else
	if (EEDITCTRLERR_FIELD_ERROR == psEditCtrlErr->eType)
	{
		SAtomToTextInfo *psAtom = NULL;
		char eOutTextName[WINDOWS_WCHAR_MAX];

		// Field error from the server
		psAtom = AtomToTextGet(psEditCtrlErr->eAtomID);

		if (psAtom)
		{
			snprintf(eOutTextName, sizeof(eOutTextName) - 1, "%s - %s", psAtom->peAtomText, GetErrorText(psEditCtrlErr->eStatus));
		}
		else
		{
			snprintf(eOutTextName, sizeof(eOutTextName) - 1, "Unknown Atom ID 0x%.8x - %s", psEditCtrlErr->eAtomID, GetErrorText(psEditCtrlErr->eStatus));
		}

		MultiByteToWideChar(CP_UTF8,
							0,
							eOutTextName,
							-1,
							peMessage,
							u32MessageLength);
	}
	else
	if (EEDITCTRLERR_HRESULT == psEditCtrlErr->eType)
	{
		DWORD u32Result;

		// It's an HRESULT
		u32Result = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 
								  NULL, 
								  psEditCtrlErr->eHResult, 
								  0, 
								  peMessage, 
								  u32MessageLength, 
								  NULL);

		if (0 == u32Result)
		{
			// Poop out an "unknown error" message
			ResourceGetString(ESTRING_ERROR_UNKNOWN_ERROR, 
							  peMessage, 
							  u32MessageLength);
		}
	}

	return(peMessage);
}

// Error message box
void UIErrorMessageBox(HWND eHWND,
					   WCHAR *peMessageBox)
{
	WCHAR eProgram[WINDOWS_MBOX_MAX];

	// It's just a message box
	ResourceGetString(ESTRING_APP_TITLE, 
					  eProgram, 
					  ARRAYSIZE(eProgram));

	// Display a message box
	MessageBox(eHWND, 
			   peMessageBox, 
			   eProgram, 
			   MB_ICONERROR | MB_OK);
}

// Informational message box
void UIErrorMessageBoxInfo(HWND eHWND,
						   WCHAR *peMessageBox)
{
	WCHAR eProgram[WINDOWS_MBOX_MAX];

	// It's just a message box
	ResourceGetString(ESTRING_APP_TITLE, 
					  eProgram, 
					  ARRAYSIZE(eProgram));

	// Display a message box
	MessageBox(eHWND, 
			   peMessageBox, 
			   eProgram, 
			   MB_OK);
}

// Resource ID based message box
void UIErrorMessageBoxResource(HWND eHWND,
							   EStringID eStringID,
							   uint32_t u32Flags)
{
	WCHAR eProgram[WINDOWS_MBOX_MAX];
	WCHAR eMessage[WINDOWS_MBOX_MAX];

	// Get the app's title
	ResourceGetString(ESTRING_APP_TITLE, 
					  eProgram, 
					  ARRAYSIZE(eProgram));

	// Now get the ID
	ResourceGetString(eStringID, 
					  eMessage, 
					  ARRAYSIZE(eMessage));

	// Display a message box
	MessageBox(eHWND, 
			   eMessage, 
			   eProgram, 
			   MB_OK | u32Flags);
}

// This will concatenate field errors
void UIErrorEditCtrlSetFieldError(SEditControlErr *psEditCtrlErr, 
								  SFieldError *psFieldError)
{
	BASSERT(psEditCtrlErr);
	BASSERT(psFieldError);

	// Go set the field error state
	UIErrorEditCtrlSetSingleField(psEditCtrlErr,
								  psFieldError->eAtomID,
								  psFieldError->eFieldError);

	// If we have more than one field error, then we'll have to dump more than one
	if (psFieldError->psNextLink)
	{
		WCHAR *peAtoms = psEditCtrlErr->eCustomErrorMsg;

		// Clear the message

		*peAtoms = L'\0';

		while (psFieldError)
		{
			SAtomToTextInfo *psAtom = NULL;
			char eOutTextName[WINDOWS_WCHAR_MAX];

			// Field error from the server
			psAtom = AtomToTextGet(psFieldError->eAtomID);

			if (psAtom)
			{
				snprintf(eOutTextName, sizeof(eOutTextName) - 1, "%s - %s\n", psAtom->peAtomText, GetErrorText(psFieldError->eFieldError));
			}
			else
			{
				snprintf(eOutTextName, sizeof(eOutTextName) - 1, "Unknown Atom ID 0x%.8x - %s\n", psFieldError->eAtomID, GetErrorText(psFieldError->eFieldError));
			}

			MultiByteToWideChar(CP_UTF8,
								0,
								eOutTextName,
								-1,
								peAtoms,
								ARRAYSIZE(psEditCtrlErr->eCustomErrorMsg));

			// Move to the end for concatenation
			peAtoms += wcslen(peAtoms);
			psFieldError = psFieldError->psNextLink;
		}
	}
}

// This handles the result of an SEditControlErr response, and will force the user back
// to a login screen or just a message box with an error. If the error is handled, true is returned.
bool UIErrorHandle(HWND eHWND, 
				   SEditControlErr *psEditCtrlErr)
{
	WCHAR eError[WINDOWS_MBOX_MAX];

	// If there's no error, there's no error. Just return
	if (UIErrorEditCtrlSuccess(psEditCtrlErr))
	{
		return(false);
	}

	// Convert whatever error there is to a WCHAR
	(void *) UIErrorEditCtrlGetMessageWCHAR(psEditCtrlErr, 
											eError, 
											ARRAYSIZE(eError));
	
	// Log the fact that we're about to kick things back to the login screen
	SyslogWCHAR(L"Failure: %s\n", eError);

	if ((psEditCtrlErr->eType != EEDITCTRLERR_ESTATUS) ||
		(false == UIErrorLoginCheck(psEditCtrlErr)))
	{
		UIErrorMessageBox(eHWND,
						  eError);
		return(false);
	}
	else
	{
		SyslogFunc("Returning to login screen\n");

		UIErrorForceLogin(eHWND, 
						  eError);
		
		return(true);
	}
}

// This copies a WCHAR message into the edit control error structure's WCHAR custom message
void UIErrorEditCtrlSetMessageWCHAR(SEditControlErr *psEditCtrlErr, 
									WCHAR *peMessage)
{
	wcsncpy_s(psEditCtrlErr->eCustomErrorMsg, 
			  ARRAYSIZE(psEditCtrlErr->eCustomErrorMsg), 
			  peMessage, 
			  _TRUNCATE);
}


// This will log an error and post a message to force the UI to go back to the login prompt
void UIErrorHandleLogout(HWND eHWND, 
						 SEditControlErr *psEditCtrlErr)
{
	if (false == UIErrorEditCtrlSuccess(psEditCtrlErr))
	{
		WCHAR eMessage[WINDOWS_WCHAR_MAX];

		// Turn the error into a WCHAR array
		UIErrorEditCtrlGetMessageWCHAR(psEditCtrlErr, 
									   eMessage, 
									   ARRAYSIZE(eMessage));
		
		SyslogWCHAR(L"Returning to login - failure: %s\n", eMessage);

		// Now force the system to go back to the login prompt
		UIErrorForceLogin(eHWND, 
						  eMessage);
	}
}

// Returns true if this is a field error
bool UIErrorFieldError(SEditControlErr *psEditCtrlErr)
{
	if (EEDITCTRLERR_FIELD_ERROR == psEditCtrlErr->eType)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// This sets an error message in the SEditControlErr structure by Windows ID
void UIErrorEditCtrlSetMessageResource(SEditControlErr *psEditCtrlErr, 
									   EStringID eStringID)
{
	WCHAR eMessage[WINDOWS_WCHAR_MAX];

	ResourceGetString(eStringID, 
					  eMessage, 
					  ARRAYSIZE(eMessage));

	UIErrorEditCtrlSetMessageWCHAR(psEditCtrlErr, 
								   eMessage);
}

// IF we have a pending error that forces us to the login page, deposit a message
void UIErrorForceLoginBackground(HWND eHWND, 
								 EStatus eStatus)
{
	SEditControlErr sErr;

	UIErrorEditCtrl(&sErr);
	sErr.eType = EEDITCTRLERR_ESTATUS;
	sErr.eStatus = eStatus;

	// IF this isn't a forced login, then don't do it
	if (false == UIErrorLoginCheck(&sErr))
	{
		return;
	}


	// Dump the error into the global login error
	MultiByteToWideChar(CP_UTF8,
						0,
						GetErrorText(sErr.eStatus),
						-1,
						g_eUserMessageToDisplay,
						ARRAYSIZE(g_eUserMessageToDisplay));

	g_bUserMessageDisplay = true;

	// Async logout message
	PostMessage(eHWND, 
				WM_RETURNTOLOGIN, 
				0, 
				0);
}
