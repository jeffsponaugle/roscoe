#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/SharedUser.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIClinic.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"

static const SControlToStringID sg_sActiveClinicControlToStringMap[] =
{
	// Labels
	{IDC_STATIC_SELECT_ACTIVE_CLINIC,	ESTRING_SELECT_ACTIVE_CLINIC},

	// Buttons
	{IDOK,								ESTRING_OK},

	// Terminator
	{WINDOW_ID_TERMINATOR}
};

// This finds a clinic by its object ID
SClinic *UIFindClinicByObjectID(uint32_t u32ObjectID)
{
	SClinic *psClinic = NULL;
	EStatus eStatus;

	eStatus = ClinicListSetLock(true);
	ERR_GOTO();
	
	psClinic = ClinicListGetHead();

	// pvUser == Windows ID
	while (psClinic)
	{
		if (((uint32_t) psClinic->sClinicGUID.pvUser) == u32ObjectID)
		{
			break;
		}
		else
		{
			psClinic = psClinic->psNextLink;
		}
	}

errorExit:
	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	return(psClinic);
}

static void InitClinicSelection(HWND eDialogHwnd, 
								SGUID *psActiveClinicGUID, 
								SEditControlErr *psEditCtrlErr)
{
	HWND eClinicListHwnd = GetDlgItem(eDialogHwnd, IDC_COMBO_CLINICLIST);
	SUserClinicAssociationRecord *psAssociationRecord;
	SClinic *psClinic = NULL;
	WCHAR eClinicName[WINDOWS_WCHAR_MAX];
	EStatus eStatus;

	UIObjDialogInit(eDialogHwnd,
					0,
					0,
					ESTRING_SELECT_CLINIC);

	// User had better exist
	BASSERT(g_psLoggedInUser);

	// Localize the dialog
	eStatus = UIDialogFieldsLocalize(eDialogHwnd,
									 sg_sActiveClinicControlToStringMap);
	BASSERT(ESTATUS_OK == eStatus);

	// Record the active clinic for later retrieval
	SetWindowLongPtr(eDialogHwnd, 
					 GWLP_USERDATA, 
					 (LONG_PTR) psActiveClinicGUID);

	// Clear clinic list
	ComboBox_ResetContent(eClinicListHwnd);

	// Insert the "select a clinic" meta clinic
	ResourceGetString(ESTRING_PLEASE_SELECT_CLINIC_PARENTHESIS, 
					  eClinicName, 
					  ARRAYSIZE(eClinicName));

	UIObjComboBoxAdd(eClinicListHwnd, 
					eClinicName, 
					SELECT_CLINIC_ID, 
					psEditCtrlErr);
	UIERR_FAIL_RETURN(psEditCtrlErr);

	// Add the clinics to the combo box
	psAssociationRecord = g_psLoggedInUser->psClinics;

	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	while (psAssociationRecord)
	{
		psClinic = ClinicListFindByGUID(&psAssociationRecord->sClinicGUID);
		BASSERT(psClinic);

		// Only include enabled clinics
		if (psClinic->bClinicEnabled)
		{
			UIUTF8ToWCHAR(eClinicName, 
						  ARRAYSIZE(eClinicName),
						  psClinic->peClinicName);

			UIObjComboBoxAdd(eClinicListHwnd, 
							eClinicName, 
							(uint32_t) psClinic->sClinicGUID.pvUser, 
							psEditCtrlErr);
			UIERR_FAIL_GOTO(psEditCtrlErr, errorExit);
		}
		
		psAssociationRecord = psAssociationRecord->psNextLink;
	}

	// Highlight the "select clinic" item
	UIObjComboBoxSelect(eClinicListHwnd, SELECT_CLINIC_ID);

errorExit:

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}

