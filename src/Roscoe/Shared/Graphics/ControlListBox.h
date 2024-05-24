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
	UINT32 u32XSize;
	UINT32 u32YSize;
	UINT32 u32Lines;

	// List box graphical assets
	UINT32 *pu32RGBACornerImage;
	UINT32 u32CornerXSize;
	UINT32 u32CornerYSize;
	UINT32 *pu32RGBAEdgeImage;
	UINT32 u32EdgeXSize;
	UINT32 u32EdgeYSize;
	UINT32 u32RGBAFillPixel;

	// Text assets
	EFontHandle eFont;
	UINT16 u16FontSize;
	UINT32 u32TextColor;

	// Slider assets (optional)

	// Left side of slider
	UINT32 *pu32RGBALeft;
	UINT32 u32XSizeLeft;
	UINT32 u32YSizeLeft;

	// Center/stretched part of slider
	UINT32 *pu32RGBACenter;
	UINT32 u32XSizeCenter;
	UINT32 u32YSizeCenter;

	// Right side of slider
	UINT32 *pu32RGBARight;
	UINT32 u32XSizeRight;
	UINT32 u32YSizeRight;

	// Thumb of slider
	UINT32 *pu32RGBAThumb;
	UINT32 u32XSizeThumb;
	UINT32 u32YSizeThumb;

	// Offset of list from upper left hand corner of control
	INT32 s32XListOFfset;
	INT32 s32YListOFfset;

	// Offset of text from its calculated position
	INT32 s32XTextOffset;
	INT32 s32YTextOffset;

	// Separator information
	UINT32 u32SeparatorXOffset;			// Offset of separator from left side of window
	UINT32 u32SeparatorThickness;		// Graphical thickness (in lines) of separator)
	UINT32 u32SeparatorSpacing;			// # Of pixels to each side of each separator
	UINT32 u32RGBASeparatorColor;		// Color of separator
} SControlListBoxConfig;

extern EStatus ControlListBoxCreate(EWindowHandle eWindowHandle,
									EControlListBoxHandle *peControlListBoxHandle,
									INT32 s32XPos,
									INT32 s32YPos);
extern EStatus ControlListBoxItemAdd(EControlListBoxHandle eControlListBoxHandle,
									 char *peText,
									 void *pvUserData,
									 UINT64 *pu64AssignedIndex,
									 INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																 char *peText1,
																 UINT64 u64Index1,
																 void *pvUserData1,
																 char *peText2,
																 UINT64 u64Index2,
																 void *pvUserData2));
extern EStatus ControlListBoxItemSort(EControlListBoxHandle eControlListBoxHandle,
									  INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																  char *peText1,
																  UINT64 u64Index1,
																  void *pvUserData1,
																  char *peText2,
																  UINT64 u64Index2,
																  void *pvUserData2));
extern EStatus ControlListBoxDeleteItem(EControlListBoxHandle eControlListBoxHandle,
										UINT64 u64Index,
										void *pvUserData);
extern EStatus ControlListBoxItemSetText(EControlListBoxHandle eControlListBoxHandle,
										 UINT64 u64Index,
										 char *peText);
extern EStatus ControlListBoxItemSetUserData(EControlListBoxHandle eControlListBoxHandle,
											 UINT64 u64Index,
											 void *pvUserData);
extern EStatus ControlListBoxItemSetFilterVisible(EControlListBoxHandle eControlListBoxHandle,
												  UINT64 u64Index,
												  BOOL bFilterVisible);
extern EStatus ControlListBoxFind(EControlListBoxHandle eControlListBoxHandle,
								  UINT64 u64Index,
								  void *pvUserData,
								  char **ppeText,
								  void **ppvUserData,
								  UINT64 *pu64Index,
								  BOOL *pbFilterVisible);
extern EStatus ControlListBoxItemSetFilters(EControlListBoxHandle eControlListBoxHandle,
											BOOL bFilteredList,
											void *pvTempUserData,
											BOOL (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																	   void *pvTempUserData,
																	   char *peText,
																	   UINT64 u64Index,
																	   void *pvUserData));
extern EStatus ControlListBoxSetAssets(EControlListBoxHandle eControlListBoxHandle,
									   SControlListBoxConfig *psControlListBoxConfig);
extern EStatus ControlListBoxSetCallback(EControlListBoxHandle eControlListBoxHandle,
										 void *pvUserData,
										 void (*SelectCallback)(EControlListBoxHandle eControlListBoxHandle,
																void *pvUserData,
																EControlCallbackReason eCallbackReason,
																char *peText,
																UINT64 u64Index,
																void *pvItemUserData));
extern EStatus ControlListBoxSetSelected(EControlListBoxHandle eControlListBoxHandle,
										 UINT64 u64Index,
										 void *pvUserData,
										 BOOL bfilterVisible);
extern EStatus ControlListBoxGetSelected(EControlListBoxHandle eControlListBoxHandle,
										 UINT64 *pu64Index,
										 char **ppeTag,
										 void **ppvUserData);
extern EStatus ControlListBoxDump(EControlListBoxHandle eControlListBoxHandle);
extern EStatus ControlListBoxInit(void);

#endif
