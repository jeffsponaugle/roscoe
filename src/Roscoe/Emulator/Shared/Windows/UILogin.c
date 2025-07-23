#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Version.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UILogin.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Network.h"

#define LOGIN_DIALOG_WIDTH			(1000)
#define LOGIN_DIALOG_HEIGHT			(700)

#define XPOS_USERNAME				(616)
#define YPOS_USERNAME				(441)
#define WIDTH_USERNAME				(284)
#define HEIGHT_USERNAME				(37)

#define XPOS_PASSWORD				(XPOS_USERNAME)
#define YPOS_PASSWORD				(486)
#define WIDTH_PASSWORD				(WIDTH_USERNAME)
#define HEIGHT_PASSWORD				(HEIGHT_USERNAME)

#define XPOS_SERVER_GROUP_COMBO		(XPOS_USERNAME)
#define YPOS_SERVER_GROUP_COMBO		(532)
#define WIDTH_SERVER_GROUP_COMBO	(WIDTH_USERNAME)
#define HEIGHT_SERVER_GROUP_COMBO	(HEIGHT_USERNAME)

#define XPOS_LOGIN_BUTTON			(XPOS_USERNAME)
#define YPOS_LOGIN_BUTTON			(585)
#define WIDTH_LOGIN_BUTTON			(WIDTH_USERNAME)
#define HEIGHT_LOGIN_BUTTON			(HEIGHT_USERNAME)

#define XPOS_TESTMODE				(670)
#define YPOS_TESTMODE				(110)
#define WIDTH_TESTMODE				(130)
#define HEIGHT_TESTMODE				(16)

#define XPOS_LOGIN_MESSAGE			(XPOS_USERNAME)
#define YPOS_LOGIN_MESSAGE			(630)
#define WIDTH_LOGIN_MESSAGE			(WIDTH_USERNAME)
#define HEIGHT_LOGIN_MESSAGE		(32)

#define XPOS_VERSION_OFFSET			(75)
#define YPOS_VERSION_OFFSET			(0)
#define WIDTH_VERSION				(300)
#define HEIGHT_VERSION				(30)

#define XPOS_COPYRIGHT_OFFSET		(510)
#define YPOS_COPYRIGHT_OFFSET		(YPOS_VERSION_OFFSET)
#define WIDTH_COPYRIGHT				(WIDTH_VERSION)
#define HEIGHT_COPYRIGHT			(HEIGHT_VERSION)

#define VERSION_FONT_WIDTH			(15)
#define LOGIN_FONT_POINTS			(18)

static uint64_t sg_u64PermissionMask;
static HBITMAP sg_hBackgroundImage;
static HFONT sg_hFontVersion;
static HFONT sg_hFontLogin;

typedef struct SLogoLanguage
{
	ELanguage eLanguage;
	const wchar_t *peLogoFile;
} SLogoLanguage;

// List of all logos/language
static const SLogoLanguage sg_LogoLanguage[] =
{
	{ELANG_ENGLISH,					L"\\files\\Images\\LoginEnglish.bmp"},
	{ELANG_SPANISH,					L"\\files\\Images\\LoginSpanish.bmp"},
	{ELANG_MANDARIN_SIMPLIFIED,		L"\\files\\Images\\LoginMandarinSimplified.bmp"},
	{ELANG_MANDARIN_TRADITIONAL,	L"\\files\\Images\\LoginMandarinTraditional.bmp"},

	// Terminator
	{ELANG_MAX}
};

static const SControlToStringID sg_sLoginControlToStringMap[] =
{
	// Labels
	{IDC_STATIC_TESTMODE,			ESTRING_TEST_SERVERS_IN_USE},

	// Buttons
	{IDC_SIGNIN,					ESTRING_SIGN_IN},

	// Terminator
	{WINDOW_ID_TERMINATOR}
};

// Find the file we load for whatever language we're loading up
static const wchar_t *LogoFindByLanguage(ELanguage eLanguage)
{
	const SLogoLanguage *psLogo = sg_LogoLanguage;

	while (psLogo->eLanguage != ELANG_MAX)
	{
		if (psLogo->eLanguage == eLanguage)
		{
			return(psLogo->peLogoFile);
		}

		++psLogo;
	}

	return(NULL);
}

