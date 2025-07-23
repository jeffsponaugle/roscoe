#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UISystem.h"
#include "Shared/SharedStrings.h"

// This adds a string to a combo or list box
EStatus UIObjComboListBoxAdd(HWND eHWND,
							 bool bListBox,
							 WCHAR *peString, 
							 int32_t s32WindowsID, 
							 SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	int s32Result;

	if (bListBox)
	{
		// Add the string to the list box
		s32Result = ListBox_AddString(eHWND,
									  peString);
		
		// Check for listbox OOM condition
		if (LB_ERRSPACE == s32Result)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			UIErrorEditCtrlSetEStatus(psEditCtrlErr,
									  eStatus);
			goto errorExit;
		}

		// Set the listbox windows ID
		s32Result = ListBox_SetItemData(eHWND,
										s32Result,
										s32WindowsID);
		BASSERT(s32Result != LB_ERR);
	}
	else
	{
		// Add the string to the combo box
		s32Result = ComboBox_AddString(eHWND, 
									   peString);

		// Check for combo-box OOM condition
		if (CB_ERRSPACE == s32Result)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			UIErrorEditCtrlSetEStatus(psEditCtrlErr,
									  eStatus);
			goto errorExit;
		}

		// Set the combobox windows ID
		s32Result = ComboBox_SetItemData(eHWND,
										 s32Result,
										 s32WindowsID);
		BASSERT(s32Result != LB_ERR);
	}

errorExit:
	return(eStatus);
}


// This will select the combo box item that has the same windows ID, returning true
// if found and false if not
bool UIObjComboBoxSelect(HWND eHWND, 
						 int32_t s32WindowsID)
{
	int32_t s32Loop;
	int s32Result;
	int s32ComboBoxCount;

	// Find out how many items are in this combo box
	s32ComboBoxCount = ComboBox_GetCount(eHWND);

	for (s32Loop = 0; s32Loop < s32ComboBoxCount; s32Loop++)
	{
		uint32_t u32ItemWindowsID;

		// Go get the data we need
		u32ItemWindowsID = (uint32_t) ComboBox_GetItemData(eHWND,
														 s32Loop);
		BASSERT(u32ItemWindowsID != CB_ERR);
	
		// IF the ID's match, select it and return
		if (u32ItemWindowsID == s32WindowsID)
		{
			// Found it! Select it
			s32Result = ComboBox_SetCurSel(eHWND,
										   s32Loop);
			BASSERT(s32Result != CB_ERR);
			return(true);
		}
	}

	// Didn't find it
	return(false);
}

// This gets the currently selected combo box item
uint32_t UIObjComboBoxGetSelected(HWND eHWND)
{
	int32_t s32ComboBoxIndex;
	LRESULT eResult;

	// Get the selected index (should always work)
	s32ComboBoxIndex = ComboBox_GetCurSel(eHWND);
	BASSERT(s32ComboBoxIndex != CB_ERR);

	// Get the user provided value
	eResult = ComboBox_GetItemData(eHWND,
								   s32ComboBoxIndex);
	BASSERT(eResult != CB_ERR);

	return((uint32_t) eResult);
}

// This returns a list of internal IDs for selected items
EStatus UIObjMultiSelectedGetList(HWND eHWND, 
								  uint32_t **ppu32ItemListHead, 
								  uint32_t *pu32ItemCount, 
								  SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t *pu32SelectedItemIndex = NULL;
	int s32SelectedCount;
	int32_t s32Loop;

	// Check the basics (item list head should not be populated)
	BASSERT(ppu32ItemListHead);
	BASSERT(NULL == *ppu32ItemListHead);
	BASSERT(pu32ItemCount);

	*pu32ItemCount = 0;
	*ppu32ItemListHead = NULL;
	s32SelectedCount = ListBox_GetSelCount(eHWND);
	BASSERT(s32SelectedCount != LB_ERR);

	// If there are no items, just exit
	if (0 == s32SelectedCount)
	{
		goto errorExit;
	}

	// Allocate some space for the selected items' indexes
	MEMALLOC(pu32SelectedItemIndex, sizeof(*pu32SelectedItemIndex) * s32SelectedCount);

	// Now go get those items' indexes
	(void) ListBox_GetSelItems(eHWND,
							   s32SelectedCount,
							   pu32SelectedItemIndex);

	// Now allocate the real list
	MEMALLOC(*ppu32ItemListHead, sizeof(**ppu32ItemListHead) * s32SelectedCount);

	for (s32Loop = 0; s32Loop < s32SelectedCount; s32Loop++)
	{
		(*ppu32ItemListHead)[s32Loop] = (uint32_t) ListBox_GetItemData(eHWND,
																	 pu32SelectedItemIndex[s32Loop]);
	}

	*pu32ItemCount = s32SelectedCount;

errorExit:
	UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);

	SafeMemFree(pu32SelectedItemIndex);
	return(eStatus);
}

