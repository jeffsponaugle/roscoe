#ifndef _CONTROLCOMBOBOX_H_
#define _CONTROLCOMBOBOX_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlComboBoxHandle;

typedef struct SControlComboBoxConfig
{
	// Filter box
	EFontHandle eFilterBoxFont;
	uint16_t u16FilterBoxFontSize;
	uint32_t u32RGBATextSelectedColor;
	char *peTextNotSelected;
	uint32_t u32FilterMaxCharInput;

	// Offset for both selected text and line input
	int32_t s32XSelectedOffset;
	int32_t s32YSelectedOffset;

	// Line input cursor input info
	uint32_t u32BlinkdRateMs;
	int32_t s32CursorYAdj;
	uint32_t u32CursorXSize;
	uint32_t u32CursorYSize;

	// Button to drop the list
	int32_t s32XDropButtonConfigOffset;
	int32_t s32YDropButtonConfigOffset;
	SControlButtonConfig sDropButtonConfig;

	// List box configuration
	SControlListBoxConfig sListBoxConfig;

} SControlComboBoxConfig;

extern EStatus ControlComboBoxCreate(EWindowHandle eWindowHandle,
									 EControlComboBoxHandle *peControlComboBoxHandle,
									 int32_t s32XPos,
									 int32_t s32YPos);
extern EStatus ControlComboBoxSetAssets(EControlComboBoxHandle eControlComboBoxHandle,
										uint32_t u32XSize,
										uint32_t u32YSize,
										SControlComboBoxConfig *psControlomboBoxConfig);
extern EStatus ControComboBoxItemAdd(EControlComboBoxHandle eControlComboBoxHandle,
									 char *peText,
									 void *pvUserData,
									 uint64_t *pu64AssignedIndex,
									 int8_t (*SortCompareFunction)(EControlComboBoxHandle eControlComboBoxHandle,
																 char *peText1,
																 uint64_t u64Index1,
																 void *pvUserData1,
																 char *peText2,
																 uint64_t u64Index2,
																 void *pvUserData2));
extern EStatus ControlComboBoxSetCallback(EControlComboBoxHandle eControlComboBoxHandle,
										  void *pvCallbackUserData,
										  void (*SelectCallback)(EControlComboBoxHandle eControlComboBoxHandle,
																 void *pvUserData,
																 EControlCallbackReason eCallbackReason,
																 char *peText,
																 uint64_t u64Index,
																 void *pvCallbackUserData));
extern EStatus ControlComboBoxDeleteItem(EControlListBoxHandle eControlComboBoxHandle,
										 uint64_t u64Index,
										 void *pvUserData);
extern EStatus ControlComboBoxFind(EControlComboBoxHandle eControlComboBoxHandle,
								   uint64_t u64Index,
								   void *pvUserData,
								   char **ppeText,
								   void **ppvUserData,
								   uint64_t *pu64Index,
								   bool *pbFilterVisible);
extern EStatus ControlComboBoxItemSetFilterVisible(EControlComboBoxHandle eControlComboBoxHandle,
												   uint64_t u64Index,
												   bool bFilterVisible);
extern EStatus ControlComboBoxSetSelected(EControlComboBoxHandle eControlComboBoxHandle,
										  uint64_t u64Index,
										  void *pvUserData,
										  bool bfilterVisible);
extern EStatus ControlComboBoxGetSelected(EControlListBoxHandle eControlComboBoxHandle,
										  uint64_t *pu64Index,
										  char **ppeTag,
										  void **ppvUserData);
extern EStatus ControlComboBoxItemSetFilters(EControlComboBoxHandle eControlComboBoxHandle,
											 bool bFilteredList,
											 void *pvTempUserData,
											 bool (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																		void *pvTempUserData,
																		char *peText,
																		uint64_t u64Index,
																		void *pvUserData));
extern EStatus ControlComboBoxInit(void);

#endif
