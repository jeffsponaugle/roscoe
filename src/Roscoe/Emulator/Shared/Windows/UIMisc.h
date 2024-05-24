#ifndef _UIMISC_H_
#define _UIMISC_H_

// Windows stuff
#include <Windows.h>
#include <WinUser.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <wchar.h>
#include <time.h>

#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/SharedTreatment.h"
#include "Shared/SharedUpdate.h"
#include "Shared/SharedUser.h"
#include "Shared/SharedPatient.h"
#include "Shared/SharedStrings.h"

#include "Shared/Admin/AdminClinic.h"
#include "Shared/Admin/AdminUser.h"
#include "Shared/Admin/AdminPatient.h"

#ifndef _NO_RESOURCE
#include "resource.h"
#endif
#include "Shared/Windows/ClientSharedResources.h"

// UI defines
#define WINDOWS_WCHAR_MAX					255
#define WINDOWS_UTF8_MAX					(WINDOWS_WCHAR_MAX << 2)
#define WINDOWS_MBOX_MAX					1024
#define WINDOWS_BYTES_PER_PATIENT			20
#define WINDOWS_DATE_CONTROL_LENGTH			10
#define HIGHLIGHTED_LABEL_NONE				-2
#define WINDOWS_UTF8_MAX_PATH				(MAX_PATH * 4)

// Terminator for windows ID list
#define	WINDOW_ID_TERMINATOR				0xffffffff

// Control ID to string ID mappings
typedef struct SControlToStringID
{
	uint32_t u32ControlID;
	EStringID eStringID;
} SControlToStringID;

#define HANDLE_SERVER_RESULT_FIELD_ERRORS(eStatus, psFieldErrors, psEditCtrlErr)	\
{																								\
	if ((eStatus) != ESTATUS_OK)																\
	{																							\
		if (ESTATUS_FIELD_ERROR == (eStatus))													\
		{																						\
			UIErrorEditCtrlSetFieldError((psEditCtrlErr), (psFieldErrors));								\
			FieldErrorDeallocate(&psFieldErrors);												\
		}																						\
		else																					\
		{																						\
			UIErrorEditCtrlSetEStatus((psEditCtrlErr), (eStatus));										\
		}																						\
		return;																					\
	}																							\
}

#define HANDLE_SERVER_RESULT_FIELD_ERRORS_GOTO(eStatus, psFieldErrors, psEditCtrlErr, lblGoto)	\
{																												\
	if ((eStatus) != ESTATUS_OK)																				\
	{																											\
		if (ESTATUS_FIELD_ERROR == (eStatus))																	\
		{																										\
			UIErrorEditCtrlSetFieldError((psEditCtrlErr), (psFieldErrors));												\
			FieldErrorDeallocate(&psFieldErrors);																\
		}																										\
		else																									\
		{																										\
			UIErrorEditCtrlSetEStatus((psEditCtrlErr), (eStatus));														\
		}																										\
		goto lblGoto;																							\
	}																											\
}

#define HANDLE_SERVER_RESULT(eStatus, psEditCtrlErr)			\
{																			\
	if ((eStatus) != ESTATUS_OK)											\
	{																		\
		UIErrorEditCtrlSetEStatus((psEditCtrlErr), (eStatus));						\
		return;																\
	}																		\
}

#define HANDLE_SERVER_RESULT_GOTO(eStatus, psEditCtrlErr, lblGoto)	\
{																					\
	if ((eStatus) != ESTATUS_OK)													\
	{																				\
		UIErrorEditCtrlSetEStatus((psEditCtrlErr), (eStatus));								\
		goto lblGoto;																\
	}																				\
}

typedef struct SColumnDef
{
	uint8_t u8Column;
	EStringID eStringID;
	uint16_t u16Width;
} SColumnDef;

// Terminator for SListColumnMap
//
#define COLUMNDEF_TERMINATOR (0xff)

// Terminator for other lists
//
#define LIST_TERMINATOR (0x7fffffff)

// Map mask settings to corresponding controls
typedef struct SControlMap
{
	uint64_t u64Mask;
	int32_t s32Control;
	bool bEnabled;
} SControlMap;

// Map atoms to info about corresponding controls
//
typedef struct SAtomControlInfo
{
	int32_t s32ControlID;
	int32_t s32LabelID;
	bool bIsEditControl;
} SAtomControlInfo;

typedef struct SAtomInfo
{
	EAtomID eAtomID;
	SAtomControlInfo sControlInfo;
} SAtomInfo;

#include "Shared/Windows/UIError.h"