// Boolean -> BST_/checked
static UINT sg_eBSTControl[] =
{
	BST_UNCHECKED,
	BST_CHECKED
};

// This will set or clear a checkbox
void UIObjCheckBoxSet(HWND eHWND, 
					  int32_t s32WindowsID, 
					  bool bSet)
{
	if (bSet)
	{
		bSet = true;
	}

	BASSERT(bSet < (sizeof(sg_eBSTControl) / sizeof(sg_eBSTControl[0])));
	CheckDlgButton(eHWND,
				   s32WindowsID,
				   sg_eBSTControl[bSet]);
}

// This gets a checkbox's state
bool UIObjCheckBoxGet(HWND eHWND,
					  int32_t s32WindowsID)
{
	if (IsDlgButtonChecked(eHWND, s32WindowsID) == BST_CHECKED)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// This is called once per field length max setting (by Windows)
static BOOL CALLBACK UIObjEditControlSetLimits(HWND eHWND,
											   LPARAM s64Unused)
{
	SEditControlErr sErrResult;
	int s32Result;
	WCHAR eClassName[WINDOWS_WCHAR_MAX];
	uint32_t u32Length;

	// Clear out the result
	UIErrorEditCtrlInitSuccess(&sErrResult);

	// Get the class of this controller - we're looking for edit controls only
	s32Result = GetClassName(eHWND,
							 eClassName,
							 ARRAYSIZE(eClassName));

	// This should never fail
	BASSERT(s32Result > 0);

	// If it's not an edit control, then just return OK
	if (_wcsicmp(eClassName,
				 L"edit"))
	{
		// Not an edit control
		goto errorExit;
	}

	u32Length = (uint32_t) GetWindowLongPtr(eHWND,
										  GWLP_USERDATA);
	if (0 == u32Length)
	{
		// Maximum length if field length is 0
		u32Length = ((uint32_t) 1 << 31) - 1;
	}

	// Set the limit for this field
	Edit_LimitText(eHWND,
				   u32Length);

errorExit:
	// Always successful - make Windows happy
	return(TRUE);
}

// Sets an edit control to a specific value from a data type
EStatus UIObjEditControlSet(HWND eHWND, 
							EAtomTypes eType,
							int32_t s32WindowsID, 
							void *pvData,
							uint8_t u8FloatDecimalPlaces)
{
	EStatus eStatus = ESTATUS_OK;
	HWND eControl;
	WCHAR *peBuffer = NULL;
	uint32_t u32Integer;
	double dDouble;
	bool bInteger = false;
	bool bFloat = false;
	bool bBufferAllocated = false;
	int s32Result;

	// Get the control within the dialog
	eControl = GetDlgItem(eHWND,
						  s32WindowsID);

	// If the value is NULL, then just set the edit control to blank
	if (NULL == pvData)
	{
		SetWindowText(eControl,
					  L"");
		goto errorExit;
	}

	switch (eType)
	{
		case EATYPE_STRING:
		{
			// UTF8 String
			int32_t s32Length;

			// Get the length of required buffer for string storage
			s32Length = MultiByteToWideChar(CP_UTF8, 
											0, 
											(LPCSTR) pvData, 
											-1, 
											NULL, 
											0);

			// Also a NULL string, but just set things blank
			if (0 == s32Length)
			{
				SetWindowText(eControl,
							  L"");
				goto errorExit;
			}

			MEMALLOC(peBuffer, sizeof(*peBuffer) * (s32Length + 1));

			// Convert to WCHAR
			MultiByteToWideChar(CP_UTF8,
								0,
								(LPCSTR) pvData,
								-1,
								peBuffer,
								s32Length);

			bBufferAllocated = true;
			break;
		}

		case EATYPE_UINT16:
		{
			u32Integer = *((uint16_t *) pvData);
			bInteger = true;
			break;
		}

		case EATYPE_UINT32:
		{
			u32Integer = *((uint32_t *) pvData);
			bInteger = true;
			break;
		}

		case EATYPE_FLOAT:
		{
			dDouble = *((float *) pvData);
			bFloat = true;
			break;
		}

		case EATYPE_DOUBLE:
		{
			dDouble = *((double *) pvData);
			bFloat = true;
			break;
		}

		default:
		{
			// Unsupported data type
			BASSERT(0);
		}
	}

	if (false == bBufferAllocated)
	{
		// Allocate a temp buffer for setting the control box
		MEMALLOC(peBuffer, sizeof(*peBuffer) * WINDOWS_WCHAR_MAX);

		if (bInteger)
		{
			s32Result = _snwprintf_s(peBuffer, WINDOWS_WCHAR_MAX, _TRUNCATE, L"%u", u32Integer);
		}
		else
		if (bFloat)
		{
			WCHAR eFormat[WINDOWS_WCHAR_MAX];
			WCHAR eDigits[WINDOWS_WCHAR_MAX];

			// Create a WCHAR format string %.xf
			wcsncpy_s(eFormat, ARRAYSIZE(eFormat), L"%.", _TRUNCATE);
			wcsncat_s(eFormat, ARRAYSIZE(eFormat), _itow(u8FloatDecimalPlaces, eDigits, ARRAYSIZE(eDigits)), _TRUNCATE);
			wcsncat_s(eFormat, ARRAYSIZE(eFormat), L"f", _TRUNCATE);

			s32Result = _snwprintf_s(peBuffer, 
									 WINDOWS_WCHAR_MAX, 
									 _TRUNCATE,
									 eFormat, 
									 dDouble);
		}
		else
		{
			// WAT?
			BASSERT(0);
		}

		// Internal rendering failure
		BASSERT(s32Result > 0);
	}

	// Now set the text, whatever it may be
	SetWindowText(eControl,
				  peBuffer);

errorExit:
	SafeMemFree(peBuffer);
	return(eStatus);
}

// This will unhighlight all highlighted labels
void UIObjLabelUnhighlight(HWND eHWND,
						   int32_t *ps32Label)
{
	BASSERT(eHWND);
	BASSERT(ps32Label);

	if (*ps32Label != HIGHLIGHTED_LABEL_NONE)
	{
		HWND eLabel;
		
		// Get the label HWND from the dialog
		eLabel = GetDlgItem(eHWND,
							*ps32Label);

		BASSERT(eLabel);

		// Indicate no label is highlighted
		*ps32Label = HIGHLIGHTED_LABEL_NONE;

		// Invalidate and redraw
		InvalidateRect(eLabel,
					   NULL,
					   true);

		// And update the window
		UpdateWindow(eLabel);
	}
}

// This will run through a list of atoms and set any appropriate fields to their limits
void UIObjEditControlSetFieldLengths(HWND eHWND,
									 SAtomInfo *psAtom)
{
	// This shouldn't be NULL
	BASSERT(psAtom);

	while (psAtom->eAtomID != EATOM_END)
	{
		HWND eControlHWND;

		eControlHWND = GetDlgItem(eHWND,
								  psAtom->sControlInfo.s32ControlID);

		// Only if it's a valid control within this dialog box
		if (eControlHWND)
		{
			SFieldInfo *psFieldInfo;

			psFieldInfo = UISystemLookupFieldInfo(psAtom->eAtomID);

			if (psFieldInfo)
			{
				SetWindowLongPtr(eControlHWND,
								 GWLP_USERDATA,
								 psFieldInfo->u64Length);
			}
		}

		psAtom++;
	}

	// Force Windows to go through and set the limits
	EnumChildWindows(eHWND,
					 UIObjEditControlSetLimits,
					 0);
}


// This will see if there's a field error, and if so, will hightlight that text
bool UIObjEditControlHighlightError(HWND eHWND,
									int32_t *ps32WindowsID,
									SEditControlErr *psEditCtrlErr)
{
	SAtomToTextInfo *psAtom;
	SAtomControlInfo *psAtomEditControl;
	HWND eEditControlHWND;
	HWND eLabelHWND;
	
	// If we don't have a field error, return
	if (psEditCtrlErr->eType != EEDITCTRLERR_FIELD_ERROR)
	{
		return true;
	}

	// Go get the atom info!
	psAtom = AtomToTextGet(psEditCtrlErr->eAtomID);

	// If the atom in question has no text or it has no platform data, return,
	// as we either have no text or associated edit control
	if ((NULL == psAtom) ||
	    (NULL == psAtom->pvPlatformData))
	{
		return true;
	}

	// Point to the atom edit control
	psAtomEditControl = (SAtomControlInfo *) psAtom->pvPlatformData;

	// Gotta get the control and label HWNDs
	eEditControlHWND = GetDlgItem(eHWND,
								  psAtomEditControl->s32ControlID);

	eLabelHWND = GetDlgItem(eHWND,
							psAtomEditControl->s32LabelID);

	// If we don't find the controls, this may indicate we have the wrong parent HWND.
	// Return false to indicate this.
	if ((NULL == eEditControlHWND) || (NULL == eLabelHWND))
	{
		return false;
	}

	// If there's a prior control, then unhighlight it
	UIObjLabelUnhighlight(eHWND,
						  ps32WindowsID);

	// Set the focus
	SetFocus(eEditControlHWND);

	// If it's an edit control, also select the text
	if (psAtomEditControl->bIsEditControl)
	{
		Edit_SetSel(eEditControlHWND,
				    0,
				    -1);
	}

	// Now mark the label that's highlighed
	*ps32WindowsID = psAtomEditControl->s32LabelID;

	// Invalidate and redraw
	InvalidateRect(eLabelHWND,
				   NULL,
				   true);
	UpdateWindow(eLabelHWND);

	return true;
}

// This will update a field with new UTF8 data
EStatus UIObjEditControlGetUTF8(HWND eHWND,
								int32_t s32WindowsID,
								char **ppeField,
								SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus;
	HWND eCtrlHWND;
	uint32_t u32MaxLength;
	int32_t s32FieldLength;
	WCHAR *peNewText = NULL;
	int s32Result;

	// Get the control ID
	eCtrlHWND = GetDlgItem(eHWND,
						   s32WindowsID);
	BASSERT(eCtrlHWND);

	// IF there's existing data, ditch it
	SafeMemFree(*ppeField);

	u32MaxLength = (uint32_t) GetWindowLongPtr(eCtrlHWND,
											 GWLP_USERDATA);
	if (0 == u32MaxLength)
	{
		// Maximum length if field length is 0
		u32MaxLength = ((uint32_t) 1 << 31) - 1;
	}

	s32FieldLength = GetWindowTextLength(eCtrlHWND);
	BASSERT(s32FieldLength >= 0);

	// If the actual text is longer than we allow, truncate it
	if ((uint32_t) s32FieldLength > u32MaxLength)
	{
		s32FieldLength = (int32_t) u32MaxLength;
	}

	// Allocate some space for it
	MEMALLOC(peNewText, sizeof(*peNewText) * (s32FieldLength + 1));

	// Get the existing field
	s32Result = GetWindowText(eCtrlHWND,
							  peNewText,
							  s32FieldLength + 1);
	BASSERT(s32Result >= 0);

	if (0 == s32FieldLength)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// Figure out how long the UTF8 equivalent will be
	s32Result = WideCharToMultiByte(CP_UTF8, 
									0, 
									peNewText, 
									-1, 
									NULL, 
									0, 
									NULL, 
									NULL);

	MEMALLOC(*ppeField, s32Result + 1);

	// Now copy the string from WCHAR to UTF8
	WideCharToMultiByte(CP_UTF8, 
						0, 
						peNewText, 
						-1, 
						(LPSTR) *ppeField, 
						s32Result + 1, 
						NULL, 
						NULL);

	eStatus = ESTATUS_OK;

errorExit:
	SafeMemFree(peNewText);
	UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);
	return(eStatus);
}

