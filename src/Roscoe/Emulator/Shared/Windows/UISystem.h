
#ifndef _SHARED_WINDOWS_UISYSTEM_H_
#define _SHARED_WINDOWS_UISYSTEM_H_

#define UISYSTEM_USERS_INCL_DISABLED			(1 << 0)
#define UISYSTEM_USERS_EXCL_DISABLED			(1 << 1)
#define UISYSTEM_AUTHORIZERS					(1 << 2)
#define UISYSTEM_CLINICS						(1 << 3)
#define UISYSTEM_PATIENTS_ALL					(1 << 4)
#define UISYSTEM_COUNTRIES						(1 << 5)
#define UISYSTEM_TREATMENTLOCATIONS				(1 << 6)
#define UISYSTEM_ROOMS							(1 << 7)
#define UISYSTEM_USERS_ASSOCIATED_INCL_DISABLED	(1 << 8)
#define UISYSTEM_USERS_ASSOCIATED_EXCL_DISABLED	(1 << 9)
#define UISYSTEM_PATIENTS_ASSOCIATED_CLINICS	(1 << 10)
#define UISYSTEM_PATIENTS_ACTIVE_CLINIC			(1 << 11)

#define UISYSTEM_ALL_TYPES (0xffffffff)
#define UISYSTEM_ALL_USER_TYPES (UISYSTEM_USERS_INCL_DISABLED | UISYSTEM_USERS_EXCL_DISABLED | UISYSTEM_USERS_ASSOCIATED_INCL_DISABLED | UISYSTEM_USERS_EXCL_DISABLED)

extern void UISystemGetTreatmentLocationList( STreatmentLocation** ppsList,
											  SEditControlErr* psEditCtrlErr );

extern void UISystemRefreshLanguages( HWND eDialog,
									  int32_t s32ControlID,
									  SEditControlErr* psErr );

extern ELanguage UISystemGetSelectedLanguage( HWND eDialog,
											  int32_t s32ControlID );

extern bool UISystemSelectLanguage( HWND eDialog,
									int32_t s32ControlID,
									ELanguage eItem );

extern void UISystemRefreshClassifications( HWND eDialog,
											int32_t s32ControlID,
											SEditControlErr* psErr );

extern EPatientClassification UISystemGetSelectedClassification( HWND eDialog,
																 int32_t s32ControlID );

extern bool UISystemSelectClassification( HWND eDialog,
										  int32_t s32ControlID,
										  EPatientClassification eItem );

extern void UISystemRefreshCountries( HWND eDialog,
									  int32_t s32ControlID,
									  SEditControlErr* psErr );

extern ECountry UISystemGetSelectedCountry( HWND eDialog,
											int32_t s32ControlID );

extern bool UISystemSelectCountry( HWND eDialog,
								   int32_t s32ControlID,
								   ECountry eItem );

extern EPatientReferredBy UISystemGetSelectedReferredByID( HWND eDialog,
														   int32_t s32ControlID );

extern bool UISystemIsReferredByOptionOther( HWND eDialog,
											 int32_t s32ControlID );

extern bool UISystemSelectReferredByOptionsItem( HWND eDialog, 
												 int32_t s32ControlID,
												 EPatientReferredBy eReferral );

extern void UISystemRefreshReferredByOptions( HWND eDialog,
											  int32_t s32ControlID,
											  SEditControlErr* psErr );

extern SFieldInfo* UISystemLookupFieldInfo( EAtomID eAtomID );

extern void UISystemRefreshSystemLists( uint32_t u32Flags,
										SEditControlErr* psErr );

#endif // _SHARED_WINDOWS_UISYSTEM_H_