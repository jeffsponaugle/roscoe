#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Windows/UIError.h"
#include "Shared/Windows/UIUser.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"

static SUser *UIUserGetHead(EUserList eUserList)
{
	SUser *psUserHead = NULL;

	if (EUSERLIST_ALL == eUserList)
	{
		psUserHead = UserListGetHead();
	}
	else
	if (EUSERLIST_ASSOCIATED == eUserList)
	{
		psUserHead = UserAssociatedListGetHead();
	}
	else
	if (EUSERLIST_AUTHORIZERS == eUserList)
	{
		psUserHead = UserAuthorizerListGetHead();
	}
	else
	{
		BASSERT(0);
	}

	return(psUserHead);
}

EStatus UIUserListRefresh(EUserList eUserList,
						  SEditControlErr *psEditCtrlErr,
						  bool bIncludeDisabledUsers)
{
	EStatus eStatus = ESTATUS_OK;
	SUser *psUserHeadNew = NULL;
	SUser *psUser = NULL;
	char *peUserListName;

	// Figure out which list we're working with
	if (EUSERLIST_ALL == eUserList)
	{
		// All users
		eStatus = UsersGet(g_eGlobalConnectionHandle,
						   NULL,
						   NULL,
						   &psUserHeadNew,
						   bIncludeDisabledUsers);
		peUserListName = "Get all users";
	}
	else
	if (EUSERLIST_ASSOCIATED == eUserList)
	{
		// Just associated users
		eStatus = UserGetListAssociated(g_eGlobalConnectionHandle,
										&psUserHeadNew,
										bIncludeDisabledUsers,
										false);
		peUserListName = "Get associated users";
	}
	else
	if (EUSERLIST_AUTHORIZERS == eUserList)
	{
		// Authorizers user list
		eStatus = UserGetAuthorizers(g_eGlobalConnectionHandle,
									 &psUserHeadNew);
		peUserListName = "Get authorizers";
	}
	else
	{
		// Bogus
		BASSERT(0);
	}
	
	// Bail out if we have errors
	if (eStatus != ESTATUS_OK)
	{
		SYSLOG_SERVER_RESULT("UIUserListRefresh",
							 eStatus);
		UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);
		
		goto errorExit;
	}

	// Set up new internal IDs
	psUser = psUserHeadNew;
	while (psUser)
	{
		psUser->sUserGUID.pvUser = (void *) SharedGetID();
		psUser = psUser->psNextLink;
	}

	eStatus = UserListSetLock(true);
	ERR_GOTO();

	if (EUSERLIST_ALL == eUserList)
	{
		UserListReplace(psUserHeadNew);
	}
	else
	if (EUSERLIST_ASSOCIATED == eUserList)
	{
		UserAssociatedListReplace(psUserHeadNew);
	}
	else
	if (EUSERLIST_AUTHORIZERS == eUserList)
	{
		UserAuthorizerListReplace(psUserHeadNew);
	}
	else
	{
		BASSERT(0);
	}

	eStatus = UserListSetLock(false);
	ERR_GOTO();

	SyslogFunc("%s succeeded - %u users\n", peUserListName, UserListGetCount(psUserHeadNew));

errorExit:
	return(eStatus);
}

SUser *UIFindUserByWindowsID(EUserList eUserList,
							 uint32_t u32WindowsID)
{
	SUser *psUser = NULL;
	EStatus eStatus;

	eStatus = UserListSetLock(true);
	ERR_GOTO();

	psUser = UIUserGetHead(eUserList);
	
	// pvUser == Windows ID
	while (psUser)
	{
		if (((uint32_t) psUser->sUserGUID.pvUser) == u32WindowsID)
		{
			break;
		}
		else
		{
			psUser = psUser->psNextLink;
		}
	}

errorExit:
	eStatus = UserListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);

	return(psUser);
}

void UIUserGetAssociatedClinicList(SUserClinicAssociationRecord *psAssoc, 
								   SGUID **ppsGUIDs, 
								   uint32_t *pu32GUIDCount, 
								   SEditControlErr *psEditCtrlErr)
{
	EStatus eStatus = ESTATUS_OK;
	SGUID *psGUID;

	*pu32GUIDCount = UserListGetClinicAssociationCount(psAssoc);
	if (0 == *pu32GUIDCount)
	{
		return;
	}

	MEMALLOC(*ppsGUIDs,
			 *pu32GUIDCount * sizeof(**ppsGUIDs));


	psGUID = *ppsGUIDs;
	while (psAssoc)
	{
		GUIDCopy(psGUID,
				 &psAssoc->sClinicGUID);
		psGUID->pvUser = psAssoc->sClinicGUID.pvUser;

		++psGUID;
		psAssoc = psAssoc->psNextLink;
	}

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		UIErrorEditCtrlSetEStatus(psEditCtrlErr,
							  eStatus);
	}
}

