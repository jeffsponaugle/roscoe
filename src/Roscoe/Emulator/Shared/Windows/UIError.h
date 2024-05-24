#ifndef _UIERROR_H_
#define _UIERROR_H_

#include "Shared/SharedMisc.h"
#include "Shared/SharedStrings.h"

// Handy macros for handling UI errors
#define UIERR_FAIL_RETURN(psEditCtrlErr) { if (UIErrorEditCtrlSuccess(psEditCtrlErr) == false) return; }
#define UIERR_FAIL_GOTO(psEditCtrlErr,label) { if (UIErrorEditCtrlSuccess(psEditCtrlErr) == false) goto label; }

// Edit control type
typedef enum 
{
	EEDITCTRLERR_NONE,				// No error
	EEDITCTRLERR_ESTATUS,			// Regular EStatus (but not ESTATUS_FIELD_ERROR)
	EEDITCTRLERR_FIELD_ERROR,		// EStatus of ESTATUS_FIELD_ERROR (special handling)
	EEDITCTRLERR_HRESULT,			// HRESULT
} EEditControlErrType;

// This is the error state of an edit control
typedef struct SEditControlErr
{
	EEditControlErrType eType;		// Type of error
	EStatus eStatus;				// Will hold EStatus, or supplementary info on a field error
	EAtomID eAtomID;				// For field errors
	HRESULT eHResult;				// For HRESULTS
	WCHAR eCustomErrorMsg[1024];	// Custom error message (if applicable)
} SEditControlErr;

extern bool UIErrorEditCtrlSuccess(SEditControlErr *psEditCtrlErr);
extern void UIErrorEditCtrlSetMessageUTF8(SEditControlErr *psEditCtrlErr, 
										  char *peMessage);
extern void UIErrorEditCtrlSetEStatus(SEditControlErr *psEditCtrlErr, 
									  EStatus eStatus);
extern void UIErrorEditCtrlSetHResult(SEditControlErr *psEditCtrlErr, 
									  HRESULT eHresult);
extern void UIErrorEditCtrlSetSingleField(SEditControlErr *psEditCtrlErr, 
										  EAtomID eAtomID, 
										  EStatus eStatus);
extern void UIErrorEditCtrlSetFieldError(SEditControlErr *psEditCtrlErr, 
										 SFieldError *psFieldError);
extern void UIErrorEditCtrlInitSuccess(SEditControlErr *psEditCtrlErr);
extern bool UIErrorAborted(SEditControlErr *psEditCtrlErr);
extern bool UIErrorFieldError(SEditControlErr *psEditCtrlErr);
extern bool UIErrorHandle(HWND eHWND, 
						  SEditControlErr *psEditCtrlErr);
extern void UIErrorForceLogin(HWND eHWND, 
							  WCHAR *peMessage);
extern bool UIErrorLoginCheck(SEditControlErr *psEditCtrlErr);
extern void UIErrorMessageBox(HWND eHWND,
							  WCHAR *peMessageBox);
extern void UIErrorMessageBoxInfo(HWND eHWND,
								  WCHAR *peMessageBox);
extern void UIErrorMessageBoxResource(HWND eHWND,
									  EStringID eStringID,
									  uint32_t u32Flags);
extern void UIErrorEditCtrlSetMessageWCHAR(SEditControlErr *psEditCtrlErr, 
										   WCHAR *peMessage);
extern void UIErrorHandleLogout(HWND eHWND, 
								SEditControlErr *psEditCtrlErr);
extern void UIErrorSetFieldType(SEditControlErr *psEditCtrlErr, 
								EAtomID eAtomID);
extern WCHAR *UIErrorEditCtrlGetMessageWCHAR(SEditControlErr *psEditCtrlErr, 
											 WCHAR *peMessage, 
											 uint32_t u32MessageLength);
extern void UIErrorEditCtrlSetMessageResource(SEditControlErr *psEditCtrlErr, 
											  EStringID eStringID);
extern void UIErrorForceLoginBackground(HWND eHWND, 
										EStatus eStatus);

#endif
