#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/SharedFileStore.h"
#include "Shared/FileType.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UIProgress.h"

#include "UIEEG.h"

static WCHAR sg_eFilePath[MAX_PATH];
static uint64_t sg_u64FileFlags;
static bool sg_bForceDefaultFolder;
static SFileStore *sg_psFileStore = NULL;
static HWND sg_eNotifyHwnd = NULL;

static SEEGCheckboxMap sg_sEEGCheckboxMap[] = {
	//
	//	Control ID							Flag value
	//
	{ IDC_CHECK_WAKEFUL,					EEEGFLAG_WAKEFUL					},
	{ IDC_CHECK_DROWSY,						EEEGFLAG_DROWSY						},
	{ IDC_CHECK_SLEEP,						EEEGFLAG_SLEEP						},
	{ IDC_CHECK_COOPERATION_IN_RECORDING,	EEEGFLAG_COOPERATION_IN_RECORDING	},
	{ IDC_CHECK_EYES_OPEN,					EEEGFLAG_EYES_OPEN					},
	{ IDC_CHECK_EYES_CLOSED,				EEEGFLAG_EYES_CLOSED				},
	{ IDC_CHECK_HYPERVENTILATION,			EEEGFLAG_HYPERVENTILATION			},
	{ IDC_CHECK_PHOTIC_STIMULATION,			EEEGFLAG_PHOTIC_STIMULATION			},

	{ LIST_TERMINATOR },
};

static const SControlToStringID sg_sEEGUploadControlToStringMap[] =
{
	// Labels
	{IDC_STATIC_SELECT_EEG,					ESTRING_SELECT_EEG_FILE},
	{IDC_STATIC_FILE,						ESTRING_FILE},
	{IDC_STATIC_FILENAME,					ESTRING_PLEASE_SELECT_EEG_FILE},
	{IDC_STATIC_SELECT_EEG_PROPERTIES,		ESTRING_SELECT_EEG_PATIENT_PROPERTIES},

	// Checkboxes
	{IDC_CHECK_WAKEFUL,						ESTRING_WAKEFUL},
	{IDC_CHECK_DROWSY,						ESTRING_DROWSY},
	{IDC_CHECK_SLEEP,						ESTRING_SLEEP},
	{IDC_CHECK_COOPERATION_IN_RECORDING,	ESTRING_COOPERATION_IN_RECORDING},
	{IDC_CHECK_EYES_OPEN,					ESTRING_EYES_OPEN},
	{IDC_CHECK_EYES_CLOSED,					ESTRING_EYES_CLOSED},
	{IDC_CHECK_HYPERVENTILATION,			ESTRING_HYPERVENTILATION},
	{IDC_CHECK_PHOTIC_STIMULATION,			ESTRING_PHOTIC_STIMULATION},

	// Buttons
	{IDC_BUTTON_SELECT_EEG,					ESTRING_SELECT_FILE},
	{IDC_BUTTON_UPLOAD,						ESTRING_UPLOAD_EEG},
	{IDCANCEL,								ESTRING_CANCEL},

	// Terminator
	{WINDOW_ID_TERMINATOR}
};

static void HandleSelectEEG(HWND eDialogHwnd)
{
	SEditControlErr sResult;
	WCHAR eTitle[WINDOWS_WCHAR_MAX];
	WCHAR eInitialPath[MAX_PATH];
	OPENFILENAME sOpenFileConfig;
	bool bCancelled = false;
	bool bResult;

	UIErrorEditCtrlInitSuccess(&sResult);

	// First, create the EEG folder location, if we're configured to do so.
	//
	if (sg_bForceDefaultFolder)
	{
		bResult = CreateDirectory(LOCAL_SAVE_EEG_FOLDER, NULL);

		if (false == bResult)
		{
			int32_t s32Result = GetLastError();
		
			if (s32Result != ERROR_ALREADY_EXISTS) // If it already exists, we don't care
			{
				WCHAR eMessage[WINDOWS_MBOX_MAX];

				SyslogWCHAR(L"Error creating folder '%s': %d\n", LOCAL_SAVE_EEG_FOLDER, s32Result);

				ResourceGetString(ESTRING_ERROR_CREATE_EEG_FOLDER, eMessage, ARRAYSIZE(eMessage));
				UIErrorMessageBox(sg_eNotifyHwnd, eMessage);
			}
		}
	}

	sg_eFilePath[0] = L'\0';

	ResourceGetString(ESTRING_TITLE_UPLOAD_EEG, eTitle, ARRAYSIZE(eTitle));
		
	ZERO_STRUCT(sOpenFileConfig);
	sOpenFileConfig.lStructSize = sizeof(sOpenFileConfig);

	// Set parent window
	sOpenFileConfig.hwndOwner = eDialogHwnd;

	// Dialog parameters
	sOpenFileConfig.lpstrFilter = EEG_FILE_FILTER;
	sOpenFileConfig.nMaxFile = ARRAYSIZE(sg_eFilePath);
	sOpenFileConfig.lpstrTitle = &eTitle[0];
	sOpenFileConfig.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
	sOpenFileConfig.lpstrFile = sg_eFilePath;

	if (sg_bForceDefaultFolder)
	{
		wcsncpy_s(eInitialPath, ARRAYSIZE(eInitialPath), LOCAL_SAVE_EEG_FOLDER, _TRUNCATE);
		sOpenFileConfig.lpstrInitialDir = eInitialPath;
	}

	bResult = GetOpenFileName(&sOpenFileConfig);

	if (false == bResult)
	{
		int32_t s32Result;
		
		// On error get the extended error code
		s32Result = CommDlgExtendedError();

		if (0 != s32Result)
		{
			// Return the error code
			UIErrorEditCtrlSetHResult(&sResult, s32Result);
			goto CleanUp;
		}
		else
		{
			// If cancel was clicked, just exit
			goto CleanUp;		
		}
	}

	// Set the dialog's displayed file name and enable the upload button
	SetDlgItemText(eDialogHwnd, IDC_STATIC_FILENAME, &sg_eFilePath[sOpenFileConfig.nFileOffset]);
	EnableWindow(GetDlgItem(eDialogHwnd, IDC_BUTTON_UPLOAD), true);

CleanUp:

	UIErrorHandle(eDialogHwnd, &sResult);
}