static void HandleClinicSelection(HWND eDialogHwnd, SEditControlErr *psEditCtrlErr)
{
	HWND eClinicListHwnd = GetDlgItem(eDialogHwnd, IDC_COMBO_CLINICLIST);	
	SClinic *psClinic = NULL;
	SGUID *psActiveClinicGUID = NULL;
	uint32_t u32InternalID;
	EStatus eStatus;

	// User had better exist
	BASSERT(g_psLoggedInUser);

	// Get storage location for active clinic's GUID
	psActiveClinicGUID = (SGUID*) GetWindowLongPtr(eDialogHwnd, 
												   GWLP_USERDATA);
	BASSERT(psActiveClinicGUID);

	u32InternalID = UIObjComboBoxGetSelected(eClinicListHwnd);

	// IF the user selects the "select clinic" item, throw up a popup to let them know
	// they're being an idiot and to try something else in the list.
	if (SELECT_CLINIC_ID == u32InternalID)
	{
		WCHAR eTitle[WINDOWS_WCHAR_MAX];
		WCHAR eMessage[WINDOWS_MBOX_MAX];

		ResourceGetString(ESTRING_APP_TITLE, 
						  eTitle, 
						  ARRAYSIZE(eTitle));
		ResourceGetString(ESTRING_ERROR_SELECT_CLINIC, 
						  eMessage, 
						  ARRAYSIZE(eMessage));

		MessageBox(eDialogHwnd, 
				   eMessage, 
				   eTitle, 
				   MB_ICONSTOP);

		UIErrorEditCtrlSetHResult(psEditCtrlErr, E_ABORT);

		return;
	}

	// Valid clinic is now selected
	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	psClinic = UIFindClinicByObjectID(u32InternalID);
	BASSERT(psClinic);

	// Record it back from the caller.
	GUIDCopy(psActiveClinicGUID, &psClinic->sClinicGUID);

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}

static INT_PTR CALLBACK ClinicSelectDlgProc(HWND eDialogHwnd, UINT u32Msg, WPARAM wParam, LPARAM lParam)
{
	switch (u32Msg)
	{
		case WM_INITDIALOG:
		{	
			SEditControlErr sResult;

			UIErrorEditCtrlInitSuccess(&sResult);
			InitClinicSelection(eDialogHwnd, 
								(SGUID*) lParam, 
								&sResult);
			UIErrorHandle(eDialogHwnd, 
							  &sResult);
			return(true);
		}

		case WM_COMMAND:
		{
			uint16_t u16CmdControl = LOWORD(wParam);

			switch (u16CmdControl)
			{
				case IDOK:
				{	
					SEditControlErr sResult;
					UIErrorEditCtrlInitSuccess(&sResult);
			
					HandleClinicSelection(eDialogHwnd, &sResult);
			
					// If it came back aborted, they tried to select the meta clinic and we provided an appropriate message, so don't exit.  
					//
					if (false == UIErrorAborted(&sResult))
					{
						UIErrorHandle(eDialogHwnd, 
										  &sResult);
						EndDialog(eDialogHwnd, 1);
					}
			
					return(true);
				}
			}
		}
	}

	return(false);
}

void SelectActiveClinicDialog(HWND eMainHwnd, SGUID *psClinicGUID, SEditControlErr *psEditCtrlErr)
{
	SClinic *psActiveClinic = NULL;
	SGUID sActiveClinicGUID;
	INT_PTR eResult;
	EStatus eStatus;

	BASSERT(g_psLoggedInUser);
	BASSERT(g_psLoggedInUser->psClinics);

	// If the logged-in user has only one clinic, that's (obviously) the one!  Otherwise, we prompt from
	// the list of the user's associated clinics.
	if (NULL == g_psLoggedInUser->psClinics->psNextLink)
	{
		GUIDCopy(&sActiveClinicGUID, 
				 &g_psLoggedInUser->psClinics->sClinicGUID);
	}
	else
	{
		eResult = DialogBoxParam(NULL, 
								 MAKEINTRESOURCE(IDD_CLINICSELECT), 
								 eMainHwnd, 
								 ClinicSelectDlgProc, 
								 (LPARAM) &sActiveClinicGUID);
		BASSERT(eResult > 0);
	}

	// Get the active clinic from the active clinic GUID
	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	psActiveClinic = ClinicListFindByGUID(&sActiveClinicGUID);
	BASSERT(psActiveClinic);

	// Set the active clinic
	eStatus = ClinicSetActive(g_eGlobalConnectionHandle,
							  psActiveClinic);

	SYSLOG_SERVER_RESULT("ClinicSetActive", eStatus);

	if (eStatus != ESTATUS_OK)
	{
		UIErrorEditCtrlSetEStatus(psEditCtrlErr, eStatus);
	}
	else
	if (psClinicGUID)
	{
		GUIDCopy(psClinicGUID, &sActiveClinicGUID);
	}

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}

EStatus UIClinicListRefresh(SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	SClinic *psClinicHead = NULL;
	SClinic *psClinic = NULL;

	// Go get the clinic list from the server
	eStatus = ClinicsGet(g_eGlobalConnectionHandle,
						 NULL,
						 NULL,
						 &psClinicHead,
						 true);
	SYSLOG_SERVER_RESULT("ClinicsGet", eStatus);
	ERR_GOTO();

	// Set up new internal IDs
	psClinic = psClinicHead;
	while (psClinic)
	{
		psClinic->sClinicGUID.pvUser = (void *) SharedGetID();
		psClinic = psClinic->psNextLink;
	}

	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	ClinicListReplace(psClinicHead);

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

errorExit:
	return(eStatus);
}

