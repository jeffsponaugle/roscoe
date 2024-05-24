#include <stdio.h>
#include "Shared/Shared.h"
#include "Shared/SharedStrings.h"
#include "Shared/Windows/UIMisc.h"
#include "UIError.h"
#include "UIObjects.h"
#include "UIUser.h"
#include "Shared/CmdLine.h"

#define	WCHAR_BUFFER_SIZE	8192

// User login related
WCHAR g_eUserMessageToDisplay[WINDOWS_WCHAR_MAX];
bool g_bUserMessageDisplay;

// This will syslog a WCHAR and do WCHAR->UTF8 translation
void SyslogWCHAR(WCHAR *peFormat, 
				 ...)
{
	EStatus eStatus;
	va_list ap;
	WCHAR *peWide = NULL;
	char *peUTF8 = NULL;

	MEMALLOC(peWide, sizeof(*peWide) * WCHAR_BUFFER_SIZE);
	MEMALLOC(peUTF8, sizeof(*peUTF8) * WCHAR_BUFFER_SIZE);

	va_start(ap, peFormat);
	_vsnwprintf(peWide, WCHAR_BUFFER_SIZE - 1, peFormat, ap);
	va_end(ap);	

	UIWCHARToUTF8(peUTF8,
				  WCHAR_BUFFER_SIZE,
				  peWide);
	SyslogFunc(peUTF8);

errorExit:
	SafeMemFree(peWide);
	SafeMemFree(peUTF8);
}

// Go get a resource's string
void ResourceGetString(EStringID eStringID,
					   WCHAR *peString,
					   uint32_t u32MaxCharacterLength)
{
	char* peStringUTF8 = NULL;
	EStatus eStatus;

	// Get the shared string
	eStatus = SharedStringsGet(eStringID,
							   &peStringUTF8);

	// If we fail to get the string, try again in English explicitly
	if (eStatus != ESTATUS_OK)
	{
		eStatus = SharedStringsGetByLanguage(ELANG_ENGLISH,
											 eStringID,
											 &peStringUTF8);
	}

	if (eStatus != ESTATUS_OK)
	{
		char eNumber[10];
		
		// If we fail, replace it with the string ID so it's easy to identify
		sprintf(eNumber, "%u ", (uint32_t) eStringID);
		
		UIUTF8ToWCHAR(peString,
					  u32MaxCharacterLength,
					  eNumber);

		SyslogFunc("Failed to find string ID '%u', error %s\n",
				   eStringID,
				   GetErrorText(eStatus));
	}
	else
	{
		UIUTF8ToWCHAR(peString,
					  u32MaxCharacterLength,
					  peStringUTF8);
	}
}

// Convert textual GUID to WCHAR
void GUIDToWCHAR(SGUID *psGUID, 
				 WCHAR *peWCHARGUID, 
				 uint32_t u32Length)
{
	char eGUIDString[sizeof(psGUID->u8GUID) + 1];

	SharedGUIDToASCII(psGUID,
					  eGUIDString,
					  sizeof(eGUIDString));

	mbstowcs(peWCHARGUID,
			 eGUIDString,
			 u32Length);
}


// Convert RTCGet() time (in milliseconds) to time_t (seconds)
time_t ConvertTimestampToTimet(uint64_t u64Timestamp)
{
	return((time_t) ((u64Timestamp + 500) / 1000));
}

// This will get formatted time in WCHAR characters
WCHAR *TimeGetFormattedWCHAR(time_t eTime,
							 bool bTime,
							 bool bLocal,
							 WCHAR *peOutputStringTime,
							 uint32_t u32Length)
{
	struct tm *psTime = NULL;
	int eResult;

	// Figure out what our psTime structure should be
	if (bLocal)
	{
		// Local time
		psTime = localtime(&eTime);
	}
	else
	{
		// UTC
		psTime = gmtime(&eTime);
	}

	// Output the date string
	eResult = UIWCHARFormat(peOutputStringTime, 
							u32Length, 
							L"%02u-%02u-%04u", 
							psTime->tm_mday, 
							psTime->tm_mon + 1, 
							(psTime->tm_year + 1900));

	if (eResult < 0)
	{
		peOutputStringTime = NULL;
		goto errorExit;
	}

	// Should we include the time?
	if (bTime)
	{
		WCHAR eTimeString[12];	// Enough for xx:xx:xx and trailing 0x00

		eResult = UIWCHARFormat(eTimeString, 
								ARRAYSIZE(eTimeString),
								L" %02u:%02u:%02u", 
								psTime->tm_hour, 
								psTime->tm_min, 
								psTime->tm_sec);
		if (eResult < 0)
		{
			peOutputStringTime = NULL;
			goto errorExit;
		}

		UIWCHARCat(peOutputStringTime,
				   u32Length,
				   eTimeString);
	}

errorExit:
	return(peOutputStringTime);
}

