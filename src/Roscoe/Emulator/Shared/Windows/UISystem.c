#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UISystem.h"
#include "Shared/Windows/UIUser.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UIClinic.h"
#include "Shared/Windows/UIPatient.h"
#include "Shared/SharedPatient.h"

void UISystemGetTreatmentLocationList( STreatmentLocation** ppsList,
									   SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	STreatmentLocation* psHead;

	psHead = TreatmentLocationGetHead();

	if( psHead )
	{
		*ppsList = psHead;
		return;
	}

	// Get the list from the server
	eStatus = TreatmentLocationGetList( g_eGlobalConnectionHandle,
										ppsList );
	
	SYSLOG_SERVER_RESULT("TreatmentLocationGetList", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

static void UISystemGetLanguageList( SLanguage** ppsList,
									 SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	SLanguage* psHead;

	psHead = LanguageGetListHead();

	if( psHead )
	{
		*ppsList = psHead;
		return;
	}

	// Get the list from the server
	eStatus = LanguageGetList( g_eGlobalConnectionHandle,
							   ppsList );
	
	SYSLOG_SERVER_RESULT("LanguageGetList", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

void UISystemRefreshLanguages( HWND eDialog,
							   int32_t s32ControlID,
							   SEditControlErr* psErr )
{
	HWND eComboHwnd;
	SLanguage* psHead = NULL;
	SLanguage* psItem;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	// Get the list from the server
	UISystemGetLanguageList( &psHead, psErr );
	UIERR_FAIL_RETURN( psErr );

	ComboBox_ResetContent( eComboHwnd );

	// Run through the list twice, first to find "unspecified" and then to add the rest
	psItem = psHead;
	while( psItem )
	{
		WCHAR eBuffer[WINDOWS_WCHAR_MAX];

		if( ELANG_NOT_SPECIFIED == psItem->eLanguageEnum )
		{
			// Convert UTF8 to WCHAR
			if( psItem->peLanguageName )
			{
				(void) MultiByteToWideChar( CP_UTF8, 
											0, 
											(LPCSTR)(psItem->peLanguageName), 
											-1, 
											&eBuffer[0], 
											sizeof(eBuffer)/sizeof(eBuffer[0]) );
			}
			else
			{
				eBuffer[0] = 0;
			}

			UIObjComboBoxAdd( eComboHwnd,
							  eBuffer,
							  psItem->eLanguageEnum,
							  psErr );
			UIERR_FAIL_RETURN( psErr );

			// Only expect one so break out when found
			break;
		}

		psItem = psItem->psNextLink;
	}

	psItem = psHead;
	while( psItem )
	{
		WCHAR eBuffer[WINDOWS_WCHAR_MAX];

		if( psItem->eLanguageEnum != ELANG_NOT_SPECIFIED )
		{
			// Convert UTF8 to WCHAR
			if( psItem->peLanguageName )
			{
				(void) MultiByteToWideChar( CP_UTF8, 
											0, 
											(LPCSTR)(psItem->peLanguageName), 
											-1, 
											&eBuffer[0], 
											sizeof(eBuffer)/sizeof(eBuffer[0]) );
			}
			else
			{
				eBuffer[0] = 0;
			}

			UIObjComboBoxAdd( eComboHwnd,
							  eBuffer,
							  psItem->eLanguageEnum,
							  psErr );
			UIERR_FAIL_RETURN( psErr );
		}

		psItem = psItem->psNextLink;
	}

	// Select the first one in the list
	ComboBox_SetCurSel( eComboHwnd, 0 );
}

ELanguage UISystemGetSelectedLanguage( HWND eDialog,
									   int32_t s32ControlID )
{
	HWND eComboHwnd;
	int32_t s32Index;
	LRESULT eResult;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	s32Index = ComboBox_GetCurSel( eComboHwnd );
	BASSERT( s32Index != CB_ERR );

	eResult = ComboBox_GetItemData( eComboHwnd, 
									s32Index );
	BASSERT( eResult != CB_ERR );

	return( (ELanguage) eResult );
}

bool UISystemSelectLanguage( HWND eDialog,
							 int32_t s32ControlID,
							 ELanguage eItem )
{
	HWND eComboHwnd;
	int32_t s32Count;
	int32_t s32Index;

	eComboHwnd = GetDlgItem( eDialog, 
							 s32ControlID );
	BASSERT( eComboHwnd );

	s32Count = ComboBox_GetCount( eComboHwnd );
	BASSERT( s32Count != CB_ERR );

	// Search the combo box for an item with matching data parameter
	s32Index = 0;
	while( s32Index < s32Count )
	{
		ELanguage eComboData;

		eComboData = (ELanguage) ComboBox_GetItemData( eComboHwnd, 
													   s32Index );
		if( eItem == eComboData )
		{
			ComboBox_SetCurSel( eComboHwnd, 
								s32Index );
			return( true );
		}

		s32Index++;
	}

	ComboBox_SetCurSel( eComboHwnd, 0 );

	return( false );
}

static void UISystemGetClassificationList( SPatientClassification** ppsList,
										   SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	SPatientClassification* psHead;

	psHead = PatientGetClassificationListHead();

	if( psHead )
	{
		*ppsList = psHead;
		return;
	}

	// Get the list from the server
	eStatus = PatientGetClassifications( g_eGlobalConnectionHandle,
										 ppsList );
	
	SYSLOG_SERVER_RESULT("PatientGetClassifications", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

void UISystemRefreshClassifications( HWND eDialog,
									 int32_t s32ControlID,
									 SEditControlErr* psErr )
{
	HWND eComboHwnd;
	SPatientClassification* psHead = NULL;
	SPatientClassification* psItem;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	// Get the list from the server
	UISystemGetClassificationList( &psHead, psErr );
	UIERR_FAIL_RETURN( psErr );

	ComboBox_ResetContent( eComboHwnd );

	psItem = psHead;
	while( psItem )
	{
		WCHAR eNameBuffer[WINDOWS_WCHAR_MAX];
		WCHAR eAbvBuffer[WINDOWS_WCHAR_MAX];
		WCHAR eCombinedBuffer[WINDOWS_WCHAR_MAX];

		// Convert UTF8 to WCHAR
		if( psItem->peDescription )
		{
			(void) MultiByteToWideChar( CP_UTF8, 
										0, 
										(LPCSTR)(psItem->peDescription), 
										-1, 
										&eNameBuffer[0], 
										sizeof(eNameBuffer)/sizeof(eNameBuffer[0]) );
		}
		else
		{
			eNameBuffer[0] = 0;
		}

		// Convert UTF8 to WCHAR
		if( psItem->peAbbreviation )
		{
			(void) MultiByteToWideChar( CP_UTF8, 
										0, 
										(LPCSTR)(psItem->peAbbreviation), 
										-1, 
										&eAbvBuffer[0], 
										sizeof(eAbvBuffer)/sizeof(eAbvBuffer[0]) );
		}
		else
		{
			eAbvBuffer[0] = 0;
		}

		// Build the combined string
		_snwprintf_s( &eCombinedBuffer[0], 
					  sizeof(eCombinedBuffer)/sizeof(eCombinedBuffer[0]), 
					  _TRUNCATE, 
					  L"%s [%s]", 
					  &eNameBuffer[0],
					  &eAbvBuffer[0] );

		UIObjComboBoxAdd( eComboHwnd,
						  eCombinedBuffer,
						  psItem->eClassification,
						  psErr );
		UIERR_FAIL_RETURN( psErr );

		psItem = psItem->psNextLink;
	}

	// Select the first one in the list
	ComboBox_SetCurSel( eComboHwnd, 0 );
}

EPatientClassification UISystemGetSelectedClassification( HWND eDialog,
														  int32_t s32ControlID )
{
	HWND eComboHwnd;
	int32_t s32Index;
	LRESULT eResult;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	s32Index = ComboBox_GetCurSel( eComboHwnd );
	BASSERT( s32Index != CB_ERR );

	eResult = ComboBox_GetItemData( eComboHwnd, 
									s32Index );
	BASSERT( eResult != CB_ERR );

	return( (EPatientClassification) eResult );
}

bool UISystemSelectClassification( HWND eDialog,
								   int32_t s32ControlID,
								   EPatientClassification eItem )
{
	HWND eComboHwnd;
	int32_t s32Count;
	int32_t s32Index;

	eComboHwnd = GetDlgItem( eDialog, 
							 s32ControlID );
	BASSERT( eComboHwnd );

	s32Count = ComboBox_GetCount( eComboHwnd );
	BASSERT( s32Count != CB_ERR );

	// Search the combo box for an item with matching data parameter
	s32Index = 0;
	while( s32Index < s32Count )
	{
		ECountry eComboData;

		eComboData = (EPatientClassification) ComboBox_GetItemData( eComboHwnd, 
																	s32Index );
		if( eItem == eComboData )
		{
			ComboBox_SetCurSel( eComboHwnd, 
								s32Index );
			return( true );
		}

		s32Index++;
	}

	ComboBox_SetCurSel( eComboHwnd, 0 );

	return( false );
}

static void UISystemGetCountryList( SCountry** ppsList,
									SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	SCountry* psHead;

	psHead = CountryListGetHead();

	if( psHead )
	{
		*ppsList = psHead;
		return;
	}

	// Get the list from the server
	eStatus = CountryGetList( g_eGlobalConnectionHandle,
							  ppsList );
	
	SYSLOG_SERVER_RESULT("CountryGetList", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

ECountry UISystemGetSelectedCountry( HWND eDialog,
									 int32_t s32ControlID )
{
	HWND eComboHwnd;
	int32_t s32Index;
	LRESULT eResult;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	s32Index = ComboBox_GetCurSel( eComboHwnd );
	BASSERT( s32Index != CB_ERR );

	eResult = ComboBox_GetItemData( eComboHwnd, 
									s32Index );
	BASSERT( eResult != CB_ERR );

	return( (ECountry) eResult );
}

bool UISystemSelectCountry( HWND eDialog,
							int32_t s32ControlID,
							ECountry eItem )
{
	HWND eComboHwnd;
	int32_t s32Count;
	int32_t s32Index;

	eComboHwnd = GetDlgItem( eDialog, 
							 s32ControlID );
	BASSERT( eComboHwnd );

	s32Count = ComboBox_GetCount( eComboHwnd );
	BASSERT( s32Count != CB_ERR );

	// Search the combo box for an item with matching data parameter
	s32Index = 0;
	while( s32Index < s32Count )
	{
		ECountry eComboData;

		eComboData = (ECountry) ComboBox_GetItemData( eComboHwnd, 
													  s32Index );
		if( eItem == eComboData )
		{
			ComboBox_SetCurSel( eComboHwnd, 
								s32Index );
			return( true );
		}

		s32Index++;
	}

	ComboBox_SetCurSel( eComboHwnd, 0 );

	return( false );
}

void UISystemRefreshCountries( HWND eDialog,
							   int32_t s32ControlID,
							   SEditControlErr* psErr )
{
	HWND eComboHwnd;
	SCountry* psHead = NULL;
	SCountry* psOption;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	// Get the list from the server
	UISystemGetCountryList( &psHead, psErr );
	UIERR_FAIL_RETURN( psErr );

	ComboBox_ResetContent( eComboHwnd );

	psOption = psHead;
	while( psOption )
	{
		WCHAR eBuffer[WINDOWS_WCHAR_MAX];

		// Convert UTF8 to WCHAR
		if( psOption->peCountryName )
		{
			(void) MultiByteToWideChar( CP_UTF8, 
										0, 
										(LPCSTR)(psOption->peCountryName), 
										-1, 
										&eBuffer[0], 
										sizeof(eBuffer)/sizeof(eBuffer[0]) );
		}
		else
		{
			eBuffer[0] = 0;
		}

		UIObjComboBoxAdd( eComboHwnd,
						  eBuffer,
						  psOption->eCountryEnum,
						  psErr );
		UIERR_FAIL_RETURN( psErr );

		psOption = psOption->psNextLink;
	}

	// Select the first one in the list
	ComboBox_SetCurSel( eComboHwnd, 0 );
}

static void UISystemGetReferredByOptionsList( SPatientReferredByOptions** ppsList,
											  SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	SPatientReferredByOptions* psHead;

	psHead = PatientGetReferredByHead();

	if( psHead )
	{
		*ppsList = psHead;
		return;
	}

	// Get the list from the server
	eStatus = PatientGetReferredByOptions( g_eGlobalConnectionHandle,
										   ppsList );
	
	SYSLOG_SERVER_RESULT("PatientGetReferredByOptions", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

EPatientReferredBy UISystemGetSelectedReferredByID( HWND eDialog,
													int32_t s32ControlID )
{
	HWND eComboHwnd;
	int32_t s32Index;
	LRESULT eResult;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	s32Index = ComboBox_GetCurSel( eComboHwnd );
	BASSERT( s32Index != CB_ERR );

	eResult = ComboBox_GetItemData( eComboHwnd, 
									s32Index );
	BASSERT( eResult != CB_ERR );

	return( (EPatientReferredBy) eResult );
}

bool UISystemSelectReferredByOptionsItem( HWND eDialog, 
										  int32_t s32ControlID,
										  EPatientReferredBy eItem )
{
	HWND eComboHwnd;
	int32_t s32Count;
	int32_t s32Index;

	eComboHwnd = GetDlgItem( eDialog, 
							 s32ControlID );
	BASSERT( eComboHwnd );

	s32Count = ComboBox_GetCount( eComboHwnd );
	BASSERT( s32Count != CB_ERR );

	// Search the combo box for an item with matching data parameter
	s32Index = 0;
	while( s32Index < s32Count )
	{
		EPatientReferredBy eComboData;

		eComboData = (EPatientReferredBy) ComboBox_GetItemData( eComboHwnd, 
																s32Index );
		if( eItem == eComboData )
		{
			ComboBox_SetCurSel( eComboHwnd, 
								s32Index );
			return( true );
		}

		s32Index++;
	}

	ComboBox_SetCurSel( eComboHwnd, 0 );

	return( false );
}

bool UISystemIsReferredByOptionOther( HWND eDialog,
									  int32_t s32ControlID )
{
	EPatientReferredBy eReferralID;
	SPatientReferredByOptions* psOption = NULL;
	SEditControlErr sUnused;
	
	UIErrorEditCtrlInitSuccess( &sUnused );

	eReferralID = UISystemGetSelectedReferredByID( eDialog, 
												   s32ControlID );

	UISystemGetReferredByOptionsList( &psOption,
									  &sUnused );

	while( psOption )
	{
		if( psOption->eValue == eReferralID )
		{
			return( psOption->bEnableSupplementalField );
		}

		psOption = psOption->psNextLink;
	}

	SyslogFunc("Referred by option %d not available\n", eReferralID);
	BASSERT_MSG("Referred by option not available");

	return( false );
}

void UISystemRefreshReferredByOptions( HWND eDialog,
									   int32_t s32ControlID,
									   SEditControlErr* psErr )
{
	HWND eComboHwnd;
	SPatientReferredByOptions* psHead = NULL;
	SPatientReferredByOptions* psOption;

	eComboHwnd = GetDlgItem( eDialog, s32ControlID );
	BASSERT( eComboHwnd );

	// Get the list from the server
	UISystemGetReferredByOptionsList( &psHead, psErr );
	UIERR_FAIL_RETURN( psErr );

	ComboBox_ResetContent( eComboHwnd );

	psOption = psHead;
	while( psOption )
	{
		WCHAR eBuffer[WINDOWS_WCHAR_MAX];

		// Convert UTF8 to WCHAR
		if( psOption->peDescription )
		{
			(void) MultiByteToWideChar( CP_UTF8, 
										0, 
										(LPCSTR)(psOption->peDescription), 
										-1, 
										&eBuffer[0], 
										sizeof(eBuffer)/sizeof(eBuffer[0]) );
		}
		else
		{
			eBuffer[0] = 0;
		}

		UIObjComboBoxAdd( eComboHwnd,
						  eBuffer,
						  psOption->eValue,
						  psErr );
		UIERR_FAIL_RETURN( psErr );

		psOption = psOption->psNextLink;
	}

	// Select the first one in the list
	ComboBox_SetCurSel( eComboHwnd, 0 );
}

void UISystemRefreshSystemLists( uint32_t u32Flags,
								 SEditControlErr* psEditCtrlErr )
{
	// Runtime check for requests for both included and excluded lists at the same time, not allowed.
	if( (u32Flags & UISYSTEM_USERS_INCL_DISABLED) && 
		(u32Flags & UISYSTEM_USERS_EXCL_DISABLED) )
	{
		BASSERT_MSG("Requested included and excluded users");
	}

	if( (u32Flags & UISYSTEM_USERS_ASSOCIATED_INCL_DISABLED) && 
		(u32Flags & UISYSTEM_USERS_ASSOCIATED_EXCL_DISABLED) )
	{
		BASSERT_MSG("Requested included and excluded user associations");
	}

	if( u32Flags & UISYSTEM_USERS_INCL_DISABLED )
	{
		UIUserListRefresh( EUSERLIST_ALL, psEditCtrlErr, true );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_USERS_EXCL_DISABLED )
	{
		UIUserListRefresh( EUSERLIST_ALL, psEditCtrlErr, false );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_USERS_ASSOCIATED_INCL_DISABLED )
	{
		UIUserListRefresh( EUSERLIST_ASSOCIATED, psEditCtrlErr, true );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_USERS_ASSOCIATED_EXCL_DISABLED )
	{
		UIUserListRefresh( EUSERLIST_ASSOCIATED, psEditCtrlErr, false );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_AUTHORIZERS )
	{
		UIUserListRefresh( EUSERLIST_AUTHORIZERS, psEditCtrlErr, false );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_CLINICS )
	{
		UIClinicListRefresh( psEditCtrlErr );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_PATIENTS_ALL )
	{
		UIPatientRefreshPatientList( false, false, psEditCtrlErr );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_PATIENTS_ASSOCIATED_CLINICS )
	{
		UIPatientRefreshPatientList( true, true, psEditCtrlErr );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}

	if( u32Flags & UISYSTEM_PATIENTS_ACTIVE_CLINIC )
	{
		UIPatientRefreshPatientList( true, false, psEditCtrlErr );
		UIERR_FAIL_RETURN( psEditCtrlErr );
	}
}

static void UISystemGetFieldInfoList( SFieldInfo** ppsHead,
									  SEditControlErr* psEditCtrlErr )
{
	EStatus eStatus;
	SFieldInfo* psHead = NULL;

	psHead = FieldInfoGetHead();

	if( psHead )
	{
		*ppsHead = psHead;
		return;
	}

	eStatus = FieldGetInfo( g_eGlobalConnectionHandle,
							ETABLE_ALL,
							ppsHead );

	SYSLOG_SERVER_RESULT("FieldGetInfo", eStatus);
	HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr);
}

SFieldInfo* UISystemLookupFieldInfo( EAtomID eAtomID )
{
	SEditControlErr sResult;
	SFieldInfo* psInfoHead = NULL;
	SFieldInfo* psInfo = NULL;

	UIErrorEditCtrlInitSuccess( &sResult );

	// Get the field descriptions from the server
	UISystemGetFieldInfoList( &psInfoHead, &sResult );

	// Check for error communicating with the server
	if( false == UIErrorEditCtrlSuccess(&sResult) )
	{
		goto errorExit;
	}

	// Search the server list for the item represented by the atom ID
	psInfo = psInfoHead;
	while( psInfo )
	{
		if( psInfo->eAtomID == eAtomID )
		{
			break;
		}

		psInfo = psInfo->psNextLink;
	}

errorExit:
	return( psInfo );
}