WCHAR *UIUserFormatName(SUser *psUser, 
						WCHAR *peBuffer, 
						uint32_t u32BufferLen)
{
	return(UIFormatNameWCHAR(psUser->peFirstName,
							 psUser->peMiddleName,
							 psUser->peLastName,
							 psUser->peUsername,
							 peBuffer,
							 u32BufferLen));
}
						
WCHAR *UIUserFormatNameNoUsername(SUser *psUser, 
								  WCHAR *peBuffer, 
								  uint32_t u32BufferLen)
{
	return(UIFormatNameWCHAR(psUser->peFirstName,
							 psUser->peMiddleName,
							 psUser->peLastName,
							 NULL,
							 peBuffer,
							 u32BufferLen));
}

bool UIUserMatchesFilter(SUser *psUser,
						 WCHAR *peFilter)
{
	return(UINameMatchesFilter(psUser->peFirstName,
							   psUser->peMiddleName,
							   psUser->peLastName,
							   peFilter));
}

// Refresh the content of the given combo box with the subset of enabled clinicians from the given user list type.
void UIUserClinicianListRefresh(HWND eDialogHWND,
								int32_t s32ControlID,
								EUserList eUserListType,
								SEditControlErr *psResult)
{
	HWND eComboHWND;
	SUser *psUser = NULL;
	WCHAR eBuffer[WINDOWS_WCHAR_MAX];
	EStatus eStatus;

	eComboHWND = GetDlgItem(eDialogHWND, 
							s32ControlID);
	
	// Start the combo content fresh
	ComboBox_ResetContent(eComboHWND);

	// Add the "not specified" item first
	ResourceGetString(ESTRING_NOT_SPECIFIED, 
					  eBuffer, 
					  ARRAYSIZE(eBuffer));

	UIObjComboBoxAdd(eComboHWND,
					 eBuffer,
					 NO_CLINICIAN_SELECTED,
					 psResult);

	UIERR_FAIL_RETURN(psResult);
	
	// Now loop through the user list and add the clinicians
	eStatus = UserListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	psUser = UIUserGetHead(eUserListType);

	while (psUser)
	{
		if ((psUser->u64UserPermissions & EPRIV_USER_CLINICIAN) && psUser->bUserEnabled)
		{
			UIUserFormatNameNoUsername(psUser,
									   eBuffer,
									   ARRAYSIZE(eBuffer));
			
			UIObjComboBoxAdd(eComboHWND,
							 eBuffer,
							 (int32_t) psUser->sUserGUID.pvUser,
							 psResult);

			UIERR_FAIL_GOTO(psResult, errorExit);
		}

		psUser = psUser->psNextLink;
	}

	// Select the "not specified" item
	UIObjComboBoxSelect(eComboHWND,
						NO_CLINICIAN_SELECTED);

errorExit:

	eStatus = UserListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}

// Get the GUID for the selected clinician in the given combo box.  If not found or
// if the "not specified" item is selected, a NULL GUID will be returned.
void UIUserClinicianListGetSelectedGUID(HWND eDialogHWND,
										int32_t s32ControlID,
										EUserList eUserListType,
										SGUID *psGUID)
{
	SUser *psUser = NULL;
	uint32_t u32InternalID;
	EStatus eStatus;
	
	eStatus = UserListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	u32InternalID = UIObjComboBoxGetSelected(GetDlgItem(eDialogHWND,
														s32ControlID));

	psUser = UIFindUserByWindowsID(eUserListType,
								   u32InternalID);

	if (psUser)
	{
		GUIDCopy(psGUID,
				 &psUser->sUserGUID);
	}
	else
	{
		GUIDZero(psGUID);
	}

	eStatus = UserListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}

// Set the selection in the given combo box to the clinician matching the given GUID.  If the GUID
// can't be looked up or doesn't match an item in the combo box, select the "not specified" item.
void UIUserClinicianListSetFromGUID(HWND eDialogHWND,
									int32_t s32ControlID,
									EUserList eUserListType,
									SGUID *psGUID)
{
	HWND eComboHWND;
	SUser *psUser = NULL;
	bool bUserSet = false;
	EStatus eStatus;

	eComboHWND = GetDlgItem(eDialogHWND,
							s32ControlID);

	eStatus = UserListSetLock(true);
	BASSERT(ESTATUS_OK == eStatus);

	psUser = UserListFindByGUID(UIUserGetHead(eUserListType),
								psGUID);

	if (psUser)
	{
		bUserSet = UIObjComboBoxSelect(eComboHWND,
									   (int32_t) psUser->sUserGUID.pvUser);
	}

	if (false == bUserSet)
	{
		UIObjComboBoxSelect(eComboHWND,
							NO_CLINICIAN_SELECTED);
	}

	eStatus = UserListSetLock(false);
	BASSERT(ESTATUS_OK == eStatus);
}
