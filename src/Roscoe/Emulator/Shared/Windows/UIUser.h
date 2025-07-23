#ifndef _UIUSER_H_
#define _UIUSER_H_

#define NO_CLINICIAN_SELECTED (0x7fffffff)

// Types of user lists
typedef enum
{
	EUSERLIST_ALL,
	EUSERLIST_ASSOCIATED,
	EUSERLIST_AUTHORIZERS
} EUserList;

extern EStatus UIUserListRefresh(EUserList eUserList,
								 SEditControlErr *psEditCtrlErr,
								 bool bIncludeDisabledUsers);
extern SUser *UIFindUserByWindowsID(EUserList eUserList,
									uint32_t u32WindowsID);
extern void UIUserGetAssociatedClinicList(SUserClinicAssociationRecord *psAssociatedClinics, 
										  SGUID **ppsGUIDs, 
										  uint32_t *pu32GUIDCount, 
										  SEditControlErr *psEditCtrlErr);
extern WCHAR *UIUserFormatName(SUser *psUser, 
							   WCHAR *peBuffer, 
							   uint32_t u32BufferLen);
extern WCHAR *UIUserFormatNameNoUsername(SUser *psUser, 
										 WCHAR *peBuffer, 
										 uint32_t u32BufferLen);
extern bool UIUserMatchesFilter(SUser *psUser,
								WCHAR *peFilter);
extern void UIUserClinicianListRefresh(HWND eDialogHWND,
									   int32_t s32ControlID,
									   EUserList eUserListType,
									   SEditControlErr *psResult);
extern void UIUserClinicianListGetSelectedGUID(HWND eDialogHWND,
											   int32_t s32ControlID,
											   EUserList eUserListType,
											   SGUID *psGUID);
extern void UIUserClinicianListSetFromGUID(HWND eDialogHWND,
										   int32_t s32ControlID,
										   EUserList eUserListType,
										   SGUID *psGUID);

#endif