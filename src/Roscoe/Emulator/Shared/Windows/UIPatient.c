#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Windows/UIMisc.h"
#include "UIError.h"
#include "UIPatient.h"
#include "UIUser.h"
#include "UIObjects.h"
#include "Shared/Windows/UIPatient.h"
#include "Shared/Windows/UIClinic.h"
#include "Shared/Admin/AdminPatient.h"


// This list must parallel the EPatientGender set of enumerations
static EStringID sg_eGenderIDs[] =
{
	ESTRING_GENDER_UNSPECIFIED,
	ESTRING_GENDER_MALE,
	ESTRING_GENDER_FEMALE,
};

// Returns a WCHAR gender string
WCHAR *UIPatientGetGenderStringWCHAR(EPatientGender eGender,
									 WCHAR *peGenderString,
									 uint32_t u32GenderLengthMaxChars)
{
	BASSERT(eGender < sizeof(sg_eGenderIDs) / sizeof(sg_eGenderIDs[0]));
	ResourceGetString(sg_eGenderIDs[eGender],
					  peGenderString,
					  u32GenderLengthMaxChars);
	return(peGenderString);
}

// Initialize the given combo box with the gender options
void UIPatientInitializeGenderComboBox(HWND eComboHWND,
									   SEditControlErr *psResult)
{
	WCHAR eGenderString[WINDOWS_WCHAR_MAX];
	uint8_t u8Index;

	ComboBox_ResetContent(eComboHWND);

	for (u8Index = 0; u8Index < (sizeof(sg_eGenderIDs) / sizeof(sg_eGenderIDs[0])); u8Index++)
	{
		UIPatientGetGenderStringWCHAR((EPatientGender) u8Index,
									  eGenderString,
									  ARRAYSIZE(eGenderString));
		
		UIObjComboBoxAdd(eComboHWND,
						 eGenderString,
						 (EPatientGender) u8Index,
						 psResult);
		UIERR_FAIL_RETURN (psResult);
	}

	// Start with default (unspecified) item
	ComboBox_SetCurSel(eComboHWND, EGENDER_UNSPECIFIED);
}

// Return the selected gender in the given combo box
EPatientGender UIPatientGetSelectedComboBoxGender(HWND eComboHWND)
{
	int32_t s32Data;
	int32_t s32Index;

	s32Index = ComboBox_GetCurSel(eComboHWND);
	BASSERT(s32Index != CB_ERR);

	s32Data = (int32_t) ComboBox_GetItemData(eComboHWND, s32Index);
	BASSERT(s32Data != CB_ERR);

	return (EPatientGender) s32Data;
}

// Select the given gender in the given combo box.  Return true if specified gender is selected, false otherwise.
bool UIPatientSelectComboBoxGender(HWND eComboHWND,
								   EPatientGender eGender)
{
	EPatientGender eItemGender;
	int32_t s32GenderCount;
	uint8_t u8Index;

	s32GenderCount = ComboBox_GetCount(eComboHWND);
	BASSERT(s32GenderCount != CB_ERR);

	for (u8Index = 0; u8Index < s32GenderCount; u8Index++)
	{
		eItemGender = (EPatientGender) ComboBox_GetItemData(eComboHWND, 
															u8Index);

		// Does it match the given gender.  If so, select it!
		if (eItemGender == eGender)
		{
			ComboBox_SetCurSel(eComboHWND,
							   u8Index);
			return true;
		}
	}

	// No match if we got here - select default item
	ComboBox_SetCurSel(eComboHWND,
					   EGENDER_UNSPECIFIED);
	return false;
}

// This returns a patient's full name in WCHAR format
WCHAR *UIFormatPatientNameWCHAR(SPatient *psPatient,
								WCHAR *peNameBuffer,
								uint32_t u32NameBufferLength)
{
	return(UIFormatNameWCHAR(psPatient->peFirstName,
							 psPatient->peMiddleName,
							 psPatient->peLastName,
							 NULL,
							 peNameBuffer,
							 u32NameBufferLength));
}