// This takes the date in packed form and makes it WCHAR-able
void WindowsPackedDateToWCHAR(uint64_t u64DatePacked,
							  WCHAR *peResult,
							  uint32_t u32ResultLength)
{
	char eDate[11];			// Enough for dd/mm/yyyy

	BASSERT(u64DatePacked);

	snprintf(eDate, sizeof(eDate), "%.2u/%.2u/%.4u", 
				(u64DatePacked & 0xff) + 1,
				((u64DatePacked >> 8) & 0xff) + 1,
				u64DatePacked >> 16);

	mbstowcs(peResult,
			 eDate,
			 u32ResultLength);
}

void SYSTEMTIMEToTimestamp(SYSTEMTIME *psSystemTime,
						   bool bLocal,
						   uint64_t *pu64Timestamp,
						   SEditControlErr *psEditCtrlErr)
{
	struct tm sTimeTM;
	int64_t s64TimeT;

	BASSERT(psSystemTime);
	BASSERT(pu64Timestamp);

	ZeroMemory(&sTimeTM, sizeof(sTimeTM));

	// Convert the SYSTEMTIME components into a struct tm
	sTimeTM.tm_year = psSystemTime->wYear - 1900;
	sTimeTM.tm_mon  = psSystemTime->wMonth - 1;
	sTimeTM.tm_mday = psSystemTime->wDay;
	sTimeTM.tm_hour = psSystemTime->wHour;
	sTimeTM.tm_min  = psSystemTime->wMinute;
	sTimeTM.tm_sec  = psSystemTime->wSecond;

	// Convert the struct tm into a time_t
	if (bLocal)
	{
		s64TimeT = mktime(&sTimeTM);
	}
	else
	{
		s64TimeT = _mkgmtime(&sTimeTM);
	}

	if (s64TimeT < 0)
	{
		UIErrorEditCtrlSetHResult(psEditCtrlErr,
								  ERROR_INVALID_TIME);
		return;
	}

	// The timestamp is basically a time_t, but in milliseconds
	*pu64Timestamp = (s64TimeT * 1000) + psSystemTime->wMilliseconds;
}

// Get the current application's folder
WCHAR *WindowsGetAppFolder(WCHAR *peFolder,
						   uint32_t u32Length)
{
	int s32Result;

	// Get fully qualified path to file
	s32Result = GetModuleFileName(NULL,
								  peFolder,
								  u32Length);
	if (s32Result <= 0)
	{
		goto errorExit;
	}

	// Remove trailing filename/backslash from path if it's present
	if (false == PathRemoveFileSpec(peFolder))
	{
		// That... doesn't make sense.
		goto errorExit;
	}
	
	return(peFolder);

errorExit:
	return(NULL);
}

// This set's a dialog item's value to a packed date
void WindowSetPackedDate(HWND eDialogWindow,
						 int32_t s32ControlID,
						 uint64_t u64PackedDate)
{
	HWND eDateWindow = GetDlgItem(eDialogWindow, s32ControlID);
	WCHAR eDate[WINDOWS_WCHAR_MAX];

	WindowsPackedDateToWCHAR(u64PackedDate, eDate, ARRAYSIZE(eDate));

	SetWindowText(eDateWindow, eDate);
}

