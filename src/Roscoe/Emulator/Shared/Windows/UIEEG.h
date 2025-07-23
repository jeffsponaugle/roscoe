#ifndef _UIEEG_H_
#define _UIEEG_H_

#include "UIError.h"

typedef struct SEEGCheckboxMap
{
	int32_t s32Control;
	uint64_t u64Flag;
} SEEGCheckboxMap;

typedef enum EEEGDialogResult
{
	RESULT_UNKNOWN = 0,
	RESULT_CANCELLED,
	RESULT_UPLOAD,
} EEEGDialogResult;

// EEG file types for open file dialog.  This should be a resource but the string table doesn't seem to like \0
//
#define EEG_FILE_FILTER (L"EEG Files\0*.DAT;*.EAS;*.EDF\0All Files\0*.*\0\0")

#define LOCAL_SAVE_EEG_FOLDER (L"C:\\EEG RECORDS")

extern void UploadEEG(HWND eDialogHwnd, SGUID *psPatientGUID, SGUID *psClinicGUID, bool bForceDefaultFolder);
extern uint64_t GetLatestEEGTimestamp(SGUID *psPatientGUID, SEditControlErr *psEditCtrlErr);

#endif