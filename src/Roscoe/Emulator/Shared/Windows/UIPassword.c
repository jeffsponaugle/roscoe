#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIPassword.h"

static const SControlToStringID sg_sChangePasswordControlToStringMap[] =
{
	// Labels
	{IDC_STATIC_CONFIRM_PASSWORD,			ESTRING_CONFIRM_PASSWORD},
	{IDC_STATIC_NEW_PASSWORD,				ESTRING_NEW_PASSWORD},
	{IDC_STATIC_OLD_PASSWORD,				ESTRING_OLD_PASSWORD},
	{IDC_STATIC_CHANGE_LOGIN_PASSWORD,		ESTRING_CHANGE_PASSWORD},

	// Buttons
	{IDC_BUTTON_SAVE_PASSWORD,				ESTRING_SAVE},
	{IDCANCEL,								ESTRING_CANCEL},

	// Terminator
	{WINDOW_ID_TERMINATOR}
};

static void InitDialog(HWND eDialogHwnd)
{
	EStatus eStatus;

	UIObjDialogInit(eDialogHwnd, 
					0, 
					0, 
					ESTRING_TITLE_CHANGE_PASSWORD);

	UIObjDialogCenter(eDialogHwnd, GetParent(eDialogHwnd));

	SendMessage(eDialogHwnd, 
				WM_NEXTDLGCTL, 
				(WPARAM) GetDlgItem(eDialogHwnd, IDC_EDIT_OLD_PASSWORD), 
				true);

	eStatus = UIDialogFieldsLocalize(eDialogHwnd,
									 sg_sChangePasswordControlToStringMap);
	BASSERT(ESTATUS_OK == eStatus);
}

static void HandleSave(HWND eHwnd)
{
	EStatus eStatus;
	SEditControlErr sResult;
	WCHAR eString[WINDOWS_WCHAR_MAX];
	char eOldConverted[WINDOWS_WCHAR_MAX * 4];
	char eNewConverted[WINDOWS_WCHAR_MAX * 4];
	char eConfirmationConverted[WINDOWS_WCHAR_MAX * 4];

	UIErrorEditCtrlInitSuccess(&sResult);

	// Grab and convert the old password value
	GetDlgItemText(eHwnd, IDC_EDIT_OLD_PASSWORD, eString, ARRAYSIZE(eString));
	
	UIWCHARToUTF8(eOldConverted, 
				  ARRAYSIZE(eOldConverted),
				  eString);

	// Grab and convert the new password value
	GetDlgItemText(eHwnd, IDC_EDIT_NEW_PASSWORD, eString, ARRAYSIZE(eString));
	
	UIWCHARToUTF8(eNewConverted, 
				  ARRAYSIZE(eNewConverted),
				  eString);

	// Grab and convert the confirmation password value
	GetDlgItemText(eHwnd, IDC_EDIT_CONFIRM_PASSWORD, eString, ARRAYSIZE(eString));
	
	UIWCHARToUTF8(eConfirmationConverted, 
				  ARRAYSIZE(eConfirmationConverted),
				  eString);

	// Send them to the server
	eStatus = UserSetPassword(g_eGlobalConnectionHandle, 
							  eOldConverted,
							  eNewConverted,
							  eConfirmationConverted);
		
	SYSLOG_SERVER_RESULT("UserSetPassword", 
						 eStatus);

	HANDLE_SERVER_RESULT_GOTO(eStatus, 
							  &sResult, 
							  CleanUp);

CleanUp:

	if( UIErrorEditCtrlSuccess(&sResult) )
	{
		WCHAR eSuccessMessage[WINDOWS_MBOX_MAX];
		WCHAR eTitle[WINDOWS_WCHAR_MAX];

		ResourceGetString(ESTRING_PASSWORD_CHANGE_SUCCESS, 
						  eSuccessMessage, 
						  ARRAYSIZE(eSuccessMessage));

		ResourceGetString(ESTRING_APP_TITLE, 
						  eTitle, 
						  ARRAYSIZE(eTitle));
		
		MessageBox(eHwnd, 
				   eSuccessMessage, 
				   eTitle, 
				   MB_OK);

		SendMessage(eHwnd, 
					WM_CLOSE, 
					0, 
					0);
	}
	else
	{
		UIErrorHandle(eHwnd, &sResult);
	}
}

static INT_PTR CALLBACK DlgProc(HWND eDialogHwnd, 
								UINT uMsg, 
								WPARAM wParam, 
								LPARAM lParam)
{
	if( WM_INITDIALOG == uMsg )
	{
		InitDialog(eDialogHwnd);
	}
	else if( WM_CLOSE == uMsg )
	{
		EndDialog(eDialogHwnd, 1);
	}
	else if( WM_RETURNTOLOGIN == uMsg )
	{
		SendMessage(eDialogHwnd, 
					WM_CLOSE, 
					0, 
					0);

		PostMessage(GetParent(eDialogHwnd), 
					WM_RETURNTOLOGIN, 
					0, 
					0);
	}
	else if( WM_COMMAND == uMsg )
	{
		// Clicked save?
		if( (IDC_BUTTON_SAVE_PASSWORD == LOWORD(wParam)) &&
			(BN_CLICKED == HIWORD(wParam)) )
		{
			HandleSave(eDialogHwnd);								
		}
		// Clicked close?
		else if( (IDCANCEL == LOWORD(wParam)) &&
				 (BN_CLICKED == HIWORD(wParam)) )
		{
			SendMessage(eDialogHwnd, 
						WM_CLOSE, 
						0, 
						0);							
		}
	}

	return false;
}

void ChangePasswordDialog(HWND eHwnd)
{
	INT_PTR eResult;
	
	eResult = DialogBox(NULL, 
						MAKEINTRESOURCE(IDD_CHANGE_PASSWORD), 
						eHwnd, 
						DlgProc);

	BASSERT (eResult > 0);
}