// Internal routine for parsing date from a dialog
EStatus UIMiscParseDateFromString(char* peDateString,
								  uint8_t *pu8Day,
								  uint8_t *pu8Month,
								  uint16_t *pu16Year,
								  SEditControlErr *psEditCtrlErr)
{
	int s32Result;
	EStatus eStatus = ESTATUS_OK;
	WCHAR eTrailer[150];
	DWORD u32Day;
	DWORD u32Month;
	DWORD u32Year;
	uint8_t u8Days;

	// Parse the dd/mm/yyyy format
	s32Result = sscanf(peDateString,
					   "%u/%u/%u%s",
					   &u32Day,
					   &u32Month,
					   &u32Year,
					   eTrailer);

	if (3 == s32Result)
	{
		// We got it
	}
	else
	if (1 == s32Result)
	{
		// This means we got the first, but didn't get subsequent. Try the mm-dd-yyyy approach and see if that fits.
		s32Result = sscanf(peDateString, 
						   "%u-%u-%u%s", 
						   &u32Day, 
						   &u32Month, 
						   &u32Year, 
						   eTrailer);
	}

	if (s32Result != 3)
	{
		// No idea what this is, but it's bogus
		if (s32Result >= 4)
		{
			eStatus = ESTATUS_DATE_YEAR_INVALID;
		}
		else
		{
			eStatus = ESTATUS_DATE_DAY_INVALID;
		}
		goto errorExit;
	}

	// First, validate the month
	if ((u32Month > 12) || (0 == u32Month))
	{
		eStatus = ESTATUS_DATE_MONTH_INVALID;
		goto errorExit;
	}

	// Now validate the year. Must be >= 1900
	if ((u32Year < 1900) || (u32Year > ((1 << 16) - 1)))
	{
		eStatus = ESTATUS_DATE_YEAR_INVALID;
		goto errorExit;
	}

	// Get the number of days in this month
	u8Days = SharedDaysInMonth((uint8_t) (u32Month - 1));

	// Now figure out if this is a leap year if it's February
	if (SharedIsLeapYear(u32Year) && (2 == u32Month))
	{
		++u8Days;
	}

	// See if the day is OK
	if (u32Day > u8Days)
	{
		eStatus = ESTATUS_DATE_DAY_INVALID;
		goto errorExit;
	}

	// All good
	if (pu8Day)
	{
		*pu8Day = (uint8_t) u32Day;
	}

	if (pu8Month)
	{
		*pu8Month = (uint8_t) u32Month;
	}

	if (pu16Year)
	{
		*pu16Year = (uint16_t) u32Year;
	}

errorExit:
	UIErrorEditCtrlSetEStatus(psEditCtrlErr,
						  eStatus);

	return( eStatus );
}

// Internal routine for parsing date from a dialog
static void ParseDateFromDialog(HWND eDialogHandle,
								uint32_t u32WindowID,
								uint8_t *pu8Day,
								uint8_t *pu8Month,
								uint16_t *pu16Year,
								SEditControlErr *psEditCtrlErr)
{
	HWND eDateHandle = GetDlgItem(eDialogHandle, u32WindowID);
	int s32Result;
	WCHAR eDateString[150];
	EStatus eStatus;
	WCHAR eTrailer[150];
	DWORD u32Day;
	DWORD u32Month;
	DWORD u32Year;
	uint8_t u8Days;

	s32Result = GetWindowText(eDateHandle,
							  eDateString,
							  sizeof(eDateString));

	if (s32Result <= 0)
	{
		// No text or date handle invalid
		eStatus = ESTATUS_DATE_DAY_INVALID;
		goto errorExit;
	}

	// Parse the dd/mm/yyyy format
	s32Result = swscanf(eDateString,
						L"%u/%u/%u%s",
						&u32Day,
						&u32Month,
						&u32Year,
						eTrailer);

	if (3 == s32Result)
	{
		// We got it
	}
	else
	if (1 == s32Result)
	{
		// This means we got the first, but didn't get subsequent. Try the mm-dd-yyyy approach and see if that fits.
		s32Result = swscanf(eDateString, 
							L"%u-%u-%u%s", 
							&u32Day, 
							&u32Month, 
							&u32Year, 
							eTrailer);
	}

	if (s32Result != 3)
	{
		// No idea what this is, but it's bogus
		if (s32Result >= 4)
		{
			eStatus = ESTATUS_DATE_YEAR_INVALID;
		}
		else
		{
			eStatus = ESTATUS_DATE_DAY_INVALID;
		}
		goto errorExit;
	}

	// First, validate the month
	if ((u32Month > 12) || (0 == u32Month))
	{
		eStatus = ESTATUS_DATE_MONTH_INVALID;
		goto errorExit;
	}

	// Now validate the year. Must be >= 1900
	if ((u32Year < 1900) || (u32Year > ((1 << 16) - 1)))
	{
		eStatus = ESTATUS_DATE_YEAR_INVALID;
		goto errorExit;
	}

	// Get the number of days in this month
	u8Days = SharedDaysInMonth((uint8_t) (u32Month - 1));

	// Now figure out if this is a leap year if it's February
	if (SharedIsLeapYear(u32Year) && (2 == u32Month))
	{
		++u8Days;
	}

	// See if the day is OK
	if (u32Day > u8Days)
	{
		eStatus = ESTATUS_DATE_DAY_INVALID;
		goto errorExit;
	}

	// All good
	if (pu8Day)
	{
		*pu8Day = (uint8_t) u32Day;
	}

	if (pu8Month)
	{
		*pu8Month = (uint8_t) u32Month;
	}

	if (pu16Year)
	{
		*pu16Year = (uint16_t) u32Year;
	}

	// No error
	return;

errorExit:
	UIErrorEditCtrlSetEStatus(psEditCtrlErr,
						  eStatus);
}