// This gets the text from an edit control
bool UIObjEditControlGetWCHAR(HWND eHWND,
							  int32_t s32WindowsID,
							  WCHAR **ppeText)
{
	EStatus eStatus;
	HWND eEditCtrlHWND;
	int32_t s32TextLength;

	// Get the edit control
	eEditCtrlHWND = GetDlgItem(eHWND,
							   s32WindowsID);
	BASSERT(eEditCtrlHWND);

	// Now get the length
	s32TextLength = GetWindowTextLength(eEditCtrlHWND);
	if (0 == s32TextLength)
	{
		*ppeText = '\0';
		return(false);
	}

	// Allocate some space for it
	MEMALLOC(*ppeText, sizeof(**ppeText) * (s32TextLength + 1));

	// Go get the text
	GetWindowText(eEditCtrlHWND,
				  *ppeText,
				  s32TextLength + 1);

	return(true);

errorExit:
	BASSERT(ESTATUS_OK == eStatus);
	return(false);
}

static void UIObjListViewSort(HWND eHWND,
							  uint8_t u8ColumnID,
							  bool bAscending,
							  PFNLVCOMPARE pfnCompare,
							  bool bSortByIndex)
{
	HWND eHeaderHWND;
	HDITEM sHeaderInfo;
	uint32_t u32ColumnCount;
	uint32_t u32Loop;
	uint32_t u32SortFlags = 0;

	ZERO_STRUCT(sHeaderInfo);

	// Get the header info
	eHeaderHWND = ListView_GetHeader(eHWND);
	BASSERT(eHeaderHWND);

	u32ColumnCount = Header_GetItemCount(eHeaderHWND);

	sHeaderInfo.mask = HDI_FORMAT;

	// Loop through each column, and if it's not the column in question, then clear the
	// sort arrows
	for (u32Loop = 0; u32Loop < u32ColumnCount; u32Loop++)
	{
		bool bResult;

		// Go get the header info
		bResult = Header_GetItem(eHeaderHWND,
								 u32Loop,
								 &sHeaderInfo);
		BASSERT(bResult);

		// Set/update the sort errors
		sHeaderInfo.mask = HDI_FORMAT;

		// Clear sort flags
		sHeaderInfo.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

		// If it's our column in question, pay attention 
		if (u8ColumnID == (uint8_t) u32Loop)
		{
			if (bAscending)
			{
				sHeaderInfo.fmt |= HDF_SORTUP;
			}
			else
			{
				sHeaderInfo.fmt |= HDF_SORTDOWN;
			}
		}

		bResult = Header_SetItem(eHeaderHWND,
								 u32Loop,
								 &sHeaderInfo);
		BASSERT(bResult);
	}

	u32SortFlags = (u8ColumnID << 1);

	if (bAscending)
	{
		// Ascending - bit 0 is 0
	}
	else
	{
		// Descending, bit 0 is 1
		u32SortFlags |= 0x01;
	}

	if (bSortByIndex)
	{
		ListView_SortItemsEx(eHWND,
							 pfnCompare,
							 (LPARAM) u32SortFlags);
	}
	else
	{
		ListView_SortItems(eHWND,
						   pfnCompare,
						   (LPARAM) u32SortFlags);
	}
}

