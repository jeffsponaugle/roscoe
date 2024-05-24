#ifndef _UIDOCUMENT_H_
#define _UIDOCUMENT_H_

#include "UIError.h"
#include "Shared/SharedFileStore.h"

typedef struct SDocumentStringToFileInfo
{
	EStringID eStringID;
	EFileClass eFileClass;
	EFileUse eFileUse;
} SDocumentStringToFileInfo;

typedef enum EDocumentDialogResult
{
	EDOCRESULT_UNKNOWN = 0,
	EDOCRESULT_CANCELLED,
	EDOCRESULT_UPLOAD,
} EDocumentDialogResult;

// Document file types for open file dialog.  This should be a resource but the string table doesn't seem to like \0
#define DOCUMENT_FILE_FILTER (L"Document Files\0*.PDF;*.DOC;*.DOCX;*.XLS;*.XLSX\0All Files\0*.*\0\0")

extern void UploadDocument(HWND eDialogHwnd,
						   SGUID *psPatientGUID, 
						   SGUID *psClinicGUID);

extern void GetDocumentUseString(EFileClass eFileClass,
								 EFileUse eFileUse,
								 WCHAR *peString,
								 uint32_t u32StringLength);

#endif