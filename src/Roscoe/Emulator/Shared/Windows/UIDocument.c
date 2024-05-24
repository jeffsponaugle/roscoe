#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/SharedFileStore.h"
#include "Shared/FileType.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UIProgress.h"

#include "UIDocument.h"

static WCHAR sg_eFilePath[MAX_PATH];
static EFileUse sg_eFileUse;
static EFileClass sg_eFileClass;
static SFileStore *sg_psFileStore = NULL;
static HWND sg_eNotifyHwnd = NULL;
static bool sg_bFileSelected = FALSE;

static const SDocumentStringToFileInfo sg_sDocumentStringToFileInfoMap[] =
{
	// String ID					File Class					File Use
	{ESTRING_PTSD,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_PTSD},
	{ESTRING_PCL_M,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_PCL_M},
	{ESTRING_PCL_5,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_PCL_5},
	{ESTRING_MMSE,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_MMSE},
	{ESTRING_AUTISM,				EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_AUTISM},
	{ESTRING_CARS,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_CARS},
	{ESTRING_HAM_A,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_HAM_A},
	{ESTRING_HAM_D,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_HAM_D},
	{ESTRING_ALSFS_R,				EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_ALSFS_R},
	{ESTRING_ASI_LITE,				EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_ASI_LITE},
	{ESTRING_CGAS,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_CGAS},
	{ESTRING_GAF,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_GAF},
	{ESTRING_CGI_S,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_CGI_S},
	{ESTRING_CGI_I,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_CGI_I},
	{ESTRING_CGI,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_CGI},
	{ESTRING_PANSS,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_PANSS},
	{ESTRING_PHQ_9,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_PHQ_9},
	{ESTRING_TBI,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_TBI},
	{ESTRING_TINNITUS,				EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_TINNITUS},
	{ESTRING_BCRS,					EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_BCRS},
	{ESTRING_BRIEF_PAIN_INVENTORY,	EFILECLASS_ASSESSMENT,		EFILEUSE_ASSESSMENT_BRIEF_PAIN_INVENTORY},
	
	{ESTRING_TERMINATOR}
};

static const SControlToStringID sg_sDocumentUploadControlToStringMap[] =
{
	// Labels
	{IDC_STATIC_SELECT_DOCUMENT,			ESTRING_SELECT_DOCUMENT_FILE},
	{IDC_STATIC_FILE,						ESTRING_FILE_COLON},
	{IDC_STATIC_FILENAME,					ESTRING_PLEASE_SELECT_DOCUMENT_FILE},
	{IDC_STATIC_SELECT_DOCUMENT_TYPE,		ESTRING_SELECT_DOCUMENT_TYPE},
	{IDC_STATIC_DOCUMENT_TYPE,				ESTRING_DOCUMENT_TYPE_COLON},

	// Buttons
	{IDC_BUTTON_SELECT_FILE,				ESTRING_SELECT_FILE},
	{IDC_BUTTON_UPLOAD,						ESTRING_UPLOAD_DOCUMENT},
	{IDCANCEL,								ESTRING_CANCEL},

	// Terminator
	{WINDOW_ID_TERMINATOR}
};

static void RefreshUploadButtonState(HWND eDialogHwnd)
{
	HWND eComboHwnd = GetDlgItem(eDialogHwnd, IDC_COMBO_DOCUMENT_TYPE);
	HWND eUploadButtonHwnd = GetDlgItem(eDialogHwnd, IDC_BUTTON_UPLOAD);
	uint32_t u32ComboValue;
	bool bEnabled = true;

	// Button is disabled is no file has been selected
	//
	if (false == sg_bFileSelected)
	{
		bEnabled = false;
	}

	// Button is disabled if no file type has been selected
	//
	u32ComboValue = UIObjComboBoxGetSelected(eComboHwnd);

	if (EFILECLASS_UNSPECIFIED == HIWORD(u32ComboValue))
	{
		bEnabled = false;
	}

	if (EFILEUSE_UNSPECIFIED == LOWORD(u32ComboValue))
	{
		bEnabled = false;
	}

	EnableWindow(GetDlgItem(eDialogHwnd, IDC_BUTTON_UPLOAD), bEnabled);
}