// Do a sort on a column on a list view (via parameter)
void UIObjListViewSortByLParam(HWND eHWND, 
							   uint8_t u8ColumnID, 
							   bool bDescending, 
							   PFNLVCOMPARE pfnCompare)
{
	UIObjListViewSort(eHWND,
					  u8ColumnID,
					  (false==bDescending),
					  pfnCompare,
					  true);
}

// Do a sort on a column on a list view (via column index)
void UIObjListViewSortByIndex(HWND eHWND, 
							  uint8_t u8ColumnID, 
							  bool bDescending, 
							  PFNLVCOMPARE pfnCompare)
{
	UIObjListViewSort(eHWND,
					  u8ColumnID,
					  (false==bDescending),
					  pfnCompare,
					  true);
}

// This initializes a list view
void UIObjListViewInit(HWND eHWND,
					   SColumnDef *psColumnDefinition)
{
	WCHAR eBuffer[WINDOWS_WCHAR_MAX];
	uint8_t u8Index = 0;
	LVCOLUMN sColumn;
	
	// Better not be NULL
	BASSERT(psColumnDefinition);
	
	ZERO_STRUCT(sColumn);
	sColumn.mask = LVCF_TEXT | LVCF_WIDTH;

	// Loop until we're done with our list columns
	while (psColumnDefinition->u8Column != COLUMNDEF_TERMINATOR)
	{
		sColumn.cx = psColumnDefinition->u16Width;

		ResourceGetString(psColumnDefinition->eStringID, 
						  eBuffer, 
						  ARRAYSIZE(eBuffer));

		sColumn.pszText = eBuffer;

		ListView_InsertColumn(eHWND, 
							  u8Index, 
							  &sColumn);

		u8Index++;
		psColumnDefinition++;
	}

	// Let Windows know about it
	SendMessage(eHWND, 
				LVM_SETEXTENDEDLISTVIEWSTYLE, 
				0, 
				LVS_EX_FULLROWSELECT);
}