bool UIClinicIsInAssociatedList(SGUID *psClinicGUID)
{
	SUserClinicAssociationRecord *psAssoc;

	BASSERT(g_psLoggedInUser);
	BASSERT(g_psLoggedInUser->psClinics);

	if (UserAllClinicsVisible(g_psLoggedInUser))
	{
		return(true);
	}

	psAssoc = g_psLoggedInUser->psClinics;

	while (psAssoc)
	{
		if (GUIDCompare(psClinicGUID, 
						&psAssoc->sClinicGUID))
		{
			return(true);
		}
		else
		{
			psAssoc = psAssoc->psNextLink;
		}
	}

	return(false);
}

// This will refresh *A* clinic list (provided on input)
void UIClinicRefreshList(HWND eHWND, 
						 int32_t s32WindowsID, 
						 SClinic *psClinic, 
						 WCHAR *peFilter, 
						 SGUID *psExcludedGUIDs, 
						 uint32_t u32ExcludedGUIDCount, 
						 SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus;
	HWND eClinicHWND;

	// Get the clinic's list box HWND
	eClinicHWND = GetDlgItem(eHWND,
							 s32WindowsID);
	BASSERT(eClinicHWND);

	// Start by clearing it
	ListBox_ResetContent(eClinicHWND);

	// Shut off list box redraw while we're filling it so it doesn't flicker like crazy
	SendMessage(eClinicHWND, 
				WM_SETREDRAW, 
				false, 
				0);

	// Lock the clinic list
	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	// Run through all clinics and post them to the list box
	while (psClinic)
	{
		bool bAddClinic;
		WCHAR eClinicName[WINDOWS_WCHAR_MAX];

		// Assume at first we add this clinic
		bAddClinic = true;

		// If there's no clinic name, barf.
		if (NULL == psClinic->peClinicName)
		{
			eStatus = ESTATUS_CLINIC_NAME_MISSING;
			goto errorExit;
		}

		// Convert the clinic name to WCHAR
		MultiByteToWideChar(CP_UTF8,
							0,
							(LPCSTR) psClinic->peClinicName,
							-1,
							eClinicName,
							ARRAYSIZE(eClinicName));

		if ((NULL == peFilter) ||
			(wcslen(peFilter) == 0))
		{
			// Fall throgh - we want it if it's nonexistent or a zero length filter
		}
		else
		{
			// Search for the clinic name in the filter string
			if (StrStrI(eClinicName,
						peFilter) == NULL)
			{
				// Not in the filter string so don't include it
				bAddClinic = false;
			}
		}

		// Check through the exclusion list and see if this clinic is in the list
		if (bAddClinic && u32ExcludedGUIDCount)
		{
			uint32_t u32Count = u32ExcludedGUIDCount;
			SGUID *psGUID = psExcludedGUIDs;

			// Run through all GUIDs and see if this is excluded or not
			while (u32Count--)
			{
				if (GUIDCompare(&psClinic->sClinicGUID, 
								psGUID))
				{
					bAddClinic = false;
					break;
				}

				psGUID++;
			}
		}

		// Only add this to the list if we want it
		if (bAddClinic)
		{
			int s32Result;

			s32Result = ListBox_AddString(eClinicHWND, 
										  eClinicName);
		
			if (LB_ERRSPACE == s32Result)
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				goto errorExit;
			}

			BASSERT(s32Result != LB_ERR); 

			// Set this to the internal list ID
			s32Result = ListBox_SetItemData(eClinicHWND, 
											s32Result, 
											psClinic->sClinicGUID.pvUser);
			BASSERT(s32Result != LB_ERR);
		}

		psClinic = psClinic->psNextLink;
	}

errorExit:
	// Set the edit control's EStatus
	UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	// Enable list box redraw now that we're done adding items
	SendMessage(eClinicHWND, 
				WM_SETREDRAW, 
				true, 
				0);
}

// This refreshes the global clinic list
void UIClinicRefreshFullClinicList(HWND eHWND, 
								   int32_t s32WindowsID, 
								   WCHAR *peFilter, 
								   SGUID *psExcludedGUIDs, 
								   uint32_t u32ExcludedGUIDCount, 
								   SEditControlErr *psEditCtrlErr)
{
	UIClinicRefreshList(eHWND, 
						s32WindowsID, 
						ClinicListGetHead(), 
						peFilter, 
						psExcludedGUIDs, 
						u32ExcludedGUIDCount, 
						psEditCtrlErr);
}
