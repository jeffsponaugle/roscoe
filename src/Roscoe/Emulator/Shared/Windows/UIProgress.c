#include <stdio.h>
#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIProgress.h"

typedef struct
{
	HWND eCurrentHWND;
	EStringID eDescriptionID;
} SProgressStatus;

static volatile SProgressStatus sg_sProgressStatus;

// Move window to center position
static void UIProgressMoveWindow( HWND eHWND )
{
	HWND eParent;
	bool bResult;
	RECT sDialog;
	RECT sParent;

	eParent = GetParent( eHWND );
	if( NULL == eParent )
	{
		eParent = GetDesktopWindow();
	}

	bResult = GetWindowRect( eHWND, &sDialog );
	if( false == bResult )
	{
		goto errorExit;
	}

	bResult = GetWindowRect( eParent, &sParent );
	if( false == bResult )
	{
		goto errorExit;
	}

	(void) SetWindowPos( eHWND,
						 HWND_TOP,
						 sParent.left + ((sParent.right - sParent.left)/2) - ((sDialog.right - sDialog.left)/2),
						 sParent.top + ((sParent.bottom - sParent.top)/2) - ((sDialog.bottom - sDialog.top)/2),
						 0,
						 0,
						 SWP_NOSIZE | SWP_SHOWWINDOW );

errorExit:
	return;
}

static void UIProgressSetDescription( HWND eHWND, EStringID eDescriptionID )
{
	WCHAR eDescription[WINDOWS_WCHAR_MAX];
	HWND eItem;

	eDescription[0] = 0;

	ResourceGetString( eDescriptionID, 
					   &eDescription[0], 
					   ARRAYSIZE(eDescription));

	eItem = GetDlgItem(eHWND, IDC_UPDATE_DESCRIPTION);

	if( eItem )
	{
		(void) SetWindowText( eItem, &eDescription[0] );
	}
}

static INT_PTR CALLBACK UIProgressDialogProcessor( HWND eHWND,
												   UINT uMsg,
												   WPARAM wParam,
												   LPARAM lParam )
{
	if( WM_INITDIALOG == uMsg )
	{
		UIProgressMoveWindow( eHWND );
		UIProgressSetDescription( eHWND, sg_sProgressStatus.eDescriptionID );
		EnableWindow( GetParent(eHWND), false );
		return( true );
	}
	else if( WM_CLOSE == uMsg )
	{
		sg_sProgressStatus.eCurrentHWND = NULL;
		EnableWindow( GetParent(eHWND), true );
		DestroyWindow( eHWND );
		return( true );
	}

	return( false );
}

void UIProgressUpdate( uint64_t u64Current,
					   uint64_t u64Max,
					   void* pvPrivate )
{
	uint8_t u8Pct;
	WCHAR eFmt[WINDOWS_WCHAR_MAX];
	WCHAR eStatusMsg[WINDOWS_WCHAR_MAX];
	HWND eItem;

	// The dialog handle must exist if we're to update it
	BASSERT( sg_sProgressStatus.eCurrentHWND );

	if( 0 == u64Max )
	{
		u8Pct = 0;
	}
	else
	{
		u8Pct = (uint8_t)((u64Current * 100) / u64Max);
	}

	SyslogFunc( "Current=%llu, Expected=%llu: %u%%\n", 
				u64Current, 
				u64Max,
				u8Pct );

	SendDlgItemMessage( sg_sProgressStatus.eCurrentHWND,
						IDC_PROGRESS_UPDATE,
						PBM_SETRANGE32,
						0,
						(uint32_t) u64Max );

	SendDlgItemMessage( sg_sProgressStatus.eCurrentHWND,
						IDC_PROGRESS_UPDATE,
						PBM_SETPOS,
						(uint32_t) u64Current,
						0 );

	eFmt[0] = 0;
	ResourceGetString( ESTRING_PROGRESS_FORMAT,
					   &eFmt[0],
					   ARRAYSIZE(eFmt));

	(void) _snwprintf_s( eStatusMsg, 
						 sizeof(eStatusMsg)/sizeof(eStatusMsg[0]), 
						 _TRUNCATE, 
						 &eFmt[0], 
						 u8Pct);

	eItem = GetDlgItem( sg_sProgressStatus.eCurrentHWND, IDC_STATIC_PROGRESS );
	if( eItem )
	{
		SetWindowText( eItem, eStatusMsg );
	}

// Right now we're only closing the dialog window manually, as some server calls linger well
// past 100% complete notification.
#if 0
	// If complete, shut down the progress dialog
	if( u64Current == u64Max )
	{
		Syslog("Progress complete - closing window\n");
		SendMessage( sg_sProgressStatus.eCurrentHWND, 
					 WM_CLOSE, 
					 0, 
					 0 );
	}
#endif
}


void UIProgressClose( void )
{
	if( sg_sProgressStatus.eCurrentHWND )
	{
		SendMessage( sg_sProgressStatus.eCurrentHWND, 
					 WM_CLOSE, 
					 0, 
					 0 );
	}
}

HWND UIProgressCreate( HWND eHWND,
					   EStringID eDescriptionID )
{
	if( sg_sProgressStatus.eCurrentHWND )
	{
		return(NULL);
	}

	// Set up the dialog state
	sg_sProgressStatus.eDescriptionID = eDescriptionID;

	sg_sProgressStatus.eCurrentHWND = CreateDialog( NULL,
													MAKEINTRESOURCE(IDD_PROGRESSDLG),
													eHWND,
													UIProgressDialogProcessor );

	if( NULL == sg_sProgressStatus.eCurrentHWND )
	{
		SyslogFunc( "Failed to create progress dialog %d\n", errno );
	}

	return( sg_sProgressStatus.eCurrentHWND );
}