// Gets the index of the selected list view item
bool UIObjListViewGetSelectedIndex(HWND eHWND,
								   int32_t *ps32Index)
{
	int s32Count;

	// Figure out how many items we have in this list view
	s32Count = ListView_GetItemCount(eHWND);
	BASSERT(s32Count >= 0);

	// Figure out which is elected (if any)
	while (s32Count)
	{
		int s32Result;

		s32Result =	ListView_GetItemState(eHWND,
										  s32Count-1,
										  LVIS_SELECTED);

		if (s32Result & LVIS_SELECTED)
		{
			break;
		}

		s32Count--;
	}

	// Index not found
	if (0 == s32Count)
	{
		return(false);
	}

	// Found it!
	if (ps32Index)
	{
		*ps32Index = s32Count-1;
	}

	return(true);
}

// This returns a basic data type from a list box into binary
bool UIObjEditControlGetValue(HWND eHWND,
							  EAtomTypes eType,
							  int32_t s32WindowsID,
							  bool bEmptyIsZero,
							  void *pvDataDestination,
							  uint8_t u8DataSize)
{
	HWND eControl;
	WCHAR eBuffer[WINDOWS_WCHAR_MAX];
	
	// Get the actual dialog item
	eControl = GetDlgItem(eHWND,
						  s32WindowsID);
	BASSERT(eControl);

	// If it's zero, then make it zero
	if ((bEmptyIsZero) &&
		(GetWindowTextLength(eControl) == 0))
	{
		memset((void *) pvDataDestination, 0, u8DataSize);
		return(true);
	}

	// Go get the window text
	if (GetWindowText(eControl,
					  eBuffer,
					  ARRAYSIZE(eBuffer)) == 0)
	{
		// No data there
		return(false);
	}

	switch (eType)
	{
		case EATYPE_FLOAT:
		{
			double dDouble;

			if (0 == swscanf(eBuffer,
							 L"%lf",
							 &dDouble))
			{
				return(false);
			}

			// Loss of precision is OK
			*((float *) pvDataDestination) = (float) dDouble;
			break;
		}

		case EATYPE_DOUBLE:
		{
			if (0 == swscanf(eBuffer,
							 L"%lf",
							 ((double *) pvDataDestination)))
			{
				return(false);
			}
			break;
		}

		case EATYPE_INT16:
		{
			int32_t s32Data;

			if (0 == swscanf(eBuffer,
							 L"%d",
							 &s32Data))
			{
				return(false);
			}

			*((int16_t *) pvDataDestination) = (int16_t) s32Data;
			break;
		}

		case EATYPE_INT32:
		{
			if (0 == swscanf(eBuffer,
							 L"%d",
							 ((int32_t *) pvDataDestination)))
			{
				return(false);
			}
			break;
		}
				
		case EATYPE_UINT32:
		{
			if (0 == swscanf(eBuffer,
							 L"%u",
							 ((uint32_t *) pvDataDestination)))
			{
				return(false);
			}
			break;
		}

		default:
		{
			// WTF? Not supported
			BASSERT(0);
		}
	}

	return(true);
}

