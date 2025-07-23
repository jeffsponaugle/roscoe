#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UITreatment.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UISystem.h"
#include "Shared/SharedMath.h"

// Update the content of the treatment location list combobox given the content of the existing system treatment location list.
// Use only the supplied treatment types.
//
void RefreshTreatmentLocationList(HWND eDialogHwnd, int32_t s32ControlID, uint64_t u64TreatmentTypeMask, SEditControlErr *psEditCtrlErr)
{
	HWND eTreatmentLocationHwnd = GetDlgItem(eDialogHwnd, s32ControlID);
	STreatmentLocation *psTreatmentLocation = NULL;
	WCHAR eTreatmentLocationName[WINDOWS_WCHAR_MAX];
	
	UISystemGetTreatmentLocationList(&psTreatmentLocation, psEditCtrlErr);
	UIERR_FAIL_RETURN(psEditCtrlErr);

	// Empty out the list box
	//
	ComboBox_ResetContent(eTreatmentLocationHwnd);
	
	// Iterate the treatment location list and add each to the combo box
	//
	while (psTreatmentLocation)
	{
		// We'll only add the treatment location to the list if it matches the specified type mask.
		//
		if (psTreatmentLocation->u64TreatmentTypeMask & u64TreatmentTypeMask)
		{
			UIUTF8ToWCHAR(eTreatmentLocationName, 
						  ARRAYSIZE(eTreatmentLocationName),
						  psTreatmentLocation->peTreatmentLocationName);
	
			UIObjComboBoxAdd(eTreatmentLocationHwnd, eTreatmentLocationName, (uint32_t) psTreatmentLocation->eTreatmentLocation, psEditCtrlErr); // Data is treatment location's enum
			UIERR_FAIL_RETURN(psEditCtrlErr);
		}
		
		psTreatmentLocation = psTreatmentLocation->psNextLink;
	}

	// Select the item
	//
	ComboBox_SetCurSel(eTreatmentLocationHwnd, 0);
}

// Select the treatment location in the combobox matching the given treatment location ID.  If not found, select the default item
//
bool SelectTreatmentLocationListItem(HWND eDialogHwnd, int32_t s32ControlID, ETreatmentLocation eTreatmentLocation)
{
	HWND eComboHwnd = GetDlgItem(eDialogHwnd, s32ControlID);
	uint32_t u32Data = 0;
	
	int32_t s32Result = ComboBox_GetCount(eComboHwnd);
	BASSERT(s32Result != CB_ERR);

	while (s32Result--)
	{
		u32Data = (uint32_t) ComboBox_GetItemData(eComboHwnd, s32Result);

		// If the item data matches the treatment location, select it
		//
		if ((ETreatmentLocation) u32Data == eTreatmentLocation)
		{
			ComboBox_SetCurSel(eComboHwnd, s32Result);
			return true;
		}
	}

	// Default to index 0
	//
	ComboBox_SetCurSel(eComboHwnd, 0);
	return false;
}

// Get the treatment location ID of the selected treatment location from the combobox
//
ETreatmentLocation GetSelectedTreatmentLocation(HWND eDialogHwnd, int32_t s32ControlID)
{
	HWND eTreatmentLocationHwnd = GetDlgItem(eDialogHwnd, s32ControlID);
	LRESULT eResult = 0;

	int32_t s32Index = ComboBox_GetCurSel(eTreatmentLocationHwnd);
	BASSERT(s32Index != CB_ERR);

	eResult = ComboBox_GetItemData(eTreatmentLocationHwnd, s32Index);
	BASSERT(eResult != CB_ERR);

	return (ETreatmentLocation) eResult;
}

// Get the treatment location name
//
bool GetTreatmentLocationName(ETreatmentLocation eTreatmentLocation, WCHAR *peName, uint32_t u32Length)
{
	STreatmentLocation *psTreatmentLocation = TreatmentLocationGetHead();

	while (psTreatmentLocation)
	{
		if (psTreatmentLocation->eTreatmentLocation == eTreatmentLocation)
		{
			UIUTF8ToWCHAR(peName, 
						  u32Length,
						  psTreatmentLocation->peTreatmentLocationName); 
			return true;
		}
		
		psTreatmentLocation = psTreatmentLocation->psNextLink;
	}

	return false;
}

