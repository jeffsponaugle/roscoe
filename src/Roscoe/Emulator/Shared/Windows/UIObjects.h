#ifndef _UIOBJECTS_H_
#define _UIOBJECTS_H_

#include "Shared/Atomizer.h"

extern EStatus UIObjComboListBoxAdd(HWND eHWND,
									bool bListBox,
									WCHAR *peString, 
									int32_t s32WindowsID, 
									SEditControlErr *psEditCtrlErr);

// Combo box
#define	UIObjComboBoxAdd(a, b, c, d) UIObjComboListBoxAdd(a, false, b, c, d)
extern bool UIObjComboBoxSelect(HWND eHWND, 
								int32_t s32WindowsID);
extern uint32_t UIObjComboBoxGetSelected(HWND eHWND);

// List box
#define	UIObjListBoxAdd(a, b, c, d) UIObjComboListBoxAdd(a, true, b, c, d)
extern EStatus UIObjMultiSelectedGetList(HWND eHWND, 
										 uint32_t **ppu32ItemListHead, 
										 uint32_t *pu32ItemCount, 
										 SEditControlErr *psEditCtrlErr);
extern bool UIObjListBoxGetIndexByObjectID(HWND eHWND,
										   uint32_t u32ObjectID,
										   uint32_t *pu32Index);

// List view
extern void UIObjListViewSortByLParam(HWND eHWND, 
									  uint8_t u8ColumnID, 
									  bool bDescending, 
									  PFNLVCOMPARE pfnCompare);
extern void UIObjListViewSortByIndex(HWND eHWND, 
									 uint8_t u8ColumnID, 
									 bool bDescending, 
									 PFNLVCOMPARE pfnCompare);
extern void UIObjListViewInit(HWND eHWND,
							  SColumnDef *psListMap);
extern bool UIObjListViewGetSelectedIndex(HWND eHWND,
										  int32_t *ps32Index);


// Check box related
extern void UIObjCheckBoxSet(HWND eHWND, 
							 int32_t s32WindowsID, 
							 bool bSet);
extern bool UIObjCheckBoxGet(HWND eHWND,
							 int32_t s32WindowsID);

// Edit control related
extern EStatus UIObjEditControlSet(HWND eHWND, 
								   EAtomTypes eType,
								   int32_t s32WindowsID, 
								   void *pvData,
								   uint8_t u8FloatDecimalPlaces);
extern void UIObjEditControlSetFieldLengths(HWND eHWND,
											SAtomInfo *psAtom);
extern bool UIObjEditControlHighlightError(HWND eHWND,
										   int32_t *ps32WindowsID,
										   SEditControlErr *psEditCtrlErr);
extern EStatus UIObjEditControlGetUTF8(HWND eHWND,
									   int32_t s32WindowsID,
									   char **ppeField,
									   SEditControlErr *psEditCtrlErr);
extern bool UIObjEditControlGetWCHAR(HWND eHWND,
									 int32_t s32WindowsID,
									 WCHAR **ppeText);
extern bool UIObjEditControlGetValue(HWND eHWND,
									 EAtomTypes eType,
									 int32_t s32WindowsID,
									 bool bEmptyIsZero,
									 void *pvDataDestination,
									 uint8_t u8DataSize);

#define	UIObjEditControlSetUTF8(a, b, c)				UIObjEditControlSet(a, EATYPE_STRING, b, (void *) c, 0);
#define	UIObjEditControlSetUINT16(a, b, c)				UIObjEditControlSet(a, EATYPE_UINT16, b, (void *) c, 0);
#define	UIObjEditControlSetUINT32(a, b, c)				UIObjEditControlSet(a, EATYPE_UINT32, b, (void *) c, 0);
#define	UIObjEditControlSetFloat(a, b, c)				UIObjEditControlSet(a, EATYPE_FLOAT, b, (void *) c, 0);
#define	UIObjEditControlSetDouble(a, b, c)				UIObjEditControlSet(a, EATYPE_DOUBLE, b, (void *) c, 1);
#define	UIObjEditControlSetDoubleDecimal(a, b, c, d)	UIObjEditControlSet(a, EATYPE_DOUBLE, b, (void *) c, d);

#define UIObjEditControlGetINT16(a, b, c, d)			UIObjEditControlGetValue(a, EATYPE_INT16, b, d, c, sizeof(int16_t))
#define UIObjEditControlGetINT32(a, b, c, d)			UIObjEditControlGetValue(a, EATYPE_INT32, b, d, c, sizeof(int32_t))
#define UIObjEditControlGetDouble(a, b, c, d)			UIObjEditControlGetValue(a, EATYPE_DOUBLE, b, d, c, sizeof(double))

// Dialog related
extern void UIObjDialogInit(HWND eHWND,
							uint16_t u16Width,
							uint16_t u16Height,
							EStringID eDialogStringID);
extern void UIObjDialogCenter(HWND eDialogHWND,
							  HWND eParentWND);

// Misc
extern void UIObjLabelUnhighlight(HWND eHWND,
								  int32_t *pu32Label);


#endif