// Equivalent of ListBox_FindItemData that finds the list item via the internal index
bool UIObjListBoxGetIndexByObjectID(HWND eHWND,
									uint32_t u32ObjectID,
									uint32_t *pu32Index)
{
	int32_t s32Count;

	s32Count = ListBox_GetCount(eHWND);

	// Run through
	while (s32Count--)
	{
		LRESULT eWindowsValue;

		eWindowsValue = ListBox_GetItemData(eHWND,
											s32Count);

		if (eWindowsValue == u32ObjectID)
		{
			if (pu32Index)
			{
				*pu32Index = (uint32_t) s32Count;
			}
			return(true);
		}
	}

	return(false);
}

// This sets up a dialog's title (and test mode text if applicable)
void UIObjDialogInit(HWND eHWND,
					 uint16_t u16Width,
					 uint16_t u16Height,
					 EStringID eDialogStringID)
{
	TITLEBARINFO sTitleBar;
	HICON eIcon;
	WCHAR eDialogTitle[WINDOWS_WCHAR_MAX];

	// How high (in pixels) is the title bar?
	ZERO_STRUCT(sTitleBar);
	sTitleBar.cbSize = sizeof(sTitleBar);
	GetTitleBarInfo(eHWND,
				    &sTitleBar);


	// Set the dialog size
	if ((u16Width > 0) ||
		(u16Height > 0))
	{
		bool bResult;

		bResult = SetWindowPos(eHWND, 
							   HWND_NOTOPMOST,
							   0, 
							   0, 
							   u16Width, 
							   u16Height + (sTitleBar.rcTitleBar.bottom - sTitleBar.rcTitleBar.top), 
							   SWP_NOMOVE | SWP_NOZORDER);

		BASSERT(bResult);
	}

	// Set title and icon
	eIcon = LoadIcon(GetModuleHandle(NULL), 
					 MAKEINTRESOURCE(IDI_APPICONS));
	BASSERT(eIcon);

	SendMessage(eHWND, WM_SETICON, ICON_SMALL, (LPARAM) eIcon);
	SendMessage(eHWND, WM_SETICON, ICON_BIG, (LPARAM) eIcon);

	ResourceGetString(eDialogStringID, 
					  eDialogTitle, 
					  ARRAYSIZE(eDialogTitle));

	// Add the "test mode" string if we're in test mode
	if (SharedIsInTestMode())
	{
		WCHAR eTitleSuffix[WINDOWS_WCHAR_MAX];

		ResourceGetString(ESTRING_TITLE_SUFFIX_TESTMODE, 
						  eTitleSuffix, 
						  ARRAYSIZE(eTitleSuffix));

		wcsncat_s(eDialogTitle,
				  ARRAYSIZE(eDialogTitle),
				  eTitleSuffix,
				  _TRUNCATE);
	}

	SetWindowText(eHWND, 
				  eDialogTitle);
}