static void HandleSelectDocument(HWND eDialogHwnd)
{
	SEditControlErr sResult;
	WCHAR eTitle[WINDOWS_WCHAR_MAX];
	OPENFILENAME sOpenFileConfig;
	bool bCancelled = false;
	bool bResult;

	UIErrorEditCtrlInitSuccess(&sResult);

	sg_eFilePath[0] = L'\0';

	ResourceGetString(ESTRING_TITLE_UPLOAD_DOCUMENT, eTitle, ARRAYSIZE(eTitle));
		
	ZERO_STRUCT(sOpenFileConfig);
	sOpenFileConfig.lStructSize = sizeof(sOpenFileConfig);

	// Set parent window
	sOpenFileConfig.hwndOwner = eDialogHwnd;

	// Dialog parameters
	sOpenFileConfig.lpstrFilter = DOCUMENT_FILE_FILTER;
	sOpenFileConfig.nMaxFile = ARRAYSIZE(sg_eFilePath);
	sOpenFileConfig.lpstrTitle = &eTitle[0];
	sOpenFileConfig.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
	sOpenFileConfig.lpstrFile = sg_eFilePath;

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

	// Set the dialog's displayed file name and refresh the upload button state
	SetDlgItemText(eDialogHwnd, IDC_STATIC_FILENAME, &sg_eFilePath[sOpenFileConfig.nFileOffset]);

	sg_bFileSelected = true;
	RefreshUploadButtonState(eDialogHwnd);

CleanUp:

	UIErrorHandle(eDialogHwnd, &sResult);
}

static DWORD WINAPI UploadDocumentThreadProc(LPVOID pvParam)
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

	// Notify the upload dialog's parent that the document upload is complete
	//
	PostMessage(sg_eNotifyHwnd, WM_DOCUMENT_UPLOAD_COMPLETE, 0, 0);

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

static void HandleUploadDocument(HWND eDialogHwnd)
{
	HWND eComboHwnd = GetDlgItem(eDialogHwnd, IDC_COMBO_DOCUMENT_TYPE);
	uint32_t u32ComboValue;

	// The combo data contains the file class (in the upper 16-bits) and the file use (in the lower 16-bits)
	//
	u32ComboValue = UIObjComboBoxGetSelected(eComboHwnd);

	sg_eFileClass = (EFileClass) HIWORD(u32ComboValue);
	sg_eFileUse = (EFileUse) LOWORD(u32ComboValue);

	// Close the dialog.  We have all the information we need, but we'll do the actual upload after the dialog is closed.
	//
	EndDialog(eDialogHwnd, EDOCRESULT_UPLOAD);
}

static void UploadDocumentFile(HWND eDialogHwnd, SGUID *psPatientGUID, SGUID *psClinicGUID)
{
	SEditControlErr sResult;
	HANDLE eThreadHandle = NULL;
	
	UIErrorEditCtrlInitSuccess(&sResult);
	
	// Construct the file store structure we'll use for the upload
	//
	sg_psFileStore = (SFileStore*) MemAlloc(sizeof(*sg_psFileStore));

	sg_psFileStore->eFileUse = sg_eFileUse;
	sg_psFileStore->eFileClass = sg_eFileClass;

	GUIDCopy(&sg_psFileStore->sAssociationGUID, psPatientGUID);
	GUIDCopy(&sg_psFileStore->sClinicsAssociatedGUID, psClinicGUID);
	
	UIProgressCreate(eDialogHwnd, ESTRING_UPDATE_UPLOAD_DOCUMENT);

	eThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) UploadDocumentThreadProc, NULL, 0, NULL);

	if (NULL == eThreadHandle)
	{
		UIErrorEditCtrlSetHResult(&sResult, GetLastError());
		goto CleanUp;
	}

CleanUp:

	UIErrorHandle(eDialogHwnd, &sResult);
}

static void HandleInitDocumentUploadDialog(HWND eDialogHwnd)
{
	HWND eComboHwnd = GetDlgItem(eDialogHwnd, IDC_COMBO_DOCUMENT_TYPE);
	SEditControlErr sResult;
	const SDocumentStringToFileInfo *psStringToFileInfo = NULL;
	WCHAR eDocumentType[WINDOWS_WCHAR_MAX];
	uint32_t u32EncodedInfo;
	EStatus eStatus;

	UIErrorEditCtrlInitSuccess(&sResult);

	UIObjDialogInit(eDialogHwnd,
					0,
					0,
					ESTRING_UPLOAD_DOCUMENT);

	// Disable upload button until both a document and a type are actually selected
	//
	EnableWindow(GetDlgItem(eDialogHwnd, IDC_BUTTON_UPLOAD), false);

	// Set the file name font to be bold
	//
	SendDlgItemMessage(eDialogHwnd, IDC_STATIC_FILENAME, WM_SETFONT, (WPARAM) UIFontBoldGet(), true);

	eStatus = UIDialogFieldsLocalize(eDialogHwnd,
									 sg_sDocumentUploadControlToStringMap);
	BASSERT(ESTATUS_OK == eStatus);

	// Populate the document type combo box.  The data will have the file class in the upper word and the file use
	// in the lower word.  The first item will be "please select..." with unspecified values.
	//
	ResourceGetString(ESTRING_PLEASE_SELECT_DOCUMENT_TYPE,
					  eDocumentType,
					  ARRAYSIZE(eDocumentType));

	u32EncodedInfo = MAKELONG(EFILECLASS_UNSPECIFIED, 
							  EFILEUSE_UNSPECIFIED),

	UIObjComboBoxAdd(eComboHwnd,
					 eDocumentType,
					 u32EncodedInfo,
					 &sResult);

	UIERR_FAIL_GOTO(&sResult, CleanUp);

	UIObjComboBoxSelect(eComboHwnd,
						u32EncodedInfo);

	// Now on to the real document type items
	psStringToFileInfo = &sg_sDocumentStringToFileInfoMap[0];

	while (psStringToFileInfo->eStringID != ESTRING_TERMINATOR)
	{
		ResourceGetString(psStringToFileInfo->eStringID,
						  eDocumentType,
						  ARRAYSIZE(eDocumentType));
		
		u32EncodedInfo = MAKELONG(psStringToFileInfo->eFileUse,
								  psStringToFileInfo->eFileClass);

		UIObjComboBoxAdd(eComboHwnd,
						 eDocumentType,
						 u32EncodedInfo,
						 &sResult);

		UIERR_FAIL_GOTO(&sResult, CleanUp);			

		psStringToFileInfo++;
	}

CleanUp:

	UIErrorHandle(eDialogHwnd, &sResult);
}