void UILoginRedrawBackground( HWND hDialog )
{
	HDC hDraw;
	PAINTSTRUCT sConfig;
	HDC hMemory;
	HBITMAP hDeleteMe;

	// Prepare to paint
	hDraw = BeginPaint( hDialog,
						&sConfig );

	hMemory = CreateCompatibleDC( hDraw );

	// Select the statically saved source image
	hDeleteMe = (HBITMAP) SelectObject( hMemory, 
										sg_hBackgroundImage );

	// Copy the src image from memory into the dialog DC
	BitBlt( hDraw,
			0, 0,
			LOGIN_DIALOG_WIDTH, LOGIN_DIALOG_HEIGHT,
			hMemory,
			0 ,0,
			SRCCOPY );

	// Done with the memory DC, reselect the previously selected object and clean up
	(void) SelectObject( hMemory, 
						 hDeleteMe );
	DeleteDC( hMemory );
	DeleteObject( hDeleteMe );

	// Finalize the paint
	EndPaint( hDialog, 
			  &sConfig );
}

void UILoginRedrawLoginText( HWND hDialog,
							 WCHAR* peMessage )
{
	HWND hText;

	// Set the text
	hText = GetDlgItem( hDialog, IDC_STATIC_LOGINMESSAGE );
	SetWindowText( hText, peMessage );

	// And redraw the window
	InvalidateRect( hDialog,
					NULL, 
					true );

	UILoginRedrawBackground( hDialog );

	UpdateWindow( hDialog );
}

// Clean up statically allocated resources and return the 
//  reason for closure to the parent window
void UILoginCloseDialog( HWND hDialog,
					     ELoginResult eCloseReason )
{
	if( sg_hBackgroundImage )
	{
		DeleteObject( sg_hBackgroundImage );
		sg_hBackgroundImage = NULL;
	}

	if( sg_hFontVersion )
	{
		DeleteObject( sg_hFontVersion );
		sg_hFontVersion = NULL;
	}

	if( sg_hFontLogin )
	{
		DeleteObject( sg_hFontLogin );
		sg_hFontLogin = NULL;
	}

	EndDialog( hDialog, 
			   eCloseReason );
}

// Populate the content of the server group combobox
static void InitializeServerGroupCombo(HWND hDialog)
{
	HWND hCombo;
	SEditControlErr sResult;
	SServerGroup *psGroup;
	wchar_t eGroupName[200];
	EStatus eStatus;
	char *peGroupName = NULL;
	uint32_t u32Index = 0;

	UIErrorEditCtrlInitSuccess(&sResult);

	hCombo = GetDlgItem(hDialog, IDC_COMBO_SERVER_GROUP);
	BASSERT(hCombo);

	if (false == NetworkServerGroupNotSpecifiedOnCommandLine())
	{
		// If they've specified a group, just bail out
		ShowWindow(hCombo,
				   SW_HIDE);
		return;
	}

	psGroup = NetworkGetServerGroupListHead();

	while (psGroup)
	{
		mbstowcs(eGroupName,
				 psGroup->peGroupName,
				 (sizeof(eGroupName) / sizeof(eGroupName[0])) - 1);

		UIObjComboBoxAdd(hCombo,
						 eGroupName,
						 (int32_t) psGroup->u32Index,
						 &sResult);

		UIERR_FAIL_GOTO(&sResult, errorExit);
		psGroup = psGroup->psNextLink;
	}

	// Load up the current group
	eStatus = ServerListGroupRead(&peGroupName);
	if (ESTATUS_OK == eStatus)
	{
		// Got it! Let's attempt to set it
		eStatus = NetworkSetServerGroupName(peGroupName);
		if (ESTATUS_OK == eStatus)
		{
			// All set! Let's figure out which we want to set it to by default
			psGroup = ServerGroupGetByName(NetworkGetServerGroupListHead(),
										   peGroupName);
			if (NULL == psGroup)
			{
				SyslogFunc("Couldn't get server group by name - %s\n", peGroupName);
			}
			else
			{
				u32Index = psGroup->u32Index;
			}
		}
		else
		{
			SyslogFunc("Failed to set server group name to '%s' - defaulting to not selected\n", peGroupName);
		}
	}
	else
	{
		if (CmdLineOption("-group"))
		{
			// Figure out which index this group is
			psGroup = ServerGroupGetByName(NetworkGetServerGroupListHead(),
										   CmdLineOptionValue("-group"));
			if (NULL == psGroup)
			{
				SyslogFunc("Provided -group of '%s' not found in our group list - setting to not selected\n", CmdLineOptionValue("-group"));
			}
			else
			{
				// Set it to whatever the -group option is set to
				u32Index = psGroup->u32Index;

				// Write it out
				eStatus = ServerListGroupWrite(CmdLineOptionValue("-group"));
				if (eStatus != ESTATUS_OK)
				{
					SyslogFunc("Failed to write server list group file - %s\n", GetErrorText(eStatus));
				}

				eStatus = ESTATUS_OK;
			}
		}
		else
		{
			SyslogFunc("No group server file and no -group - defaulting to not selected\n");
		}
	}

	// Set the default
	UIObjComboBoxSelect(hCombo,
						u32Index);

errorExit:
	SafeMemFree(peGroupName);
	UIErrorHandle(hCombo, &sResult);
}