static DWORD WINAPI UploadEEGThreadProc(LPVOID pvParam)
{
	SEditControlErr sResult;
	SFieldError *psFieldErrors = NULL;
	SFileStore *psFileStoreAdded = NULL;
	char eFilePathUTF8[MAX_PATH * 4]; // Is this needed for worst-case?  Dunno, better safe than sorry.
	char *peFileClass = NULL;
	char *peFileType = NULL;
	EStatus eStatus;

	UIErrorEditCtrlInitSuccess(&sResult);

	// Convert file name to UTF-8 for the API
	//
	UIWCHARToUTF8(eFilePathUTF8,
				  ARRAYSIZE(eFilePathUTF8),
				  sg_eFilePath);

	// Do the actual upload call
	//
	eStatus = FileStoreUpload(g_eGlobalConnectionHandle,
							  &psFieldErrors,
							  eFilePathUTF8,
							  sg_psFileStore,
							  &psFileStoreAdded,
							  NULL,
							  UIProgressUpdate);

	SYSLOG_SERVER_RESULT("FileStoreUpload", eStatus);
	HANDLE_SERVER_RESULT_FIELD_ERRORS_GOTO(eStatus, psFieldErrors, &sResult, CleanUp);

	(void) FileTypeGetText(psFileStoreAdded->eFileClass,
							psFileStoreAdded->eFileType,
							&peFileClass,
							&peFileType);

	SyslogFunc("Uploaded '%s' - class %u/%s, type = %u/%s\n", 
				psFileStoreAdded->peFilename,
				psFileStoreAdded->eFileClass,
				peFileClass,
				psFileStoreAdded->eFileType,
				peFileType);

CleanUp:

	// Notify the upload dialog's parent that the EEG upload is complete
	//
	PostMessage(sg_eNotifyHwnd, WM_EEG_UPLOAD_COMPLETE, 0, 0);

	// Close the progress window
	//
	UIProgressClose();

	// Free stuff that needs free'ing
	//
	FileStoreDeallocate(&psFileStoreAdded);
	FileStoreDeallocate(&sg_psFileStore);
	FieldErrorDeallocate(&psFieldErrors);

	UIErrorHandle(NULL, &sResult);
	return 0;
}

static void HandleUploadEEG(HWND eDialogHwnd)
{
	SEEGCheckboxMap *psCheckboxItem = sg_sEEGCheckboxMap;

	// Grab the selected file flags with the correpsonding checkbox settings
	//
	sg_u64FileFlags = 0;

	while (psCheckboxItem->s32Control != LIST_TERMINATOR)
	{
		if (BST_CHECKED == IsDlgButtonChecked(eDialogHwnd, psCheckboxItem->s32Control))
		{
			sg_u64FileFlags |= psCheckboxItem->u64Flag;
		}

		psCheckboxItem++;
	}

	// Close the dialog.  We have all the information we need, but we'll do the actual upload elsewhere.
	//
	EndDialog(eDialogHwnd, RESULT_UPLOAD);
}

static void UploadEEGFile(HWND eDialogHwnd, SGUID *psPatientGUID, SGUID *psClinicGUID)
{
	SEditControlErr sResult;
	HANDLE eThreadHandle = NULL;
	
	UIErrorEditCtrlInitSuccess(&sResult);
	
	// Construct the file store structure we'll use for the upload
	//
	sg_psFileStore = (SFileStore*) MemAlloc(sizeof(*sg_psFileStore));

	sg_psFileStore->eFileUse = EFILEUSE_EEG;
	sg_psFileStore->eFileClass = EFILECLASS_EEG;
	sg_psFileStore->u64FileFlags = sg_u64FileFlags;

	GUIDCopy(&sg_psFileStore->sAssociationGUID, psPatientGUID);
	GUIDCopy(&sg_psFileStore->sClinicsAssociatedGUID, psClinicGUID);
	
	UIProgressCreate(eDialogHwnd, ESTRING_UPDATE_UPLOAD_EEG);

	eThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) UploadEEGThreadProc, NULL, 0, NULL);

	if (NULL == eThreadHandle)
	{
		UIErrorEditCtrlSetHResult(&sResult, GetLastError());
		goto CleanUp;
	}