// Let's see if this patient appears in this clinic list
static bool UIPatientAssociatedWithClinicIDs(SPatient *psPatient,
											 uint32_t *pu32ClinicIDListPtr,
											 uint32_t u32ClinicCount)
{
	SPatientClinicAssociationRecord *psAssoc = NULL;
	SClinic *psClinic = NULL;
	bool bMatched = false;
	EStatus eStatus;

	BASSERT(psPatient);

	eStatus = ClinicListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	if (0 == u32ClinicCount)
	{
		goto errorExit;
	}
	
	while ((u32ClinicCount--) &&
		   (false == bMatched))
	{
		psClinic = UIFindClinicByObjectID(*pu32ClinicIDListPtr);
		psAssoc = psPatient->psClinics;
		while (psAssoc)
		{
			if (GUIDCompare(&psClinic->sClinicGUID,
							&psAssoc->sClinicGUID))
			{
				bMatched = true;
				break;
			}
			psAssoc = psAssoc->psNextLink;
		}

		++pu32ClinicIDListPtr;
	}

errorExit:

	eStatus = ClinicListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	return(bMatched);
}

// This will create a list of displayed and hidden GUIDs
EStatus UIPatientGetAssociatedClinics(SPatientClinicAssociationRecord *psAssociatedClinics,
									  SGUID **ppsDisplayedGUIDs,
									  uint32_t *pu32DisplayedCount,
									  SGUID **ppsHiddenGUIDs,
									  uint32_t *pu32HiddenCount)
{
	EStatus eStatus = ESTATUS_OK;
	SPatientClinicAssociationRecord *psAssoc = NULL;
	bool bFound = false;
	SGUID *psDisplayed;
	SGUID *psHidden;

	if (pu32DisplayedCount)
	{
		*pu32DisplayedCount = 0;
	}

	if (pu32HiddenCount)
	{
		*pu32HiddenCount = 0;
	}

	// Count up everything
	psAssoc = psAssociatedClinics;
	while (psAssoc)
	{
		if (UIClinicIsInAssociatedList(&psAssoc->sClinicGUID))
		{
			++(*pu32DisplayedCount);
		}
		else
		{
			++(*pu32HiddenCount);
		}

		bFound = true;
		psAssoc = psAssoc->psNextLink;
	}

	// If we don't have antyhing to do, then bail out
	if (false == bFound)
	{
		goto errorExit;
	}

	// Allocate any memory we need for displayed and hidden lists
	if (*pu32DisplayedCount)
	{
		MEMALLOC(*ppsDisplayedGUIDs, sizeof(**ppsDisplayedGUIDs) * *pu32DisplayedCount);
	}

	if (*pu32HiddenCount)
	{
		MEMALLOC(*ppsHiddenGUIDs, sizeof(**ppsHiddenGUIDs) * *pu32HiddenCount);
	}

	// Now we populate the list
	psAssoc = psAssociatedClinics;
	psDisplayed = *ppsDisplayedGUIDs;
	psHidden = *ppsHiddenGUIDs;
	while (psAssoc)
	{
		SGUID *psTarget;

		if (UIClinicIsInAssociatedList(&psAssoc->sClinicGUID))
		{
			psTarget = psDisplayed;
			++psDisplayed;
		}
		else
		{
			psTarget = psHidden;
			++psHidden;
		}

		if (psTarget)
		{
			GUIDCopy(psTarget, &psAssoc->sClinicGUID);
			psTarget->pvUser = psAssoc->sClinicGUID.pvUser;
		}

		psAssoc = psAssoc->psNextLink;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

static void UIPatientListGetFromClinicListbox(HWND eClinicListHWND,
											  SPatient **ppsPatientListPtr,
											  uint32_t u32PatientListSize,
											  uint32_t *pu32PatientCount,
											  SEditControlErr *psEditCtrlErr)
{
	uint32_t *pu32ClinidIDListHead = NULL;
	uint32_t u32ClinicCount = 0;
	SPatient *psPatient = NULL;
	EStatus eStatus;

	// Lock the global patient list
	eStatus = PatientListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	psPatient = PatientListGetHead();
	if (NULL == psPatient)
	{
		// Nothing in the patient list - bail out
		goto errorExit;
	}

	*pu32PatientCount = 0;

	// Get the clinic list based on the clinic list box
	UIObjMultiSelectedGetList(eClinicListHWND, 
							 &pu32ClinidIDListHead, 
							 &u32ClinicCount, 
							 psEditCtrlErr);
	UIERR_FAIL_GOTO(psEditCtrlErr, 
					errorExit);

	// Figure out all patients and populate each that's associated with
	// any clinic in the list box
	while (psPatient)
	{
		// If we don't have any clinics selected, show the patient
		if ((UIPatientAssociatedWithClinicIDs(psPatient, 
											  pu32ClinidIDListHead, 
											  u32ClinicCount)) || 
			(0 == u32ClinicCount))
		{
			// Internal consistency check to ensure we have enough space for the patient
			BASSERT(*pu32PatientCount < u32PatientListSize);

			// Add the patient
			*ppsPatientListPtr = psPatient;
			++ppsPatientListPtr;
			++(*pu32PatientCount);
		}

		psPatient = psPatient->psNextLink;
	}

errorExit:
	// Unlock the global patient list
	eStatus = PatientListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	// Get rid of the clinic list
	SafeMemFree(pu32ClinidIDListHead);
}

// Returns true if peMatchString is a partial match for the patient's name *OR* the patient ID
bool UIPatientMatchesStringWCHAR(SPatient *psPatient,
								 WCHAR *peMatchString)
{
	WCHAR ePatientID[WINDOWS_WCHAR_MAX];

	// No filter, then assume a "match"
	if (NULL == peMatchString)
	{
		return(true);
	}

	// Quick check to see if we have a patient ID match
	UIUTF8ToWCHAR(ePatientID,
				  sizeof(ePatientID),
				  psPatient->pePatientID);

	if (wcscmp(ePatientID,
			   peMatchString) == 0)
	{
		return(true);
	}

	return(UINameMatchesFilter(psPatient->peFirstName,
							   psPatient->peMiddleName,
							   psPatient->peLastName,
							   peMatchString));
}

// This will return a list of all patients or all associated patients
EStatus UIPatientRefreshPatientList(bool bSkipUserCheck,
									bool bAllAssociatedClinics,
									SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	SPatient *psPatientListNewHead = NULL;
	SPatient *psPatient;
	uint32_t u32PatientCount = 0;
	
	if (false == bSkipUserCheck)
	{
		// If the user can see all patients or the app views them all, then return the entire patient list
		if (UserAllClinicsVisible(g_psLoggedInUser))
		{
			eStatus = PatientsGet(g_eGlobalConnectionHandle, 
								  NULL, 
								  NULL, 
								  &psPatientListNewHead);
		}
		else
		{
			// Only associated patients
			eStatus = PatientGetListAssociated(g_eGlobalConnectionHandle, 
											   &psPatientListNewHead, 
											   true);
		}
	}
	else
	{
		// We're skipping the user check. Just get the associated list
		eStatus = PatientGetListAssociated(g_eGlobalConnectionHandle, 
										   &psPatientListNewHead, 
										   bAllAssociatedClinics);
	}

	if (eStatus != ESTATUS_OK)
	{
		UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);

		ERR_SYSLOG_GOTO("Failed to get patient list/associated");
	}

	// We have a new list!
	eStatus = PatientListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	PatientListReplace(psPatientListNewHead);

	// Give these users new IDs
	psPatient = psPatientListNewHead;
	while (psPatient)
	{
		psPatient->sPatientGUID.pvUser = (void *) SharedGetID();
		++u32PatientCount;
		psPatient = psPatient->psNextLink;
	}

	// Unlock the list
	eStatus = PatientListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	SyslogFunc("Succeded - %u patient(s) loaded\n", u32PatientCount);

errorExit:
	return(eStatus);
}

// Update the patient list box with the context of the clinic list and patient filter. All patients matching
// the existing filter will be shown (further restrictions possible with the bPendingCorticalFilter flag).
EStatus UIPatientListFilteredRefresh(HWND eDialog,
									 uint32_t u32PatientListControlID,
									 uint32_t u32ClinicListControlID,
									 uint32_t u32PatientFilterControlID,
									 bool bPendingCorticalFilter,
									 SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	HWND ePatientHWND;
	HWND eClinicHWND;
	HWND eFilterHWND;
	uint32_t u32PatientCount;
	SPatient **ppsPatientListHead = NULL;
	SPatient **ppsPatientListPtr = NULL;
	int s32Result;
	WCHAR *peFilter = NULL;
	bool bPatientListLocked = false;
	uint32_t u32DisplayedPatientCount = 0;

	// Get the individual window handles for each control type
	ePatientHWND = GetDlgItem(eDialog,
							  u32PatientListControlID);
	eClinicHWND = GetDlgItem(eDialog,
							 u32ClinicListControlID);
	eFilterHWND = GetDlgItem(eDialog,
							 u32PatientFilterControlID);

	eStatus = PatientListSetLock(true);
	ERR_GOTO();

	bPatientListLocked = true;

	// Wipe out the patient list box
	ListBox_ResetContent(ePatientHWND);

	u32PatientCount = PatientListGetCount();
	if (0 == u32PatientCount)
	{
		// No patients
		goto errorExit;
	}

	// Let Windows know how many patients we have coming
	SendMessage(ePatientHWND, 
				LB_INITSTORAGE, 
				u32PatientCount, 
				u32PatientCount * WINDOWS_BYTES_PER_PATIENT);
	SendMessage(ePatientHWND, 
			    WM_SETREDRAW, 
				false, 
				0);

	// Allocate list for all patients
	MEMALLOC(ppsPatientListHead, 
			 sizeof(**ppsPatientListHead) * u32PatientCount);

	// Get the patient list
	UIPatientListGetFromClinicListbox(eClinicHWND, 
									  ppsPatientListHead, 
									  u32PatientCount, 
									  &u32DisplayedPatientCount, 
									  psEditCtrlErr);

	// Get the patient filter (if there is one)
	s32Result = GetWindowTextLength(eFilterHWND);

	// If we have a filter then go get its string
	if (s32Result > 0)
	{
		MEMALLOC(peFilter, 
				 (s32Result + 1) * sizeof(*peFilter));
		GetWindowText(eFilterHWND, 
					  peFilter, 
					  s32Result + 1);
	}

	// Add the filtered items to the master list
	ppsPatientListPtr = ppsPatientListHead;

	while (u32DisplayedPatientCount--)
	{
		bool bMatchedName;
		bool bMatchedPending;
		SPatient *psPatient;
		WCHAR ePatientName[255];

		bMatchedName = true;
		bMatchedPending = true;
		psPatient = *ppsPatientListPtr;

		// Figure out if the patient should be displayed
		bMatchedName = UIPatientMatchesStringWCHAR(psPatient,
												   peFilter);

		

		if (false == bPendingCorticalFilter)
		{
			bMatchedPending = true;
		}
		else
		{
			bMatchedPending = (psPatient->u32PatientStatus & PATIENTSTAT_TREATMENT_SETTINGS_PENDING) && bPendingCorticalFilter;
		}

		if ((bMatchedName) && (bMatchedPending))
		{
			// Get a WCHAR version of the patient's name
			UIFormatPatientNameWCHAR(psPatient, 
									 ePatientName,
									 sizeof(ePatientName));

			UIObjListBoxAdd(ePatientHWND, 
						   ePatientName, 
						   (uint32_t) psPatient->sPatientGUID.pvUser, 
						   psEditCtrlErr);

			UIERR_FAIL_GOTO(psEditCtrlErr,
							errorExit);
		}

		++ppsPatientListPtr;
	}

errorExit:
	if (bPatientListLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = PatientListSetLock(false);
		}
		else
		{
			(void) PatientListSetLock(false);
		}
	}

	SafeMemFree(ppsPatientListHead);
	SafeMemFree(peFilter);

	// Redraw the listbox
	SendMessage(ePatientHWND,
				WM_SETREDRAW,
				true,
				0);

	return(eStatus);
}
