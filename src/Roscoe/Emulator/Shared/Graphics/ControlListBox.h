#ifndef _CONTROLLISTBOX_H_
#define _CONTROLLISTBOX_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlListBoxHandle;

// Assigned indext for "list box not selected"
#define	LISTBOX_NOT_SELECTED		0

// Structure used for
typedef struct SControlListBoxConfig
{
	// Overall size we want our list box
	uint32_t u32XSize;
	uint32_t u32YSize;
	uint32_t u32Lines;

	// List box graphical assets
	uint32_t *pu32RGBACornerImage;
	uint32_t u32CornerXSize;
	uint32_t u32CornerYSize;
	uint32_t *pu32RGBAEdgeImage;
	uint32_t u32EdgeXSize;
	uint32_t u32EdgeYSize;
	uint32_t u32RGBAFillPixel;

	// Text assets
	EFontHandle eFont;
	uint16_t u16FontSize;
	uint32_t u32TextColor;

	// Slider assets (optional)

	// Left side of slider
	uint32_t *pu32RGBALeft;
	uint32_t u32XSizeLeft;
	uint32_t u32YSizeLeft;

	// Center/stretched part of slider
	uint32_t *pu32RGBACenter;
	uint32_t u32XSizeCenter;
	uint32_t u32YSizeCenter;

	// Right side of slider
	uint32_t *pu32RGBARight;
	uint32_t u32XSizeRight;
	uint32_t u32YSizeRight;

	// Thumb of slider
	uint32_t *pu32RGBAThumb;
	uint32_t u32XSizeThumb;
	uint32_t u32YSizeThumb;

	// Offset of list from upper left hand corner of control
	int32_t s32XListOFfset;
	int32_t s32YListOFfset;

	// Offset of text from its calculated position
	int32_t s32XTextOffset;
	int32_t s32YTextOffset;

	// Separator information
	uint32_t u32SeparatorXOffset;			// Offset of separator from left side of window
	uint32_t u32SeparatorThickness;		// Graphical thickness (in lines) of separator)
	uint32_t u32SeparatorSpacing;			// # Of pixels to each side of each separator
	uint32_t u32RGBASeparatorColor;		// Color of separator
} SControlListBoxConfig;

extern EStatus ControlListBoxCreate(EWindowHandle eWindowHandle,
									EControlListBoxHandle *peControlListBoxHandle,
									int32_t s32XPos,
									int32_t s32YPos);
extern EStatus ControlListBoxItemAdd(EControlListBoxHandle eControlListBoxHandle,
									 char *peText,
									 void *pvUserData,
									 uint64_t *pu64AssignedIndex,
									 int8_t (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																 char *peText1,
																 uint64_t u64Index1,
																 void *pvUserData1,
																 char *peText2,
																 uint64_t u64Index2,
																 void *pvUserData2));
extern EStatus ControlListBoxItemSort(EControlListBoxHandle eControlListBoxHandle,
									  int8_t (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																  char *peText1,
																  uint64_t u64Index1,
																  void *pvUserData1,
																  char *peText2,
																  uint64_t u64Index2,
																  void *pvUserData2));
extern EStatus ControlListBoxDeleteItem(EControlListBoxHandle eControlListBoxHandle,
										uint64_t u64Index,
										void *pvUserData);
extern EStatus ControlListBoxItemSetText(EControlListBoxHandle eControlListBoxHandle,
										 uint64_t u64Index,
										 char *peText);
extern EStatus ControlListBoxItemSetUserData(EControlListBoxHandle eControlListBoxHandle,
											 uint64_t u64Index,
											 void *pvUserData);
extern EStatus ControlListBoxItemSetFilterVisible(EControlListBoxHandle eControlListBoxHandle,
												  uint64_t u64Index,
												  bool bFilterVisible);
extern EStatus ControlListBoxFind(EControlListBoxHandle eControlListBoxHandle,
								  uint64_t u64Index,
								  void *pvUserData,
								  char **ppeText,
								  void **ppvUserData,
								  uint64_t *pu64Index,
								  bool *pbFilterVisible);
extern EStatus ControlListBoxItemSetFilters(EControlListBoxHandle eControlListBoxHandle,
											bool bFilteredList,
											void *pvTempUserData,
											bool (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																	   void *pvTempUserData,
																	   char *peText,
																	   uint64_t u64Index,
																	   void *pvUserData));
extern EStatus ControlListBoxSetAssets(EControlListBoxHandle eControlListBoxHandle,
									   SControlListBoxConfig *psControlListBoxConfig);
extern EStatus ControlListBoxSetCallback(EControlListBoxHandle eControlListBoxHandle,
										 void *pvUserData,
										 void (*SelectCallback)(EControlListBoxHandle eControlListBoxHandle,
																void *pvUserData,
																EControlCallbackReason eCallbackReason,
																char *peText,
																uint64_t u64Index,
																void *pvItemUserData));
extern EStatus ControlListBoxSetSelected(EControlListBoxHandle eControlListBoxHandle,
										 uint64_t u64Index,
										 void *pvUserData,
										 bool bfilterVisible);
extern EStatus ControlListBoxGetSelected(EControlListBoxHandle eControlListBoxHandle,
										 uint64_t *pu64Index,
										 char **ppeTag,
										 void **ppvUserData);
extern EStatus ControlListBoxDump(EControlListBoxHandle eControlListBoxHandle);
extern EStatus ControlListBoxInit(void);

#endif