// Parses and retrieves a packed date format based on the dialog
uint64_t WindowsGetDialogDatePacked(HWND eDialog,
								  uint32_t u32WindowID)
{
	uint8_t u8Day;
	uint8_t u8Month;
	uint16_t u16Year;
	SEditControlErr sResult;

	UIErrorEditCtrlInitSuccess(&sResult);

	ParseDateFromDialog(eDialog,
						u32WindowID,
						&u8Day,
						&u8Month,
						&u16Year,
						&sResult);

	return(SharedPackDate(u8Day,
						  u8Month,
						  u16Year));
}

// Sets the dialog associated with the window ID 
void WindowsSetDateValid(HWND eDialog,
						 uint32_t u32WindowID,
						 SEditControlErr *psEditCtrlErr)
{
	ParseDateFromDialog(eDialog,
						u32WindowID,
						NULL,
						NULL,
						NULL,
						psEditCtrlErr);
}

// Converts a UTF8 input to a WCHAR array
void UIUTF8ToWCHAR(WCHAR *peWideDestination,
				   uint32_t u32WideDestinationChars,
				   char *peUTF8Source)
{
	if (NULL == peWideDestination)
	{
		return;
	}

	*peWideDestination = L'\0';
	*(peWideDestination + u32WideDestinationChars - 1) = L'\0';

	if (peUTF8Source)
	{
		MultiByteToWideChar(CP_UTF8,
							0,
							(LPCSTR) peUTF8Source,
							-1,
							peWideDestination,
							u32WideDestinationChars - 1); // Stop 1 char short to guarantee we always end with null terminator
	}
}

// Convert wide string to a UTF-8 string
void UIWCHARToUTF8(char *peUTF8Destination,
				   uint32_t u32UTF8DestinationChars,
				   WCHAR *peWideSource)
{
	if (NULL == peUTF8Destination)
	{
		return;
	}

	*peUTF8Destination = L'\0';
	*(peUTF8Destination + u32UTF8DestinationChars - 1) = L'\0';

	if (peWideSource)
	{
		WideCharToMultiByte(CP_UTF8,
							0,
							peWideSource,
							-1,
							peUTF8Destination,
							(u32UTF8DestinationChars - 1) * sizeof(*peUTF8Destination), // Stop 1 char short to guarantee we always end with null terminator
							NULL,
							NULL);
	}
}

// Concatenate a wide string
void UIWCHARCat(WCHAR *peDestination,
				uint32_t u32DestinationChars,
				WCHAR *peSource)
{
	wcsncat_s(peDestination,
			  u32DestinationChars,
			  peSource,
			  _TRUNCATE);
}

// Format a wide string.  Returns number of characters stored (not including null terminator).  Returns 0 if nothing
// is copied, and < 0 on a formatting failure.
int32_t UIWCHARFormat(WCHAR *peDestination,
				    uint32_t u32DestinationChars,
				    WCHAR *peFormat,
				    ...)
{
	va_list ap;
	int32_t s32Result;
	
	va_start(ap, peFormat);

	s32Result = _vsnwprintf_s(peDestination,
							  u32DestinationChars,
							  _TRUNCATE,
							  peFormat,
							  ap);

	va_end(ap);

	return s32Result;
}

// Format a first/middle/last name into last, first middle in WCHAR format
WCHAR *UIFormatNameWCHAR(char *peFirstName,
						 char *peMiddleName,
						 char *peLastName,
						 char *peTag,
						 WCHAR *peNameBuffer,
						 uint32_t u32NameBufferLength)
{
	char eName[255];
	bool bDirty = false;

	eName[0] = '\0';

	if (peLastName)
	{
		strcat(eName, peLastName);
		strcat(eName, ", ");
	}

	if (peFirstName)
	{
		strcat(eName, peFirstName);
		bDirty = true;
	}

	if (peMiddleName)
	{
		if (bDirty)
		{
			strcat(eName, " ");
		}

		strcat(eName, peMiddleName);
	}

	if (peTag)
	{
		if (bDirty)
		{
			strcat(eName, " ");
		}

		strcat(eName, "(");
		strcat(eName, peTag);
		strcat(eName, ")");
	}

	mbstowcs(peNameBuffer,
			 eName,
			 u32NameBufferLength);

	return(peNameBuffer);
}