void UILoginDialog( HWND hDialog )
{
	WCHAR eMessage[WINDOWS_WCHAR_MAX];
	WCHAR eFormat[WINDOWS_WCHAR_MAX];
	HWND hItem;
	bool bMoved;
	int32_t s32Option;
	EStatus eStatus;

	UIObjDialogInit( hDialog, 
					 LOGIN_DIALOG_WIDTH, 
					 LOGIN_DIALOG_HEIGHT, 
					 ESTRING_LOGIN_TITLE );

	// Load the background image into memory
	if( NULL == sg_hBackgroundImage )
	{
		WCHAR eAppPath[WINDOWS_WCHAR_MAX];
		WCHAR* peResult;
		WCHAR ePath[WINDOWS_WCHAR_MAX];

		peResult = WindowsGetAppFolder( &eAppPath[0],
										sizeof(eAppPath)/sizeof(eAppPath[0]) );

		if( peResult )
		{
			const wchar_t *peLogoFilename = NULL;
			wcsncpy_s( ePath,
					   sizeof(ePath)/sizeof(ePath[0]),
					   eAppPath,
					   _TRUNCATE );

			peLogoFilename = LogoFindByLanguage(LanguageGetLocal(NULL, 0));
			if (NULL == peLogoFilename)
			{
				Syslog("%s: Logo filename for language enum %u missing - defaulting to English\n", __FUNCTION__, LanguageGetLocal(NULL, 0));
				peLogoFilename = LogoFindByLanguage(ELANG_ENGLISH);
			}

			wcsncat_s( ePath,
					   sizeof(ePath)/sizeof(ePath[0]),
					   peLogoFilename, 
					   _TRUNCATE );

			if( false == PathFileExists(ePath) )
			{
				wcsncpy_s( ePath,
						   sizeof(ePath)/sizeof(ePath[0]),
						   eAppPath,
						   _TRUNCATE );
				wcsncat_s( ePath,
						   sizeof(ePath)/sizeof(ePath[0]),
						   L"\\..\\..", 
						   _TRUNCATE );
				wcsncat_s( ePath,
						   sizeof(ePath)/sizeof(ePath[0]),
						   peLogoFilename, 
						   _TRUNCATE );

				if( false == PathFileExists(ePath) )
				{
					BASSERT_MSG("Logo file missing");
				}
			}

			sg_hBackgroundImage = (HBITMAP) LoadImage( NULL,
													   ePath,
													   IMAGE_BITMAP,
													   0, 0,
													   LR_LOADFROMFILE | LR_DEFAULTSIZE );
			BASSERT( sg_hBackgroundImage );
		}
	}

	// Load fonts into memory
	if( NULL == sg_hFontVersion )
	{
		sg_hFontVersion = CreateFont( VERSION_FONT_WIDTH, 
									  0, 
									  0, 
									  0, 
									  FW_NORMAL, 
									  false, 
									  false, 
									  false, 
									  DEFAULT_CHARSET, 
									  OUT_DEFAULT_PRECIS, 
									  CLIP_DEFAULT_PRECIS, 
									  PROOF_QUALITY, 
									  DEFAULT_PITCH, 
									  NULL );
		BASSERT( sg_hFontVersion );
	}

	if( NULL == sg_hFontLogin )
	{
		int32_t s32Capabilities;
		int32_t s32Height;

		s32Capabilities = GetDeviceCaps( GetDC(NULL), 
										 LOGPIXELSY );

		s32Height = -MulDiv( LOGIN_FONT_POINTS, 
							 s32Capabilities, 
							 72);
	
		sg_hFontLogin = CreateFont( s32Height, 
									0, 
									0, 
									0, 
									FW_THIN, 
									false, 
									false, 
									false, 
									DEFAULT_CHARSET, 
									OUT_DEFAULT_PRECIS, 
									CLIP_DEFAULT_PRECIS, 
									PROOF_QUALITY, 
									DEFAULT_PITCH, 
									L"Arial" );
		BASSERT( sg_hFontLogin );
	}

	// Set copyright
	ResourceGetString( ESTRING_ABOUT_COPYRIGHT,
					   &eMessage[0],
					   ARRAYSIZE(eMessage) );

	hItem = GetDlgItem( hDialog, IDC_STATIC_COPYRIGHT );

	SetWindowText( hItem, eMessage );

	// Set version
	ResourceGetString( ESTRING_ABOUT_VERSION_TEMPLATE,
					   &eFormat[0],
					   ARRAYSIZE(eFormat) );
	
	(void) _snwprintf_s( eMessage, 
						 sizeof(eMessage)/sizeof(eMessage[0]),
						 _TRUNCATE, 
						 &eFormat[0], 
						 g_sVersionInfo.u8Major, 
						 g_sVersionInfo.u8Minor, 
						 g_sVersionInfo.u16BuildNumber );

	hItem = GetDlgItem( hDialog, IDC_STATIC_VERSION );

	SetWindowText( hItem, eMessage );

	// Set positions
	hItem = GetDlgItem( hDialog, IDC_EDIT_LOGIN_USER );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_USERNAME,
						 YPOS_USERNAME,
						 WIDTH_USERNAME,
						 HEIGHT_USERNAME,
						 true );
	BASSERT( bMoved );

	hItem = GetDlgItem( hDialog, IDC_EDIT_LOGIN_PASSWORD );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_PASSWORD,
						 YPOS_PASSWORD,
						 WIDTH_PASSWORD,
						 HEIGHT_PASSWORD,
						 true );
	BASSERT( bMoved );

	if (NetworkServerGroupNotSpecifiedOnCommandLine())
	{
		hItem = GetDlgItem( hDialog, IDC_COMBO_SERVER_GROUP );
		BASSERT( hItem );
		bMoved = MoveWindow( hItem, 
							 XPOS_SERVER_GROUP_COMBO,
							 YPOS_SERVER_GROUP_COMBO,
							 WIDTH_SERVER_GROUP_COMBO,
							 HEIGHT_SERVER_GROUP_COMBO,
							 true );
		BASSERT( bMoved );
	}

	hItem = GetDlgItem( hDialog, IDC_SIGNIN );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_LOGIN_BUTTON,
						 YPOS_LOGIN_BUTTON,
						 WIDTH_LOGIN_BUTTON,
						 HEIGHT_LOGIN_BUTTON,
						 true );
	BASSERT( bMoved );

	hItem = GetDlgItem( hDialog, IDC_STATIC_TESTMODE );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_TESTMODE,
						 YPOS_TESTMODE,
						 WIDTH_TESTMODE,
						 HEIGHT_TESTMODE,
						 true );
	BASSERT( bMoved );

	hItem = GetDlgItem( hDialog, IDC_STATIC_LOGINMESSAGE );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_LOGIN_MESSAGE,
						 YPOS_LOGIN_MESSAGE,
						 WIDTH_LOGIN_MESSAGE,
						 HEIGHT_LOGIN_MESSAGE,
						 true );
	BASSERT( bMoved );

	hItem = GetDlgItem( hDialog, IDC_STATIC_VERSION );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 LOGIN_DIALOG_WIDTH - XPOS_VERSION_OFFSET - WIDTH_VERSION,
						 LOGIN_DIALOG_HEIGHT - YPOS_VERSION_OFFSET - HEIGHT_VERSION,
						 WIDTH_VERSION,
						 HEIGHT_VERSION,
						 true );
	BASSERT( bMoved );

	hItem = GetDlgItem( hDialog, IDC_STATIC_COPYRIGHT );
	BASSERT( hItem );
	bMoved = MoveWindow( hItem, 
						 XPOS_COPYRIGHT_OFFSET,
						 LOGIN_DIALOG_HEIGHT - YPOS_COPYRIGHT_OFFSET - HEIGHT_COPYRIGHT,
						 WIDTH_COPYRIGHT,
						 HEIGHT_COPYRIGHT,
						 true );
	BASSERT( bMoved );

	// Set fonts into items
	(void) SendDlgItemMessage( hDialog, 
							   IDC_STATIC_VERSION, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontVersion, 
							   true );

	(void) SendDlgItemMessage( hDialog, 
							   IDC_STATIC_COPYRIGHT, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontVersion, 
							   true );

	(void) SendDlgItemMessage( hDialog, 
							   IDC_EDIT_LOGIN_USER, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontLogin, 
							   true );

	(void) SendDlgItemMessage( hDialog, 
							   IDC_EDIT_LOGIN_PASSWORD, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontLogin, 
							   true );

	(void) SendDlgItemMessage( hDialog, 
							   IDC_COMBO_SERVER_GROUP, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontLogin, 
							   true );

	(void) SendDlgItemMessage( hDialog, 
							   IDC_SIGNIN, 
							   WM_SETFONT, 
							   (WPARAM)sg_hFontLogin, 
							   true );

	eStatus = UIDialogFieldsLocalize(hDialog,
									 sg_sLoginControlToStringMap);
	BASSERT(ESTATUS_OK == eStatus);

	// Init the server group combo content
	InitializeServerGroupCombo(hDialog);

	// Display test mode if relevant
	s32Option = SW_HIDE;
	if( SharedIsInTestMode() )
	{
		s32Option = SW_SHOW;
	}

	hItem = GetDlgItem( hDialog, IDC_STATIC_TESTMODE );
	ShowWindow( hItem, s32Option );

	// Display optional login message and then reset it
	if( g_bUserMessageDisplay )
	{
		UILoginRedrawLoginText( hDialog, g_eUserMessageToDisplay );

		g_bUserMessageDisplay = false;
		ZERO_STRUCT( g_eUserMessageToDisplay );
	}
}