static INT_PTR CALLBACK UploadDocumentDlgProc(HWND eDialogHwnd, UINT u32Msg, WPARAM wParam, LPARAM lParam)
{
	switch (u32Msg)
	{
		case WM_INITDIALOG:
		{	
			HandleInitDocumentUploadDialog(eDialogHwnd);

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
						EndDialog(eDialogHwnd, EDOCRESULT_CANCELLED);
					}

					return true;
				}
			
				case IDC_BUTTON_SELECT_FILE:
				{
					if (BN_CLICKED == u16CmdNotify)
					{
						HandleSelectDocument(eDialogHwnd);
					}

					return true;
				}

				case IDC_COMBO_DOCUMENT_TYPE:
				{
					if (CBN_SELCHANGE == u16CmdNotify)
					{
						RefreshUploadButtonState(eDialogHwnd);
					}

					return true;
				}
				
				case IDC_BUTTON_UPLOAD:
				{
					if (BN_CLICKED == u16CmdNotify)
					{
						HandleUploadDocument(eDialogHwnd);
					}

					return true;
				}
			}
		}
	}

	return false;
}

void UploadDocument(HWND eDialogHwnd, 
					SGUID *psPatientGUID, 
					SGUID *psClinicGUID)
{
	INT_PTR eResult;

	BASSERT(psPatientGUID);
	BASSERT(psClinicGUID);
	BASSERT(false == GUIDIsNULL(psPatientGUID));
	BASSERT(false == GUIDIsNULL(psClinicGUID));
	
	// Set the global notify handle so the thread knows who to notify when upload is complete
	//
	sg_eNotifyHwnd = eDialogHwnd;

	eResult = DialogBox(NULL, MAKEINTRESOURCE(IDD_UPLOAD_DOCUMENT), eDialogHwnd, UploadDocumentDlgProc);
	BASSERT(eResult > 0);

	if (EDOCRESULT_UPLOAD == eResult)
	{
		UploadDocumentFile(eDialogHwnd, psPatientGUID, psClinicGUID);
	}
}

void GetDocumentUseString(EFileClass eFileClass,
						  EFileUse eFileUse,
						  WCHAR *peString,
						  uint32_t u32StringLength)
{
	const SDocumentStringToFileInfo *psStringToFileInfo = NULL;
	EStringID eStringID = ESTRING_TERMINATOR;

	// Is this a known defined type (assessments, basically)?
	//
	psStringToFileInfo = &sg_sDocumentStringToFileInfoMap[0];

	while (psStringToFileInfo->eStringID != ESTRING_TERMINATOR)
	{
		if ((psStringToFileInfo->eFileClass == eFileClass) &&
			(psStringToFileInfo->eFileUse == eFileUse))
		{
			eStringID = psStringToFileInfo->eStringID;
			break;
		}

		psStringToFileInfo++;
	}

	// If it's not found in the list, see if it's another one we know about
	//
	if (ESTRING_TERMINATOR == eStringID)
	{
		switch (eFileUse)
		{
			case EFILEUSE_EEG:				eStringID = ESTRING_EEG;			break;
			case EFILEUSE_PATIENT_PHOTO:	eStringID = ESTRING_PHOTO;			break;
			case EFILEUSE_EEG_REPORT:		eStringID = ESTRING_EEG_REPORT;		break;
			case EFILEUSE_BILLING_REPORT:	eStringID = ESTRING_BILLING_REPORT;	break;
			default:						eStringID = ESTRING_UNKNOWN;		break;
		}
	}

	ResourceGetString(eStringID,
					  peString,
					  u32StringLength);	
}
						  