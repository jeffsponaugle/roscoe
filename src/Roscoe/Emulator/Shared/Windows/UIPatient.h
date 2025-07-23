#ifndef _UIPATIENT_H_
#define _UIPATIENT_H_

#include "Shared/SharedPatient.h"
#include "Shared/Windows/UIError.h"

extern WCHAR *UIPatientGetGenderStringWCHAR(EPatientGender eGender,
											WCHAR *peGenderString,
											uint32_t u32GenderLength);
extern void UIPatientInitializeGenderComboBox(HWND eComboHWND,
											  SEditControlErr *psResult);
extern EPatientGender UIPatientGetSelectedComboBoxGender(HWND eComboHWND);
extern bool UIPatientSelectComboBoxGender(HWND eComboHWND,
								          EPatientGender eGender);
extern WCHAR *UIFormatPatientNameWCHAR(SPatient *psPatient,
									   WCHAR *peNameBuffer,
									   uint32_t u32NameBufferLength);
extern EStatus UIPatientGetAssociatedClinics(SPatientClinicAssociationRecord *psAssociatedClinics,
											 SGUID **ppsDisplayedGUIDs,
											 uint32_t *pu32DisplayedCount,
											 SGUID **ppsHiddenGUIDs,
											 uint32_t *pu32HiddenCount);
extern bool UIPatientMatchesStringWCHAR(SPatient *psPatient,
										WCHAR *peMatchString);
extern EStatus UIPatientRefreshPatientList(bool bSkipUserCheck,
										   bool bAllAssociatedClinics,
										   SEditControlErr *psEditCtrlErr);
extern EStatus UIPatientListFilteredRefresh(HWND eDialog,
											uint32_t u32PatientListControlID,
											uint32_t u32ClinicListControlID,
											uint32_t u32PatientFilterControlID,
											bool bPendingCorticalFilter,
											SEditControlErr *psEditCtrlErr);
#define UIPatientListFilteredRefreshNoPending(a, b, c, d, e) UIPatientListFilteredRefresh(a, b, c, d, false, e)

#endif