static DWORD WINAPI LoginAttemptThread( LPVOID pvParam )
{
	SEditControlErr sResult;
	HWND hDialog;
	WCHAR eUsername[WINDOWS_WCHAR_MAX];
	char eUsernameConverted[WINDOWS_UTF8_MAX];
	WCHAR ePassword[WINDOWS_WCHAR_MAX];
	char ePasswordConverted[WINDOWS_UTF8_MAX];
	EStatus eStatus;
	SUser* psLoggedInUser = NULL;
	WCHAR eErrMessage[WINDOWS_WCHAR_MAX];
	uint32_t u32ServerGroupID = 0;
	SServerGroup *psGroup = NULL;

	hDialog = (HWND) pvParam;

	UIErrorEditCtrlInitSuccess(&sResult);

	g_psLoggedInUser = NULL;

	GetDlgItemText( hDialog,
					IDC_EDIT_LOGIN_USER,
					&eUsername[0],
					sizeof(eUsername)/sizeof(eUsername[0]) );

	GetDlgItemText( hDialog,
					IDC_EDIT_LOGIN_PASSWORD,
					&ePassword[0],
					sizeof(ePassword)/sizeof(ePassword[0]) );

	// Convert to UTF8 for sending to the server
	WideCharToMultiByte( CP_UTF8, 
						 0, 
						 &eUsername[0], 
						 -1, 
						 (LPSTR)&eUsernameConverted[0], 
						 sizeof(eUsernameConverted), 
						 NULL, 
						 NULL );

	WideCharToMultiByte( CP_UTF8, 
						 0, 
						 &ePassword[0], 
						 -1, 
						 (LPSTR)&ePasswordConverted[0], 
						 sizeof(ePasswordConverted), 
						 NULL, 
						 NULL );

	SyslogFunc( "User %s login attempt.\n", 
				&eUsernameConverted[0] );

	// Figure out what our selection is
	u32ServerGroupID = UIObjComboBoxGetSelected(GetDlgItem(hDialog, IDC_COMBO_SERVER_GROUP));

	if (0 == u32ServerGroupID)
	{
		// Server group not set
		eStatus = ESTATUS_SERVER_GROUP_NOT_SET;
		goto errorExit;
	}

	// Try to find it in our list
	psGroup = ServerGroupGetByIndex(NetworkGetServerGroupListHead(),
									u32ServerGroupID);
	if (NULL == psGroup)
	{
		SyslogFunc("Couldn't find server group ID %u - setting to 'not set'\n", u32ServerGroupID);
		eStatus = ESTATUS_SERVER_GROUP_NOT_SET;
	}
	else
	{
		// All good. Set it anyway. This will force it to be written to disk
		eStatus = NetworkSetServerGroupName(psGroup->peGroupName);
	}

	ERR_GOTO();

	// Now do the login
	eStatus = UserLogin( g_eGlobalConnectionHandle,
						 &eUsernameConverted[0],
						 &ePasswordConverted[0],
						 &psLoggedInUser );

	SYSLOG_SERVER_RESULT( "UserLogin", 
						  eStatus );

	if( eStatus != ESTATUS_OK )
	{
		if( ESTATUS_USER_CLINIC_ACTIVE_SELECT_REQUIRED == eStatus )
		{
			// Ignore this error because it just means the user needs to select a clinic
			// Fall through
		}
		else
		{
			UIErrorEditCtrlSetEStatus( &sResult,
									   eStatus );

			goto errorExit;
		}
	}

	// Check for two other conditions that result in a failed login
	if( NULL == psLoggedInUser )
	{
		BASSERT_MSG("No user selected");
	}
	else
	{
		eStatus = ESTATUS_OK;

		// No permission to do anything?
		if( 0 == (psLoggedInUser->u64UserPermissions & sg_u64PermissionMask) )
		{
			eStatus = ESTATUS_USER_PERMISSIONS_FAULT;

			ResourceGetString( ESTRING_LOGIN_FAILURE_INSUFFICIENT_PRIVILEGE,
							   &eErrMessage[0],
							   ARRAYSIZE(eErrMessage));
		}

		// No clinics available?
		else if( NULL == psLoggedInUser->psClinics )
		{
			eStatus = ESTATUS_CLINIC_USER_NOT_ASSOCIATED;

			ResourceGetString( ESTRING_ERROR_NO_CLINICS,
							   &eErrMessage[0],
							   ARRAYSIZE(eErrMessage) );
		}

		if( eStatus != ESTATUS_OK )
		{
			UIErrorEditCtrlSetEStatus( &sResult, eStatus );
			UIErrorEditCtrlSetMessageWCHAR( &sResult, eErrMessage );
			goto errorExit;
		}
	}

	g_psLoggedInUser = psLoggedInUser;

	// Go reinit the server group combo dialog in case we've updated our server group list
	InitializeServerGroupCombo(hDialog);

	UILoginCloseDialog( hDialog, 
						LOGIN_SUCCESS );


errorExit:
	// Failure to login?
	if( NULL == g_psLoggedInUser )
	{
		HWND hItem;

		eErrMessage[0] = L'\0';

		// Update the error text for the user
		UIErrorEditCtrlGetMessageWCHAR( &sResult,
										&eErrMessage[0],
										sizeof(eErrMessage)/sizeof(eErrMessage[0]) );
		UILoginRedrawLoginText( hDialog,
								eErrMessage );

		SyslogWCHAR(L"User login failure: %s\n", eErrMessage);

		// Reenable the login button
		hItem = GetDlgItem( hDialog, IDC_SIGNIN );
		EnableWindow( hItem, true );

		// Clean up partial success
		if( psLoggedInUser )
		{
			eStatus = UserLogout( g_eGlobalConnectionHandle,
								  &psLoggedInUser );
			SYSLOG_SERVER_RESULT( "UserLogout",
								  eStatus );

			UserDeallocate( &psLoggedInUser );
		}
	}

	return( 0 );
}