// This centers a dialog in a parent window
void UIObjDialogCenter(HWND eDialogHWND,
					   HWND eParentWND)
{
	RECT sDialogRect;
	RECT sParentRect;
	int32_t s32Result;
	bool bResult;
	uint32_t u32DialogWidth;
	uint32_t u32DialogHeight;
	uint32_t u32ParentWidth;
	uint32_t u32ParentHeight;
	uint32_t u32StartX;
	uint32_t u32StartY;

	// Get the dialog window's dimensions
	ZERO_STRUCT(sDialogRect);
	s32Result = GetWindowRect(eDialogHWND, 
							  &sDialogRect);
	BASSERT(s32Result);

	// And the parent window's dimensions
	s32Result = GetWindowRect(eParentWND, &sParentRect);
	BASSERT(s32Result);

	u32ParentWidth = sParentRect.right - sParentRect.left;
	u32DialogWidth = sDialogRect.right - sDialogRect.left;

	u32ParentHeight = sParentRect.bottom - sParentRect.top;
	u32DialogHeight = sDialogRect.bottom - sDialogRect.top;
	
	u32StartX = sParentRect.left + (u32ParentWidth >> 1) - (u32DialogWidth >> 1);
	u32StartY = sParentRect.top + (u32ParentHeight >> 1) - (u32DialogHeight >> 1);

	bResult = MoveWindow(eDialogHWND, 
						 u32StartX, 
						 u32StartY, 
						 u32DialogWidth, 
						 u32DialogHeight, 
						 true);
	BASSERT(bResult);
}