CleanUp:

	UIErrorHandle(eDialogHwnd, &sResult);
}

static void HandleInitEEGUploadDialog(HWND eDialogHwnd)
{
	EStatus eStatus;

	UIObjDialogInit(eDialogHwnd,
					0,
					0,
					ESTRING_UPLOAD_EEG);

	// Disable upload button until an EEG is actually selected
	//
	EnableWindow(GetDlgItem(eDialogHwnd, IDC_BUTTON_UPLOAD), false);

	// Set the file name font to be bold
	//
	SendDlgItemMessage(eDialogHwnd, IDC_STATIC_FILENAME, WM_SETFONT, (WPARAM) UIFontBoldGet(), true);

	eStatus = UIDialogFieldsLocalize(eDialogHwnd,
									 sg_sEEGUploadControlToStringMap);
	BASSERT(ESTATUS_OK == eStatus);
}

static INT_PTR CALLBACK UploadEEGDlgProc(HWND eDialogHwnd, UINT u32Msg, WPARAM wParam, LPARAM lParam)
{
	switch (u32Msg)
	{
		case WM_INITDIALOG:
		{	
			HandleInitEEGUploadDialog(eDialogHwnd);

			return true;
		}

		case WM_COMMAND:
		{
			uint16_t u16CmdControl = LOWORD(wParam);
			uint16_t u16CmdNotify = HIWORD(wParam);

			switch (u16CmdControl)
			{
				case IDCANCEL:
				{
					if (BN_CLICKED == u16CmdNotify)
					{
						EndDialog(eDialogHwnd, RESULT_CANCELLED);
					}

					return true;
				}
			
				case IDC_BUTTON_SELECT_EEG:
				{
					if (BN_CLICKED == u16CmdNotify)
					{
						HandleSelectEEG(eDialogHwnd);
					}

					return true;
				}

				case IDC_BUTTON_UPLOAD:
				{
					if (BN_CLICKED == u16CmdNotify)
					{
						HandleUploadEEG(eDialogHwnd);
					}

					return true;
				}
			}
		}
	}

	return false;
}

void UploadEEG(HWND eDialogHwnd, SGUID *psPatientGUID, SGUID *psClinicGUID, bool bForceDefaultFolder)
{
	INT_PTR eResult;

	BASSERT(psPatientGUID);
	BASSERT(psClinicGUID);
	BASSERT(false == GUIDIsNULL(psPatientGUID));
	BASSERT(false == GUIDIsNULL(psClinicGUID));
	
	// Set the global notify handle so the thread knows who to notify when upload is complete
	//
	sg_eNotifyHwnd = eDialogHwnd;

	// And set whether we want to use the default path for EEGs
	//
	sg_bForceDefaultFolder = bForceDefaultFolder;
	
	eResult = DialogBox(NULL, MAKEINTRESOURCE(IDD_UPLOAD_EEG), eDialogHwnd, UploadEEGDlgProc);
	BASSERT(eResult > 0);

	if (RESULT_UPLOAD == eResult)
	{
		UploadEEGFile(eDialogHwnd, psPatientGUID, psClinicGUID);
	}
}

uint64_t GetLatestEEGTimestamp(SGUID *psPatientGUID, SEditControlErr *psEditCtrlErr)
{
	SFileStore sFileFilter;
	SFileStore *psEEGList = NULL;
	SFileStore *psEEG = NULL;
	SFieldError *psFieldError = NULL;
	uint64_t u64Timestamp = 0;
	EStatus eStatus;

	// Set up the filter to look for EEGs for the given patient
	//
	ZERO_STRUCT(sFileFilter);
	GUIDCopy(&sFileFilter.sAssociationGUID, psPatientGUID);
	sFileFilter.eFileClass = EFILECLASS_EEG;

	eStatus = FileStoreGetList(g_eGlobalConnectionHandle,
							   &psFieldError,
							   &sFileFilter,
							   &psEEGList);

	SYSLOG_SERVER_RESULT("FileStoreGetList", eStatus);
	HANDLE_SERVER_RESULT_FIELD_ERRORS_GOTO(eStatus, psFieldError, psEditCtrlErr, CleanUp);

	// Look through the EEG list and find the most recent.  We're not making assumptions about how the
	// result list is sorted.
	//
	psEEG = psEEGList;

	while (psEEG)
	{
		if (psEEG->u64FileTimestamp > u64Timestamp)
		{
			u64Timestamp = psEEG->u64FileTimestamp;
		}
		
		psEEG = psEEG->psNextLink;
	}

CleanUp:

	FileStoreDeallocate(&psEEGList);

	return u64Timestamp;
}