// Window events
//
#define WM_RETURNTOLOGIN			(WM_APP + 1)
#define WM_SOFTWARE_UPDATE_COMPLETE (WM_APP + 2)
#define WM_PERFORM_SOFTWARE_UPDATE	(WM_APP + 3)
#define WM_EEG_UPLOAD_COMPLETE		(WM_APP + 4)
#define WM_FULLREFRESH				(WM_APP + 5)
#define WM_REFRESHPATIENT			(WM_APP + 6)
#define WM_REFRESH_PATIENT_EEG_LIST (WM_APP + 7)
#define WM_REFRESH_PATIENT_DOC_LIST (WM_APP + 8)
#define WM_REFRESHUSER				(WM_APP + 9)
#define WM_REFRESHCLINIC			(WM_APP + 10)
#define WM_REFRESHTREATMENT			(WM_APP + 11)
#define WM_REFRESHFILES				(WM_APP + 12)
#define WM_REFRESHTREATMENTLOG		(WM_APP + 13)
#define WM_REFRESHJOBS				(WM_APP + 14)
#define WM_REFRESHLASTJOBSTATUS		(WM_APP + 15)
#define WM_DOCUMENT_UPLOAD_COMPLETE	(WM_APP + 16)

extern bool g_bUserMessageDisplay;
extern WCHAR g_eUserMessageToDisplay[WINDOWS_WCHAR_MAX];


extern void SyslogWCHAR(WCHAR *peFormat, 
						...);
extern void ResourceGetString(EStringID eStringID,
							  WCHAR *peString,
							  uint32_t u32MaxCharacterLength);
extern void GUIDToWCHAR(SGUID *psGUID, 
						WCHAR *peWCHARGUID, 
						uint32_t u32Length);
extern time_t ConvertTimestampToTimet(uint64_t u64Timestamp);
extern WCHAR *TimeGetFormattedWCHAR(time_t eTime,
									bool bTime,
									bool bLocal,
									WCHAR *peOutputStringTime,
									uint32_t u32Length);
extern void WindowsPackedDateToWCHAR(uint64_t u64DatePacked,
									 WCHAR *peResult,
									 uint32_t u32ResultLength);
extern void SYSTEMTIMEToTimestamp(SYSTEMTIME *psSystemTime,
								  bool bLocal,
								  uint64_t *pu64Timestamp,
								  SEditControlErr *psEditCtrlErr);
extern WCHAR *WindowsGetAppFolder(WCHAR *peFolder,
								  uint32_t u32Length);
extern void WindowSetPackedDate(HWND eDialogWindow,
								int32_t s32ControlID,
								uint64_t u64PackedDate);
extern EStatus UIMiscParseDateFromString(char* peDateString,
										 uint8_t *pu8Day,
										 uint8_t *pu8Month,
										 uint16_t *pu16Year,
										 SEditControlErr *psEditCtrlErr);
extern uint64_t WindowsGetDialogDatePacked(HWND eDialog,
										 uint32_t u32WindowID);
extern void WindowsSetDateValid(HWND eDialog,
								uint32_t u32WindowID,
								SEditControlErr *psEditCtrlErr);
extern void UIUTF8ToWCHAR(WCHAR *peWideDestination,
						  uint32_t u32WideDestinationChars,
						  char *peUTF8Source);
extern void UIWCHARToUTF8(char *peUTF8Destination,
						  uint32_t u32UTF8DestinationChars,
						  WCHAR *peWideSource);
extern void UIWCHARCat(WCHAR *peDestination,
					   uint32_t u32DestinationChars,
					   WCHAR *peSource);
extern int32_t UIWCHARFormat(WCHAR *peDestination,
						   uint32_t u32DestinationChars,
						   WCHAR *peFormat,
						   ...);
extern WCHAR *UIFormatNameWCHAR(char *peFirstName,
								char *peMiddleName,
								char *peLastName,
								char *peTag,
								WCHAR *peNameBuffer,
								uint32_t u32NameBufferLength);
extern bool UINameMatchesFilter(char *peFirstName,
								char *peMiddleName,
								char *peLastName,
								WCHAR *peFilter);
extern EStatus UIStringReplaceUTF8(HWND eHWND,
								   int32_t s32WindowsID,
								   char **ppeField,
								   SEditControlErr *psEditCtrlErr);
extern EStatus UIDialogFieldsLocalize(HWND hDialogHandle,
									  const SControlToStringID *psControlToStringList);
extern bool SharedIsInTestMode(void);
extern void UIInit(void);
extern void UIShutdown(void);
extern HFONT UIFontBoldGet(void);

#endif
