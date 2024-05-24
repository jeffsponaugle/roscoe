#ifndef _UIFILE_H_
#define _UIFILE_H_

#include "UIError.h"

typedef struct SUploadFileParams
{
	HWND eDialogHWND;
	WCHAR *peFilePath;
	SGUID sPatientGUID;
} SUploadFileParams;

#define FILE_FILTER_ALL_FILES (L"All Files\0*.*\0\0")

extern void UIUploadFile(HWND eDialogHwnd, 
						 SGUID *psPatientGUID);

#endif