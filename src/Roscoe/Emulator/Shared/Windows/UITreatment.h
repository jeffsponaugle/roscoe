#ifndef _UITREATMENT_
#define _UITREATMENT_

extern void RefreshTreatmentLocationList(HWND eDialogHwnd, int32_t s32ControlID, uint64_t u64TreatmentTypeMask, SEditControlErr *psEditCtrlErr);
extern bool SelectTreatmentLocationListItem(HWND eDialogHwnd, int32_t s32ControlID, ETreatmentLocation eTreatmentLocation);
extern ETreatmentLocation GetSelectedTreatmentLocation(HWND eDialogHwnd, int32_t s32ControlID);
extern bool GetTreatmentLocationName(ETreatmentLocation eTreatmentLocation, WCHAR *peName, uint32_t u32Length);

#endif