#ifndef _CONTROLCOMBOBOX_H_
#define _CONTROLCOMBOBOX_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlComboBoxHandle;

typedef struct SControlComboBoxConfig
{
	// Filter box
	EFontHandle eFilterBoxFont;
	UINT16 u16FilterBoxFontSize;
	UINT32 u32RGBATextSelectedColor;
	char *peTextNotSelected;
	UINT32 u32FilterMaxCharInput;

	// Offset for both selected text and line input
	INT32 s32XSelectedOffset;
	INT32 s32YSelectedOffset;

	// Line input cursor input info
	UINT32 u32BlinkdRateMs;
	INT32 s32CursorYAdj;
	UINT32 u32CursorXSize;
	UINT32 u32CursorYSize;

	// Button to drop the list
	INT32 s32XDropButtonConfigOffset;
	INT32 s32YDropButtonConfigOffset;
	SControlButtonConfig sDropButtonConfig;

	// List box configuration
	SControlListBoxConfig sListBoxConfig;

} SControlComboBoxConfig;

extern EStatus ControlComboBoxCreate(EWindowHandle eWindowHandle,
									 EControlComboBoxHandle *peControlComboBoxHandle,
									 INT32 s32XPos,
									 INT32 s32YPos);
extern EStatus ControlComboBoxSetAssets(EControlComboBoxHandle eControlComboBoxHandle,
										UINT32 u32XSize,
										UINT32 u32YSize,
										SControlComboBoxConfig *psControlomboBoxConfig);
extern EStatus ControComboBoxItemAdd(EControlComboBoxHandle eControlComboBoxHandle,
									 char *peText,
									 void *pvUserData,
									 UINT64 *pu64AssignedIndex,
									 INT8 (*SortCompareFunction)(EControlComboBoxHandle eControlComboBoxHandle,
																 char *peText1,
																 UINT64 u64Index1,
																 void *pvUserData1,
																 char *peText2,
																 UINT64 u64Index2,
																 void *pvUserData2));
extern EStatus ControlComboBoxSetCallback(EControlComboBoxHandle eControlComboBoxHandle,
										  void *pvCallbackUserData,
										  void (*SelectCallback)(EControlComboBoxHandle eControlComboBoxHandle,
																 void *pvUserData,
																 EControlCallbackReason eCallbackReason,
																 char *peText,
																 UINT64 u64Index,
																 void *pvCallbackUserData));
extern EStatus ControlComboBoxDeleteItem(EControlListBoxHandle eControlComboBoxHandle,
										 UINT64 u64Index,
										 void *pvUserData);
extern EStatus ControlComboBoxFind(EControlComboBoxHandle eControlComboBoxHandle,
								   UINT64 u64Index,
								   void *pvUserData,
								   char **ppeText,
								   void **ppvUserData,
								   UINT64 *pu64Index,
								   BOOL *pbFilterVisible);
extern EStatus ControlComboBoxItemSetFilterVisible(EControlComboBoxHandle eControlComboBoxHandle,
												   UINT64 u64Index,
												   BOOL bFilterVisible);
extern EStatus ControlComboBoxSetSelected(EControlComboBoxHandle eControlComboBoxHandle,
										  UINT64 u64Index,
										  void *pvUserData,
										  BOOL bfilterVisible);
extern EStatus ControlComboBoxGetSelected(EControlListBoxHandle eControlComboBoxHandle,
										  UINT64 *pu64Index,
										  char **ppeTag,
										  void **ppvUserData);
extern EStatus ControlComboBoxItemSetFilters(EControlComboBoxHandle eControlComboBoxHandle,
											 BOOL bFilteredList,
											 void *pvTempUserData,
											 BOOL (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																		void *pvTempUserData,
																		char *peText,
																		UINT64 u64Index,
																		void *pvUserData));
extern EStatus ControlComboBoxInit(void);

#endif
