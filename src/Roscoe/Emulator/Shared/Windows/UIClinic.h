#ifndef _UICLINIC_H_
#define _UICLINIC_H_

// Used to denote "Select clinic" item so as to not conflict with others
#define SELECT_CLINIC_ID	0x7fffffff

extern SClinic *UIFindClinicByObjectID(uint32_t u32ObjectID);
extern EStatus UIClinicListRefresh(SEditControlErr *psEditCtrlErr);
extern bool UIClinicIsInAssociatedList(SGUID *psClinicGUID);
extern void SelectActiveClinicDialog(HWND eMainHwnd, 
									 SGUID *psClinicGUID, 
									 SEditControlErr *psEditCtrlErr);
extern void UIClinicRefreshList(HWND eHWND, 
								int32_t s32WindowsID, 
								SClinic *psClinic, 
								WCHAR *peFilter, 
								SGUID *psExcludedGUIDs, 
								uint32_t u32ExcludedGUIDCount, 
								SEditControlErr *psEditCtrlErr);
extern void UIClinicRefreshFullClinicList(HWND eHWND, 
										  int32_t s32WindowsID, 
										  WCHAR *peFilter, 
										  SGUID *psExcludedGUIDs, 
										  uint32_t u32ExcludedGUIDCount, 
										  SEditControlErr *psEditCtrlErr);

#endif