bool UINameMatchesFilter(char *peFirstName,
						 char *peMiddleName,
						 char *peLastName,
						 WCHAR *peFilter)
{
	WCHAR eFirstName[WINDOWS_WCHAR_MAX];
	WCHAR eMiddleName[WINDOWS_WCHAR_MAX];
	WCHAR eLastName[WINDOWS_WCHAR_MAX];

	memset((void *) eFirstName, 0, sizeof(eFirstName));
	memset((void *) eMiddleName, 0, sizeof(eMiddleName));
	memset((void *) eLastName, 0, sizeof(eLastName));

	if ((NULL == peFilter) ||
		(wcslen(peFilter) == 0))
	{
		// If it's a blank or NULL filter, then 
		return(true);
	}

	// First name
	if (peFirstName)
	{
		UIUTF8ToWCHAR(eFirstName,
					  ARRAYSIZE(eFirstName),
					  peFirstName);

		if (StrStrI(eFirstName,
					peFilter))
		{
			return(true);
		}
	}

	// Middle name
	if (peMiddleName)
	{
		UIUTF8ToWCHAR(eMiddleName,
					  ARRAYSIZE(eMiddleName),
					  peMiddleName);
		if (StrStrI(eMiddleName,
					peFilter))
		{
			return(true);
		}
	}

	// Last name
	if (peLastName)
	{
		UIUTF8ToWCHAR(eLastName,
					  ARRAYSIZE(eLastName),
					  peLastName);
		if (StrStrI(eLastName,
					peFilter))
		{
			return(true);
		}
	}

	return(false);
}

EStatus UIDialogFieldsLocalize(HWND hDialogHandle,
							   const SControlToStringID *psControlToStringList)
{
	const SControlToStringID *psControlToStringItem = NULL;
	WCHAR eString[WINDOWS_WCHAR_MAX];

	psControlToStringItem = psControlToStringList;

	while (psControlToStringItem->u32ControlID != WINDOW_ID_TERMINATOR)
	{
		ResourceGetString(psControlToStringItem->eStringID,
						  eString,
						  ARRAYSIZE(eString));

		SetDlgItemText(hDialogHandle,
					   psControlToStringItem->u32ControlID,
					   eString);

		psControlToStringItem++;
	}

	return ESTATUS_OK;
}

// Bold font
static HFONT sg_eDialogBoldFont = NULL;

// Font size
#define BOLD_FONT_SIZE	(9)

// This initializes the UI components
void UIInit(void)
{
	int32_t s32DialogBoldFontHeight;

	// Load up whatever fonts are needed (see MSDN)
	s32DialogBoldFontHeight = -MulDiv(BOLD_FONT_SIZE, 
									  GetDeviceCaps(GetDC(NULL), 
									  LOGPIXELSY), 
									  72);
	// Now create the bold font
	sg_eDialogBoldFont = CreateFont(s32DialogBoldFontHeight, 
									0, 
									0, 
									0, 
									FW_BOLD, 
									false, 
									false, 
									false, 
									DEFAULT_CHARSET,
									OUT_DEFAULT_PRECIS, 
									CLIP_DEFAULT_PRECIS, 
									DEFAULT_QUALITY, 
									DEFAULT_PITCH, 
									NULL);
	BASSERT(sg_eDialogBoldFont);
}

HFONT UIFontBoldGet(void)
{
	return(sg_eDialogBoldFont);
}

// This will shut down the UI components
void UIShutdown(void)
{
	EStatus eStatus;

	eStatus = PatientListShutdown();
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = ClinicListShutdown();
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = UserListShutdown();
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = SharedMiscShutdown();
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = SharedTreatmentShutdown();
	BASSERT(ESTATUS_OK == eStatus);

	// Get rid of the created font(s)
	if (sg_eDialogBoldFont)
	{
		DeleteObject(sg_eDialogBoldFont);
		sg_eDialogBoldFont = NULL;
	}
}