static INT_PTR CALLBACK DialogMessageHandler( HWND hDialog, 
											  UINT uMsg, 
											  WPARAM wParam, 
											  LPARAM lParam)
{
	// Non-static object redraw properties
	if( WM_CTLCOLOREDIT == uMsg )
	{
		HBRUSH hBrush;

		SetBkColor( (HDC)wParam,
					RGB(240, 240, 210) );

		hBrush = CreateSolidBrush( RGB(240, 240, 210) );

		return( (INT_PTR) hBrush );
	}

	// Static object redraw properties
	else if( WM_CTLCOLORSTATIC == uMsg )
	{
		HWND hItem;
		HBRUSH hBrush;

		// Login message?
		hItem = GetDlgItem( hDialog,
							IDC_STATIC_LOGINMESSAGE);

		if( hItem == (HWND) lParam )
		{
			SetTextColor( (HDC) wParam,
						  RGB(200, 0, 0) );

			SetBkMode( (HDC) wParam,
					   TRANSPARENT );

			hBrush = GetStockObject( NULL_BRUSH );

			return( (INT_PTR) hBrush );
		}

		// Otherwise, is it the test mode message?
		hItem = GetDlgItem( hDialog,
							IDC_STATIC_TESTMODE);

		if( hItem == (HWND) lParam )
		{
			SetTextColor( (HDC) wParam,
						  RGB(255, 255, 255) );

			SetBkColor( (HDC) wParam,
						RGB(255, 0, 0) );

			hBrush = CreateSolidBrush( RGB(255, 0, 0) );

			return( (INT_PTR) hBrush );
		}

		// If not either then make everything transparent
		SetBkMode( (HDC) wParam,
				   TRANSPARENT );
		hBrush = GetStockObject( NULL_BRUSH );
		return( (INT_PTR) hBrush );
	}

	// Redraw the background image
	else if( WM_ERASEBKGND == uMsg )
	{
		UILoginRedrawBackground( hDialog );
		return( true );
	}

	else if( WM_COMMAND == uMsg )
	{
		uint16_t u16Command;

		u16Command = LOWORD( wParam );

		if( IDCLOSE == u16Command )
		{
			SendMessage( hDialog,
						 WM_CLOSE,
						 0,
						 0);
			return( true );
		}
		else if( IDC_SIGNIN == u16Command )
		{
			HWND hButton;
			HANDLE hThread;
			WCHAR eMessage[WINDOWS_WCHAR_MAX];

			// Start by disabling the login button
			hButton = GetDlgItem( hDialog, IDC_SIGNIN);
			EnableWindow( hButton, false );

			// Create the thread - auto started
			hThread = CreateThread( NULL,
									0,
									LoginAttemptThread,
									(LPVOID)hDialog,
									0,
									NULL);
			BASSERT( hThread );

			ResourceGetString( ESTRING_LOGGING_IN,
							   &eMessage[0],
							   ARRAYSIZE(eMessage));

			UILoginRedrawLoginText( hDialog,
									eMessage );

			return( true );
		}
	}

	else if( WM_INITDIALOG == uMsg )
	{
		UILoginDialog( hDialog );
		return( true );
	}

	else if( WM_CLOSE == uMsg )
	{
		UILoginCloseDialog( hDialog, 
							LOGIN_CLOSE );
		return( true );
	}

	return( false );
}

ELoginResult UILogin( uint64_t u64PermissionsMask )
{
	sg_u64PermissionMask = u64PermissionsMask;
	return( DialogBox( NULL,
					   MAKEINTRESOURCE(IDD_LOGINDLG),
					   NULL,
					   DialogMessageHandler ));
}

