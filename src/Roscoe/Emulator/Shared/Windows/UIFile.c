#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/SharedFileStore.h"
#include "Shared/FileType.h"
#include "Shared/Windows/UIMisc.h"
#include "Shared/Windows/UIObjects.h"
#include "Shared/Windows/UIProgress.h"

#include "UIFile.h"

static SUploadFileParams sg_sUploadFileParams;

// Select a file with the common open file dialog
static void SelectFile(HWND eDialogHWND, 
					   WCHAR *peFilePath,
					   uint32_t u32Length,
					   SEditControlErr *psResult)
{
	OPENFILENAME sOpenFileConfig;
	WCHAR eTitle[WINDOWS_WCHAR_MAX];
	bool bCancelled = false;
	bool bResult;

	BASSERT(peFilePath);
	peFilePath[0] = L'\0';

	ResourceGetString(ESTRING_SELECT_DOCUMENT,
					  eTitle,
					  ARRAYSIZE(eTitle));

	// OFN config parameters
	ZERO_STRUCT(sOpenFileConfig);
	sOpenFileConfig.lStructSize = sizeof(sOpenFileConfig);

	sOpenFileConfig.hwndOwner = eDialogHWND;
	sOpenFileConfig.lpstrFilter = FILE_FILTER_ALL_FILES;
	sOpenFileConfig.nMaxFile = u32Length;
	sOpenFileConfig.lpstrTitle = eTitle;
	sOpenFileConfig.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	sOpenFileConfig.lpstrFile = peFilePath;

	// Now open the dialog and we'll handle the result
	bResult = GetOpenFileName(&sOpenFileConfig);

	if (false == bResult)
	{
		int32_t s32Result;

		// On error get the extended error code.  A 0 value means it was just cancelled
		s32Result = CommDlgExtendedError();

		if (s32Result)
		{
			UIErrorEditCtrlSetHResult(psResult, s32Result);
			return;
		}
		else
		{
			UIErrorEditCtrlSetEStatus(psResult, ESTATUS_CANCELLED);
			return;
		}
	}
}

static DWORD WINAPI UploadFileThreadProc(LPVOID pvParam)
{
	SEditControlErr sResult;
	SFileStore sFileStore;
	SFileStore *psFileStore = NULL;
	SFileStore *psFileStoreAdded = NULL;
	SFieldError *psFieldErrors = NULL;
	char eFilePathUTF8[MAX_PATH * 4];
	char *peFileClass = NULL;
	char *peFileType = NULL;
	EStatus eStatus;
	
	UIErrorEditCtrlInitSuccess(&sResult);

	// Convert file name to UTF-8 for the API
	UIWCHARToUTF8(eFilePathUTF8,
				  ARRAYSIZE(eFilePathUTF8),
				  sg_sUploadFileParams.peFilePath);

	// Set up the file store parameters
	ZERO_STRUCT(sFileStore);
	
	sFileStore.eFileUse = EFILEUSE_UNSPECIFIED;
	sFileStore.eFileClass = EFILECLASS_DOCUMENT;

	GUIDCopy(&sFileStore.sAssociationGUID, &sg_sUploadFileParams.sPatientGUID);

	// Do the actual upload call
	//
	eStatus = FileStoreUpload(g_eGlobalConnectionHandle,
							  &psFieldErrors,
							  eFilePathUTF8,
							  &sFileStore,
							  &psFileStoreAdded,
							  NULL,
							  UIProgressUpdate);

	SYSLOG_SERVER_RESULT("FileStoreUpload", eStatus);
	HANDLE_SERVER_RESULT_FIELD_ERRORS_GOTO(eStatus, psFieldErrors, &sResult, errorExit);

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

errorExit:

	// Notify the upload dialog's parent that the document upload is complete
	PostMessage(sg_sUploadFileParams.eDialogHWND, WM_DOCUMENT_UPLOAD_COMPLETE, 0, 0);

	// Close the progress window
	UIProgressClose();

	// Free stuff that needs free'ing
	FileStoreDeallocate(&psFileStoreAdded);
	FieldErrorDeallocate(&psFieldErrors);	
	
	UIErrorHandle(sg_sUploadFileParams.eDialogHWND, &sResult);
	
	return 0;
}

static void UploadFile(HWND eDialogHWND,
					   SGUID *psPatientGUID,
					   WCHAR *peFilePath)
{
	SEditControlErr sResult;
	HANDLE eThreadHandle = NULL;

	UIErrorEditCtrlInitSuccess(&sResult);

	// Create the progress window
	UIProgressCreate(eDialogHWND, ESTRING_UPDATE_UPLOAD_DOCUMENT);

	// Configure and start the upload thread
	ZERO_STRUCT(sg_sUploadFileParams);

	sg_sUploadFileParams.eDialogHWND = eDialogHWND;
	sg_sUploadFileParams.peFilePath = peFilePath;
	
	GUIDCopy(&sg_sUploadFileParams.sPatientGUID, psPatientGUID);

	eThreadHandle = CreateThread(NULL, 
								 0, 
								 (LPTHREAD_START_ROUTINE) UploadFileThreadProc, 
								 NULL, 
								 0, 
								 NULL);

	if (NULL == eThreadHandle)
	{
		UIErrorEditCtrlSetHResult(&sResult, GetLastError());
		goto errorExit;
	}

errorExit:

	UIErrorHandle(eDialogHWND, &sResult);
}

// Select a file to upload associated with the given patient
void UIUploadFile(HWND eDialogHWND, 
				  SGUID *psPatientGUID)
{
	SEditControlErr sResult;
	WCHAR eFilePath[MAX_PATH];

	UIErrorEditCtrlInitSuccess(&sResult);

	// First, show the open dialog to get the file path
	SelectFile(eDialogHWND,
			   eFilePath,
			   ARRAYSIZE(eFilePath),
			   &sResult);

	UIERR_FAIL_GOTO(&sResult, errorExit);

	// Now upload the file
	(void) UploadFile(eDialogHWND,
					  psPatientGUID,
					  eFilePath);
			
errorExit:

	UIErrorHandle(eDialogHWND, &sResult);
}
