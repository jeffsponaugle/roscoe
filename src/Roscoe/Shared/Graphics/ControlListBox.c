#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"
#include "Shared/Graphics/Font.h"

// Item list
typedef struct SControlListBoxItem
{
	char *peText;								// Text of item
	UINT64 u64Index;							// Index into list of items
	BOOL bFilterVisible;						// TRUE If the filter visibility is OK
	void *pvUserData;							// User data for this item

	// Sorted item list
	struct SControlListBoxItem *psPriorSorted;
	struct SControlListBoxItem *psNextSorted;

	// Linear
	struct SControlListBoxItem *psPriorLink;
	struct SControlListBoxItem *psNextLink;

} SControlListBoxItem;

// List box items
typedef struct SControlListBox
{
	// The graphics for the control's guide (background window + lines)
	SGraphicsImage *psControlGuide;

	// Text overlay
	SGraphicsImage *psTextOverlay;

	// Set TRUE if the text list needs to get rerendered
	BOOL bTextOverlayRender;

	// Font related
	EFontHandle eFont;
	UINT16 u16FontSize;
	UINT32 u32TextColor;

	// Location of text draw
	INT32 s32XTextBase;
	INT32 s32YTextBase;
	UINT32 u32TextSpacing;
	UINT32 u32XSeparatorSize;

	// Offset of text from its calculated position
	INT32 s32XTextOffset;
	INT32 s32YTextOffset;

	// Slider control
	EControlSliderHandle eSelectSlider;			// List selection control

	// Hit control
	EControlHitHandle eHitHandle;

	// Next assigned item ID
	UINT64 u64NextID;

	// # Of lines we're displaying
	UINT32 u32Lines;

	// All items
	SControlListBoxItem *psItems;				// All list
	SControlListBoxItem *psItemSorted;			// Sorted list
	UINT32 u32TotalItems;						// Total # of items in item list
	UINT32 u32FilteredItems;					// Total # of items that are filter viewaable

	// Selected items
	SControlListBoxItem *psItemTopOfVisibleList;
	SControlListBoxItem *psItemSelected;

	// Callback information
	void *pvUserData;
	void (*SelectCallback)(EControlListBoxHandle eControlListBoxHandle,
						   void *pvUserData,
						   EControlCallbackReason eCallbackReason,
						   char *peText,
						   UINT64 u64Index,
						   void *pvItemUserData);
} SControlListBox;

// Destroys a list box item
static void ListBoxItemDestroy(SControlListBoxItem *psItem)
{
	if (psItem)
	{
		SafeMemFree(psItem->peText);
		SafeMemFree(psItem);
	}
}

// Called when we need to set the slider's position
static EStatus ControlListBoxSetSliderPosition(SControlListBox *psControlListBox)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBoxItem *psItem;
	INT32 s32Value = 0;

	psItem = psControlListBox->psItemSorted;
	while (psItem && psItem != psControlListBox->psItemSelected)
	{
		s32Value++;
		psItem = psItem->psNextSorted;
	}

	if (psItem)
	{
		eStatus = ControlSliderSetValue(psControlListBox->eSelectSlider,
										s32Value);
	}

	return(eStatus);
}

// This processes a user callback
static void ControlListBoxProcessCallback(SControl *psControl,
										  EControlCallbackReason eCallbackReason,
										  SControlListBox *psControlListBox,
										  SControlListBoxItem *psItem)
{
	EStatus eStatus;

	if (psControlListBox->SelectCallback)
	{
		if (psItem)
		{
			psControlListBox->SelectCallback((EControlListBoxHandle) psControl->eControlHandle,
											 psControlListBox->pvUserData,
											 eCallbackReason,
											 psItem->peText,
											 psItem->u64Index,
											 psItem->pvUserData);
		}
		else
		{
			psControlListBox->SelectCallback((EControlListBoxHandle) psControl->eControlHandle,
											 psControlListBox->pvUserData,
											 eCallbackReason,
											 NULL,
											 0,
											 NULL);
		}
	}

	// Set the slider position (if present)
	eStatus = ControlListBoxSetSliderPosition(psControlListBox);
	BASSERT(ESTATUS_OK == eStatus);
}

// This does a hit test to see if the control relative coordinate is in the text list
static EStatus ControlListBoxHitTest(SControl *psControl,
									 SControlListBox *psControlListBox,
									 INT32 s32ControlRelativeXPos,
									 INT32 s32ControlRelativeYPos)
{
	EStatus eStatus = ESTATUS_OK;

	if( (FALSE == psControl->bVisible) ||
		psControl->bDisabled )
	{
		// Not us, man
		return(ESTATUS_UI_CONTROL_NOT_HIT);
	}

	// Only register a hit if it's within the list 
	if ( ((s32ControlRelativeXPos >= psControlListBox->s32XTextBase) && (s32ControlRelativeXPos < ((INT32) psControlListBox->u32XSeparatorSize + psControlListBox->s32XTextBase))) &&
		 ((s32ControlRelativeYPos >= (psControlListBox->s32YTextBase + psControlListBox->s32YTextOffset) && (s32ControlRelativeYPos < (INT32) (psControlListBox->s32YTextBase + psControlListBox->s32YTextOffset + ((INT32) psControlListBox->u32TextSpacing * psControlListBox->u32Lines))))) )
	{
		// Hit it!
		eStatus = ESTATUS_OK;
	}
	else
	{
		// Not us, man
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
	} 

	return(eStatus);
}

// Gets list box pointers and locks/unlocks the 
static EStatus ControlListBoxLockPointers(EControlListBoxHandle eControlListBoxHandle,
										  BOOL bLock,
										  SControl **ppsControl,
										  SControlListBox **ppsControlListBox,
										  EStatus eStatusIncoming)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlListBox *psControlListBox = NULL;

	eStatus = ControlSetLock(eControlListBoxHandle,
							 bLock,
							 &psControl,
							 eStatusIncoming);
	ERR_GOTO();

	if (ppsControl)
	{
		*ppsControl = psControl;
	}

	psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	if (ppsControlListBox)
	{
		*ppsControlListBox = psControlListBox;
	}

errorExit:
	return(eStatus);
}

// Called when someone wants to create a list box control
static EStatus ControlListBoxCreateMethod(SControl *psControl,
										  void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	// Make it focusable by default
	psControl->bFocusable = TRUE;

	// And set a nonzero "next" ID
	psControlListBox->u64NextID = 1;

	// Control guide
	eStatus = GraphicsCreateImage(&psControlListBox->psControlGuide,
								  TRUE,
								  0, 0);
	ERR_GOTO();

	// Text overlay
	eStatus = GraphicsCreateImage(&psControlListBox->psTextOverlay,
								  TRUE,
								  0, 0);
	ERR_GOTO();

errorExit:	
	return(eStatus);
}

// Find the next u32Count item that's visible
static SControlListBoxItem *ControlListBoxFindVisible(SControlListBoxItem *psStartItem,
													  UINT32 u32Count,
													  BOOL bAdvance)
{
	while ((u32Count) &&
		   (psStartItem))
	{
		// We either move backward or forward
		if (bAdvance)
		{
			psStartItem = psStartItem->psNextSorted;
		}
		else
		{
			psStartItem = psStartItem->psPriorSorted;
		}

		// If this item is visible, then decrement the count
		if (psStartItem && psStartItem->bFilterVisible)
		{
			--u32Count;
		}
	}

	return(psStartItem);
}

// Render text overlay (if necessary)
static void ControlListBoxRenderText(SControl *psControl,
									 SControlListBox *psControlListBox)
{
	UINT32 u32Loop;
	INT32 s32YPos;
	INT32 s32XPos;
	SControlListBoxItem *psItem;
	EStatus eStatus;

	// If we aren't changing things
	if (FALSE == psControlListBox->bTextOverlayRender)
	{
		return;
	}

	// Set the image to completely transparent to start with
	GraphicsFillImage(psControlListBox->psTextOverlay,
					  MAKERGBA(0, 0, 0, PIXEL_TRANSPARENT));

	s32XPos = psControlListBox->s32XTextBase + psControlListBox->s32XTextOffset;
	s32YPos = psControlListBox->s32YTextBase + psControlListBox->s32YTextOffset;
	u32Loop = psControlListBox->u32Lines;
	psItem = psControlListBox->psItemTopOfVisibleList;

	while (u32Loop && psItem)
	{
		if (psItem->bFilterVisible)
		{
			UINT32 u32Color;

			u32Color = psControlListBox->u32TextColor;

			if (psItem == psControlListBox->psItemSelected)
			{
				// Draw a bounding box for the text
				GraphicsFillImageRegion(psControlListBox->psTextOverlay,
										s32XPos,
										s32YPos,
										psControlListBox->u32XSeparatorSize,
										psControlListBox->u32TextSpacing,
										psControlListBox->u32TextColor);

				// Black pixel since we draw inverted
				u32Color = MAKERGBA(0, 0, 0, PIXEL_OPAQUE);
			}

			eStatus = FontRenderStringClip(psControlListBox->eFont,
										   psControlListBox->u16FontSize,
										   psItem->peText,
										   u32Color,
										   psControlListBox->psTextOverlay->pu32RGBA,
										   psControlListBox->psTextOverlay->u32TotalXSize,
										   s32XPos,
										   s32YPos,
										   psControlListBox->psTextOverlay->u32TotalXSize,
										   psControlListBox->psTextOverlay->u32TotalYSize);
			u32Loop--;
			s32YPos += psControlListBox->u32TextSpacing;
		}
		else
		{
			// Not visible - skip it.
		}

		psItem = psItem->psNextSorted;
	}

	// Cause a new texture transfer
	GraphicsImageUpdated(psControlListBox->psTextOverlay);
}

// Draws the list box assets
static EStatus ControlListBoxDrawMethod(SControl *psControl,
										UINT8 u8WindowIntensity,
										INT32 s32WindowXPos,
										INT32 s32WindowYPos)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	if (psControlListBox)
	{
		// Draw the control guide first
		GraphicsDraw(psControlListBox->psControlGuide,
					 s32WindowXPos + psControl->s32XPos,
					 s32WindowYPos + psControl->s32YPos,
					 psControl->bOriginCenter,
					 psControl->dAngle);

		// If we rerender the text, go do so (if we need to)
		ControlListBoxRenderText(psControl,
								 psControlListBox);

		// Now draw the text overlay
		GraphicsDraw(psControlListBox->psTextOverlay,
					 s32WindowXPos + psControl->s32XPos,
					 s32WindowYPos + psControl->s32YPos,
					 psControl->bOriginCenter,
					 psControl->dAngle);
	}

	return(eStatus);
}

static EStatus ControlListBoxProcessHit(SControl *psControl,
										SControlListBox *psControlListBox,
										INT32 s32ControlRelativeYPos,
										EControlCallbackReason eCallbackReason)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBoxItem *psItem = NULL;
	SControlListBoxItem *psItemOriginal = NULL;

	// We've clicked on a reasonable region. Let's figure out which item they're clicking on
	// and adjust it accordingly (if necessary).

	// Subtract off the text base and offset coordinates
	s32ControlRelativeYPos -= (psControlListBox->s32YTextBase + psControlListBox->s32YTextOffset); 

	// Now divide by the text spacing and it should give us the index #
	// in our current list.
	s32ControlRelativeYPos /= psControlListBox->u32TextSpacing;

	BASSERT(s32ControlRelativeYPos < (INT32) psControlListBox->u32Lines);

	psItem = psControlListBox->psItemTopOfVisibleList;
	psItemOriginal = psControlListBox->psItemSelected;

	// Now scan down the list for the nth item
	psControlListBox->psItemSelected = ControlListBoxFindVisible(psItem,
																	(UINT32) s32ControlRelativeYPos,
																	TRUE);

	if ((psControlListBox->psItemSelected != psItemOriginal) ||
		((ECTRLACTION_MOUSEBUTTON == eCallbackReason) && (psControlListBox->psItemSelected)))
	{
		// Notify the callbacks if there are any
		ControlListBoxProcessCallback(psControl,
										eCallbackReason,
										psControlListBox,
										psControlListBox->psItemSelected);

		// Things changed. Time for an update
		psControlListBox->bTextOverlayRender = TRUE;
		WindowUpdated();
	}

	return(eStatus);
}


// Called when a mouse button is hit
// Button pressed
static EStatus ControlListBoxButtonMethod(SControl *psControl,
										  UINT8 u8ButtonMask,
										  INT32 s32ControlRelativeXPos,
										  INT32 s32ControlRelativeYPos,
										  BOOL bPressed)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	// Only care about buttons being pressed
	if (bPressed)
	{
		eStatus = ControlListBoxHitTest(psControl,
										psControlListBox,
										s32ControlRelativeXPos,
										s32ControlRelativeYPos);
		if (ESTATUS_OK == eStatus)
		{
			eStatus = ControlListBoxProcessHit(psControl,
											   psControlListBox,
											   s32ControlRelativeYPos,
											   ECTRLACTION_MOUSEBUTTON);
		}
	}

	return(eStatus);
}

// This will move the selector up one item (TRUE if updated, FALSE if not)
static BOOL ControlListBoxUpItem(SControlListBox *psControlListBox)
{
	BOOL bUpdated = FALSE;
	SControlListBoxItem *psItemPrior = NULL;

	psItemPrior = ControlListBoxFindVisible(psControlListBox->psItemSelected,
											1,
											FALSE);

	// If we don't have a prior item, then we're at the top of the list
	if (psItemPrior)
	{
		// If we're already at the top item, then move the list up one if we can
		if (psControlListBox->psItemTopOfVisibleList == psControlListBox->psItemSelected)
		{
			psControlListBox->psItemTopOfVisibleList = psItemPrior;
		}

		psControlListBox->psItemSelected = psItemPrior;
		bUpdated = TRUE;
	}
	else
	{
		// Ignore the up arrow
	}

	return(bUpdated);
}

// This will move the selector down one item
static BOOL ControlListBoxDownItem(SControlListBox *psControlListBox)
{
	BOOL bUpdated = FALSE;
	SControlListBoxItem *psItemNext = NULL;

	// Find the next item
	psItemNext = ControlListBoxFindVisible(psControlListBox->psItemSelected,
											1,
											TRUE);

	if (psItemNext)
	{
		SControlListBoxItem *psItemEndOfPage = NULL;

		psItemEndOfPage = ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
													psControlListBox->u32Lines - 1,
													TRUE);

		// If we're at the end of the page, we may need to scroll it
		if (psItemEndOfPage == psControlListBox->psItemSelected)
		{
			// Move the top of list to the next item
			psControlListBox->psItemTopOfVisibleList = ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
																					1,
																					TRUE);
			BASSERT(psControlListBox->psItemTopOfVisibleList);
		}

		psControlListBox->psItemSelected = psItemNext;
		bUpdated = TRUE;
	}

	return(bUpdated);
}

// Called during scancode entry
static EStatus ControlListBoxScancodeInputMethod(SControl *psControl,		// Keyscan code input
												 EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call (or invalid if none)
												 SDL_Scancode eScancode,	// SDL Scancode for key hit
												 BOOL bPressed)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;
	BOOL bUpdated = FALSE;
	SControlListBoxItem *psItemEntry = NULL;

	psItemEntry = psControlListBox->psItemSelected;

	// If we have no selected item on entry, then we need to select the item at the
	// top of the page and not process the press
	if ((NULL == psItemEntry) &&
		(bPressed))
	{
		if ((SDL_SCANCODE_UP == eScancode) ||
			(SDL_SCANCODE_DOWN == eScancode) ||
			(SDL_SCANCODE_HOME == eScancode) ||
			(SDL_SCANCODE_END == eScancode) ||
			(SDL_SCANCODE_PAGEUP == eScancode) ||
			(SDL_SCANCODE_PAGEDOWN == eScancode))
		{
			psControlListBox->psItemSelected = psControlListBox->psItemTopOfVisibleList;

			// We want to do an update but no processing the first time
			bPressed = FALSE;
			bUpdated = TRUE;
		}
	}

	// This routine only looks for enter, backspace, tab, etc...
	if ((bPressed) &&
		(psControlListBox))
	{
		// Moving up?
		if (SDL_SCANCODE_UP == eScancode)
		{
			bUpdated = ControlListBoxUpItem(psControlListBox);
		}
		else	// Moving down?
		if (SDL_SCANCODE_DOWN == eScancode)
		{
			bUpdated = ControlListBoxDownItem(psControlListBox);
		}
		else
		if (SDL_SCANCODE_HOME == eScancode)
		{
			// If we're not at the top of the list, then move the selected item there
			if (psControlListBox->psItemTopOfVisibleList != psControlListBox->psItemSelected)
			{
				psControlListBox->psItemSelected = psControlListBox->psItemTopOfVisibleList;
				bUpdated = TRUE;
			}
			else
			if (psControlListBox->psItemSelected != psControlListBox->psItemSorted)
			{
				psControlListBox->psItemSelected = psControlListBox->psItemSorted;
				psControlListBox->psItemTopOfVisibleList = psControlListBox->psItemSelected;
				bUpdated = TRUE;
			}
		}
		else
		if (SDL_SCANCODE_END == eScancode)
		{
			SControlListBoxItem *psItemEndOfPage = NULL;

			psItemEndOfPage = ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
														psControlListBox->u32Lines - 1,
														TRUE);

			if (psItemEndOfPage != psControlListBox->psItemSelected)
			{
				psControlListBox->psItemSelected = psItemEndOfPage;
			}
			else
			{
				SControlListBoxItem *psItemLast = NULL;

				// We need to find the end of the entire list and walk everything back.
				psItemLast = ControlListBoxFindVisible(psControlListBox->psItemSorted,
													   psControlListBox->u32FilteredItems - 1,
													   TRUE);
				BASSERT(psItemLast);

				// From the last item, let's back up a page so we can set the top of visible list item
				psControlListBox->psItemTopOfVisibleList = ControlListBoxFindVisible(psItemLast,
																					 psControlListBox->u32Lines - 1,
																					 FALSE);

				// If top of the visible list is NULL, it means we don't have enough items to display, so just align it to the first item
				if (NULL == psControlListBox->psItemTopOfVisibleList)
				{
					psControlListBox->psItemTopOfVisibleList = psControlListBox->psItemSorted;
				}

				psControlListBox->psItemSelected = psItemLast;
			}

			bUpdated = TRUE;
		}
		else
		if (SDL_SCANCODE_PAGEUP == eScancode)
		{
			if (psControlListBox->psItemSelected == psControlListBox->psItemTopOfVisibleList)
			{
				SControlListBoxItem *psItemPriorPage = NULL;

				// We're already at the top. Let's move back a "page".
				psItemPriorPage = ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
															psControlListBox->u32Lines - 1,
															FALSE);

				if (NULL == psItemPriorPage)
				{
					// Top of list.
					psItemPriorPage = psControlListBox->psItemSorted;
				}

				psControlListBox->psItemTopOfVisibleList = psItemPriorPage;
			}
			else
			{
				// Just move to the top of the list box
			}

			bUpdated = TRUE;
			psControlListBox->psItemSelected = psControlListBox->psItemTopOfVisibleList;
		}
		else
		if (SDL_SCANCODE_PAGEDOWN == eScancode)
		{
			SControlListBoxItem *psItemEndOfPage = NULL;

			psItemEndOfPage = ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
														psControlListBox->u32Lines - 1,
														TRUE);

			// If we're not at the bottom of the page, highlight the last item
			if (psItemEndOfPage != psControlListBox->psItemSelected)
			{
				// Just set it to the end of the page and call it god
				psControlListBox->psItemSelected = psItemEndOfPage;
			}
			else
			{
				// Need to move down an entire page.
				psControlListBox->psItemTopOfVisibleList = psItemEndOfPage;

				psItemEndOfPage = ControlListBoxFindVisible(psItemEndOfPage,
															psControlListBox->u32Lines - 1,
															TRUE);
				if (NULL == psItemEndOfPage)
				{
					// Whoops. Went too far. Let's find the last item in the list
					psItemEndOfPage = ControlListBoxFindVisible(psControlListBox->psItemSorted,
																psControlListBox->u32FilteredItems - 1,
																TRUE);

					if (psItemEndOfPage)
					{
						// Let's adjust things to the end of the list
						psControlListBox->psItemTopOfVisibleList = ControlListBoxFindVisible(psItemEndOfPage,
																							 psControlListBox->u32Lines - 1,
																							 FALSE);

						// If this is true, we don't have enough items to fill the page, so just select the top
						if (NULL == psControlListBox->psItemTopOfVisibleList)
						{
							psControlListBox->psItemTopOfVisibleList = psControlListBox->psItemSorted;
						}
					}
				}

				psControlListBox->psItemSelected = psItemEndOfPage;
			}

			bUpdated = TRUE;
		}
	}

	// If the item has changed, let the callback know
	if (psItemEntry != psControlListBox->psItemSelected)
	{
		ControlListBoxProcessCallback(psControl,
									  ECTRLACTION_SELECTED,
									  psControlListBox,
									  psControlListBox->psItemSelected);
	}

	// Process any control common scancodes
	if (bPressed)
	{
		EControlCallbackReason eReason = ECTRLACTION_UNKNOWN;

		eReason = ControlKeyscanToCallbackReason(eScancode);
		if (eReason != ECTRLACTION_UNKNOWN)
		{
			ControlListBoxProcessCallback(psControl,
										  eReason,
										  psControlListBox,
										  psControlListBox->psItemSelected);
		}
	}

	// If something got updated, update the window and cause it to rerender
	if (bUpdated)
	{
		psControlListBox->bTextOverlayRender = TRUE;
		WindowUpdated();
	}

	return(eStatus);
}

// Whenever focus is gained or lost
static EStatus ControlListBoxFocusMethod(SControl *psControl,					// Called when this control gains focus
										 EControlHandle eControlHandleOld,
										 EControlHandle eControlHandleNew,
										 UINT8 u8ButtonMask,
										 INT32 s32WindowRelativeX,
										 INT32 s32WindowRelativeY)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;
	BOOL bFocusSet = FALSE;

	// We are the new focus if this is true
	if (eControlHandleNew == psControl->eControlHandle)
	{
		bFocusSet = TRUE;
	}

	if ((FALSE == bFocusSet) &&
		(psControl) &&
		(psControlListBox))
	{
		ControlListBoxProcessCallback(psControl,
									  ECTRLACTION_LOSTFOCUS,
									  psControlListBox,
									  psControlListBox->psItemSelected);
	}

	return(ESTATUS_OK);
}

// Called when the mouse wheel is moved
static EStatus ControlListBoxMouseWheelMethod(SControl *psControl,
											  INT32 s32WheelChange)
{
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;
	SControlListBoxItem *psItemEntry = NULL;
	BOOL bUpdated = FALSE;

	psItemEntry = psControlListBox->psItemSelected;

	// If this is the case, then no item was selected before, so we need to assign it to the top
	// of the list
	if (NULL == psItemEntry)
	{
		psControlListBox->psItemSelected = psControlListBox->psItemTopOfVisibleList;
	}

	while (s32WheelChange < 0)
	{
		if (ControlListBoxDownItem(psControlListBox))
		{
			bUpdated = TRUE;
		}

		++s32WheelChange;
	}

	while (s32WheelChange > 0)
	{
		if (ControlListBoxUpItem(psControlListBox))
		{
			bUpdated = TRUE;
		}

		--s32WheelChange;
	}

	// If the item has changed, let the callback know
	if (psItemEntry != psControlListBox->psItemSelected)
	{
		ControlListBoxProcessCallback(psControl,
									  ECTRLACTION_SELECTED,
									  psControlListBox,
									  psControlListBox->psItemSelected);
	}

	if (bUpdated)
	{
		psControlListBox->bTextOverlayRender = TRUE;
		WindowUpdated();
	}

	return(ESTATUS_OK);
}

// Called when setting list box visibility
static EStatus ControlListBoxSetVisibleMethod(SControl *psControl,
											  BOOL bVisible)
{
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	// Set/clear the slider control's visibility as well
	(void) ControlSetVisible((EControlHandle) psControlListBox->eSelectSlider,
							 bVisible);

	return(ESTATUS_OK);
}

static EStatus ControlListBoxSetPositionMethod(SControl *psControl,			// When a new position is set
											  INT32 s32WindowXPos,
											  INT32 s32WindowYPos)
{
	EStatus eStatus;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	eStatus = ControlSetPosition((EControlHandle) psControlListBox->eSelectSlider,
										 (psControl->s32XPos + psControl->u32XSize),
										 psControl->s32YPos);	

	return(eStatus);
}

// Called when this control is enabled or disabled
static EStatus ControlListBoxSetDisableMethod(SControl *psControl,
											  BOOL bDisabledBefore,
											  BOOL bDisabled)
{
	EStatus eStatus;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	// Disable/enable the subordinate controls
	eStatus = ControlSetDisable(psControlListBox->eSelectSlider,
								bDisabled);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

// Mouseover events
static EStatus ControlListBoxMouseoverMethod(struct SControl *psControl,		// Called when a mouse is moved over a control when not buttons pressed
											 INT32 s32ControlRelativeXPos,
											 INT32 s32ControlRelativeYPos)
{
	EStatus eStatus;
	SControlListBox *psControlListBox = (SControlListBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxHitTest(psControl,
									psControlListBox,
									s32ControlRelativeXPos,
									s32ControlRelativeYPos);
	if (ESTATUS_OK == eStatus)
	{
		eStatus = ControlListBoxProcessHit(psControl,
											psControlListBox,
											s32ControlRelativeYPos,
											ECTRLACTION_SELECTED);
	}

	return(eStatus);
}

// List of methods for this control
static SControlMethods sg_sControlListBoxMethods =
{
	ECTRLTYPE_LISTBOX,							// Type of control
	"List box",									// Name of control
	sizeof(SControlListBox),					// Size of control specific structure

	ControlListBoxCreateMethod,					// Create a control
	NULL,										// Destroy a control
	ControlListBoxSetDisableMethod,				// Control enable/disable
	ControlListBoxSetPositionMethod,			// New position
	ControlListBoxSetVisibleMethod,				// Set visible
	NULL,										// Set angle
	ControlListBoxDrawMethod,					// Draw the control
	NULL,										// Runtime configuration
	NULL,										// Hit test
	ControlListBoxFocusMethod,					// Focus set/loss
	ControlListBoxButtonMethod,					// Button press/release
	NULL,										// Mouseover when selected
	ControlListBoxMouseWheelMethod,				// Mouse wheel
	NULL,										// UTF8 Input
	ControlListBoxScancodeInputMethod,			// Scancode input
	NULL,										// Periodic timer
	ControlListBoxMouseoverMethod,				// Mouseover
	NULL,										// Size
};

// Creates a list box
EStatus ControlListBoxCreate(EWindowHandle eWindowHandle,
							 EControlListBoxHandle *peControlListBoxHandle,
							 INT32 s32XPos,
							 INT32 s32YPos)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControlListBox *psControlListBox;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peControlListBoxHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_LISTBOX,
							NULL);
	ERR_GOTO();

	// Now create a slider handler for this list box
	eStatus = ControlListBoxLockPointers(*peControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 eStatus);
	ERR_GOTO();

	// Create a slider
	eStatus = ControlSliderCreate(eWindowHandle,
								  &psControlListBox->eSelectSlider,
								  s32XPos,
								  s32YPos);

	if (ESTATUS_OK == eStatus)
	{
		// Kill the slider's range since we have no items
		eStatus = ControlSliderSetRange(psControlListBox->eSelectSlider,
										0, 0);
	}

	eStatus = ControlListBoxLockPointers(*peControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
	ERR_GOTO();

	// And the hit control. We'll set the size later
	eStatus = ControlHitCreate(eWindowHandle,
							   &psControlListBox->eHitHandle,
							   s32XPos,
							   s32YPos,
							   0, 0);
	ERR_GOTO();

	// Connect this slider to its parent
	eStatus = ControlAddChild(*peControlListBoxHandle,
							  psControlListBox->eSelectSlider);

errorExit:
	return(eStatus);
}

// Adds an item to the sorted list
static void ControlListBoxAddItemSortedInternal(EControlListBoxHandle eControlListBoxHandle,
												SControlListBox *psControlListBox,
												SControlListBoxItem *psItemToAdd,
												INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																			char *peTag1,
																			UINT64 u64Index1,
																			void *pvUserData1,
																			char *peTag2,
																			UINT64 u64Index2,
																			void *pvUserData2))
{
	SControlListBoxItem *psItem = NULL;
	SControlListBoxItem *psItemPrior = NULL;

	// Now we do the sort
	psItem = psControlListBox->psItemSorted;
	psItemPrior = NULL;

	while (psItem)
	{
		// If we have a sort compare function...
		if (SortCompareFunction)
		{
			// ... then see where this new item should be located. Anything > 0 means we've
			// found the spot
			if (SortCompareFunction(eControlListBoxHandle,
									psItem->peText,
									psItem->u64Index,
									psItem->pvUserData,
									psItemToAdd->peText,
									psItemToAdd->u64Index,
									psItemToAdd->pvUserData) > 0)
			{
				// This is where it goes!
				break;
			}
			else
			{
				// Goes later! By definition, end of the list
			}
		}
		else
		{
			// No sort compare function
		}

		psItemPrior = psItem;
		psItem = psItem->psNextSorted;
	}

	// Link it in to the sorted linked list
	if (NULL == psItemPrior)
	{
		psItemToAdd->psNextSorted = psControlListBox->psItemSorted;
		psControlListBox->psItemSorted = psItemToAdd;
	}
	else
	{
		// Inserting somewhere in the middle
		psItemToAdd->psNextSorted = psItemPrior->psNextSorted;
		psItemToAdd->psPriorSorted = psItemPrior;
		psItemPrior->psNextSorted = psItemToAdd;
		if (psItemToAdd->psNextSorted)
		{
			psItemToAdd->psNextSorted->psPriorSorted = psItemToAdd;
		}
	}
}

// This adds an item into the master list provided on input, both in the
// sorted order (or the order-as-added) list
static void ControlListBoxAddItemInternal(EControlListBoxHandle eControlListBoxHandle,
										  SControlListBox *psControlListBox,
										  SControlListBoxItem *psItemToAdd,
										  INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
																	  char *peTag1,
																	  UINT64 u64Index1,
																	  void *pvUserData1,
																	  char *peTag2,
																	  UINT64 u64Index2,
																	  void *pvUserData2))
{
	SControlListBoxItem *psItem = NULL;
	SControlListBoxItem *psItemPrior = NULL;

	// Better make sure there is no 
	BASSERT(NULL == psItemToAdd->psNextLink);

	// Add it to the "all" list.
	psItem = psControlListBox->psItems;
	while (psItem)
	{
		psItemPrior = psItem;
		psItem = psItem->psNextLink;
	}

	BASSERT(NULL == psItem);
	if (NULL == psItemPrior)
	{
		psControlListBox->psItems = psItemToAdd;
	}
	else
	{
		if(psItemPrior->psNextLink)
		{
			psItemPrior->psNextLink->psPriorLink = psItemToAdd;
			psItemToAdd->psNextLink = psItemPrior->psNextLink;
		}

		psItemToAdd->psPriorLink = psItemPrior;
		psItemPrior->psNextLink = psItemToAdd;
	}

	// Increase our total item count
	psControlListBox->u32TotalItems++;

	// Now add it to the sorted list
	ControlListBoxAddItemSortedInternal(eControlListBoxHandle,
										psControlListBox,
										psItemToAdd,
										SortCompareFunction);

	// If the item is filter visible, then add it to the count
	if (psItemToAdd->bFilterVisible)
	{
		EStatus eStatus;

		++psControlListBox->u32FilteredItems;
		BASSERT(psControlListBox->u32FilteredItems <= psControlListBox->u32TotalItems);

		// If we don't have a visible item list, then show it
		if (NULL == psControlListBox->psItemTopOfVisibleList)
		{
			psControlListBox->bTextOverlayRender = TRUE;
			psControlListBox->psItemTopOfVisibleList = psItemToAdd;
		}

		eStatus = ControlSliderSetRange(psControlListBox->eSelectSlider,
										0,
										psControlListBox->u32FilteredItems - 1);

		// This call should always work
		BASSERT(ESTATUS_OK == eStatus);
	}
}

// Removes an item from the *all* list
static void ControlListBoxRemoveItem(SControlListBox *psControlListBox,
									 SControlListBoxItem *psItemToRemove)
{
	// Take care of the prior link
	if (psItemToRemove->psPriorLink)
	{
		// Make sure the prior link's next link points to ths item
		BASSERT(psItemToRemove->psPriorLink->psNextLink == psItemToRemove);
		psItemToRemove->psPriorLink->psNextLink = psItemToRemove->psNextLink;
	}
	else
	{
		// Make sure we're at the head of the list
		BASSERT(psControlListBox->psItems == psItemToRemove);
		psControlListBox->psItems = psItemToRemove->psNextLink;
	}

	// Now take care of the next link
	if (psItemToRemove->psNextLink)
	{
		BASSERT(psItemToRemove->psNextLink->psPriorLink == psItemToRemove);
		psItemToRemove->psNextLink->psPriorLink = psItemToRemove->psPriorLink;
	}

	// And NULL out of previous and next pointers
	psItemToRemove->psNextLink = NULL;
	psItemToRemove->psPriorLink = NULL;
}

// Removes an item from the *SORTED* list
static void ControlListBoxRemoveItemSorted(SControlListBox *psControlListBox,
										   SControlListBoxItem *psItemToRemove)
{
	// Take care of the prior link
	if (psItemToRemove->psPriorSorted)
	{
		// Make sure the prior link's next link points to ths item
		BASSERT(psItemToRemove->psPriorSorted->psNextSorted == psItemToRemove);
		psItemToRemove->psPriorSorted->psNextSorted = psItemToRemove->psNextSorted;
	}
	else
	{
		// Make sure we're at the head of the list
		BASSERT(psControlListBox->psItemSorted == psItemToRemove);
		psControlListBox->psItemSorted = psItemToRemove->psNextSorted;
	}

	// Now take care of the next link
	if (psItemToRemove->psNextSorted)
	{
		BASSERT(psItemToRemove->psNextSorted->psPriorSorted == psItemToRemove);
		psItemToRemove->psNextSorted->psPriorSorted = psItemToRemove->psPriorSorted;
	}

	// And NULL out of previous and next pointers
	psItemToRemove->psNextSorted = NULL;
	psItemToRemove->psPriorSorted = NULL;
}

// Finds an item based on ID in either the "all" or "sorted" list. If u64ItemID is
// LISTBOX_NOT_SELECTED, pvUserData must match
static SControlListBoxItem *ControlListBoxFindInternal(SControlListBox *psControlListBox,
													   UINT64 u64ItemID,
													   void *pvUserData,
													   BOOL bSortedList,
													   BOOL bIgnoreFiltered)
{
	SControlListBoxItem *psItem;
	BOOL bFindByUserData = FALSE;

	// If we're set to not selected, the nlook by user data
	if (LISTBOX_NOT_SELECTED == u64ItemID)
	{
		bFindByUserData = TRUE;
	}

	if (bSortedList)
	{
		// Sorted list
		psItem = psControlListBox->psItemSorted;
	}
	else
	{
		// All list
		psItem = psControlListBox->psItems;
	}

	// Run through the appropriate list
	while (psItem)
	{
		BOOL bIncludeItem;
		
		// Assume we're going to include this item
		bIncludeItem = TRUE;

		// If it's not the right ID
		if (FALSE == bFindByUserData)
		{
			if (u64ItemID != psItem->u64Index)
			{
				bIncludeItem = FALSE;
			}
		}
		else
		{
			// We're finding by user data
			if (pvUserData != psItem->pvUserData)
			{
				bIncludeItem = FALSE;
			}
		}

		// If we're ignoring filtered-out items, then it's not us
		if ((bIgnoreFiltered) && 
			(FALSE == psItem->bFilterVisible))
		{
			bIncludeItem = FALSE;
		}
		
		// If bIncludeItem is still TRUE, we found it!
		if (bIncludeItem)
		{
			return(psItem);
		}

		// Next link!
		if (bSortedList)
		{
			psItem = psItem->psNextSorted;
		}
		else
		{
			psItem = psItem->psNextLink;
		}
	}

	// Not found
	return(NULL);
}

// Adds an item to the list box. If provided, SortCompareFunction will be
// called to find out. Otherwise, the item in question will be added to the
// end of the list
EStatus ControlListBoxItemAdd(EControlListBoxHandle eControlListBoxHandle,
							  char *peText,
							  void *pvUserData,
							  UINT64 *pu64AssignedIndex,
							  INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
														  char *peTag1,
														  UINT64 u64Index1,
														  void *pvUserData1,
														  char *peTag2,
														  UINT64 u64Index2,
														  void *pvUserData2))
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem;

	// Allocate space for this item
	MEMALLOC(psItem, sizeof(*psItem));

	psItem->peText = strdupHeap(peText);
	if (NULL == psItem->peText)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	psItem->pvUserData = pvUserData;
	psItem->bFilterVisible = TRUE;

	// Now create a slider handler for this list box
	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	// Assign this the next ID
	psItem->u64Index = psControlListBox->u64NextID;
	psControlListBox->u64NextID++;

    if( pu64AssignedIndex )
    {
		*pu64AssignedIndex = psItem->u64Index;
    }

	ControlListBoxAddItemInternal(eControlListBoxHandle,
								  psControlListBox,
								  psItem,
								  SortCompareFunction);

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
	ERR_GOTO();


errorExit:
	return(eStatus);
}

// This will take an existing list and resort the entire list of items
EStatus ControlListBoxItemSort(EControlListBoxHandle eControlListBoxHandle,
							  INT8 (*SortCompareFunction)(EControlListBoxHandle eControlListBoxHandle,
														   char *peTag1,
														   UINT64 u64Index1,
														   void *pvUserData1,
														   char *peTag2,
														   UINT64 u64Index2,
														   void *pvUserData2))
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;
	SControlListBoxItem *psItemPrior = NULL;

	// Now create a slider handler for this list box
	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	// Run through the entire list and wipe out the sorted previous/next pointers
	psItem = psControlListBox->psItemSorted;
	while (psItem)
	{
		psItemPrior = psItem;
		psItem = psItem->psNextSorted;
		psItemPrior->psNextSorted = NULL;
		psItemPrior->psPriorSorted = NULL;
	}

	psControlListBox->psItemSorted = NULL;

	// Now let's readd everything and do an insertion sort on every item
	psItem = psControlListBox->psItems;
	while (psItem)
	{
		ControlListBoxAddItemSortedInternal(eControlListBoxHandle,
											psControlListBox,
											psItem,
											SortCompareFunction);
		psItem = psItem->psNextLink;
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
	ERR_GOTO();


errorExit:
	return(eStatus);
}

// This will find an item by ID or user data
EStatus ControlListBoxFind(EControlListBoxHandle eControlListBoxHandle,
						   UINT64 u64Index,
						   void *pvUserData,
						   char **ppeText,
						   void **ppvUserData,
						   UINT64 *pu64Index,
						   BOOL *pbFilterVisible)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;

	// Now create a slider handler for this list box
	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	psItem = ControlListBoxFindInternal(psControlListBox,
										u64Index,
										pvUserData,
										FALSE,
										FALSE);
	if (psItem)
	{
		if (ppeText)
		{
			*ppeText = psItem->peText;
		}

		if (ppvUserData)
		{
			*ppvUserData = psItem->pvUserData;
		}

		if (pbFilterVisible)
		{
			*pbFilterVisible = psItem->bFilterVisible;
		}

		if (pu64Index)
		{
			*pu64Index = psItem->u64Index;
		}
	}
	else
	{
		// Item not found
		eStatus = ESTATUS_UI_ITEM_NOT_FOUND;
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
errorExit:
	return(eStatus);
}

// Deletes an item out of the overall list. If u64Index is set to LISTBOX_NOT_SELECTED,
// it assumes a delete by pvUserData
EStatus ControlListBoxDeleteItem(EControlListBoxHandle eControlListBoxHandle,
								 UINT64 u64Index,
								 void *pvUserData)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;
	BOOL bFindByUserData = FALSE;
	BOOL bListBoxItemsDestroyed = FALSE;
	SControlListBoxItem *psOldSelected = NULL;

	// Now create a slider handler for this list box
	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 &psControl,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	psOldSelected = psControlListBox->psItemSelected;

	if (LISTBOX_NOT_SELECTED == u64Index)
	{
		// Look for it by user data
		bFindByUserData = TRUE;
	}

	if ((LISTBOX_NOT_SELECTED == u64Index) &&
		(NULL == pvUserData))
	{
		// This means delete everything
		while (psControlListBox->psItems)
		{
			psItem = psControlListBox->psItems;

			// Delete from both lists
			ControlListBoxRemoveItem(psControlListBox,
									 psItem);
			ControlListBoxRemoveItemSorted(psControlListBox,
										   psItem);

			if (psItem->bFilterVisible)
			{
				BASSERT(psControlListBox->u32FilteredItems);
				psControlListBox->u32FilteredItems--;
			}

			ListBoxItemDestroy(psItem);

			bListBoxItemsDestroyed = TRUE;
			BASSERT(psControlListBox->u32TotalItems);
			--psControlListBox->u32TotalItems;
		}

		BASSERT(0 == psControlListBox->u32TotalItems);
		BASSERT(0 == psControlListBox->u32FilteredItems);
		psControlListBox->psItemSelected = NULL;
		psControlListBox->psItemTopOfVisibleList = NULL;
	}
	else
	{
		psItem = ControlListBoxFindInternal(psControlListBox,
											u64Index,
											pvUserData,
											FALSE,
											FALSE);

		if (psItem)
		{
			// Delete from both lists
			ControlListBoxRemoveItem(psControlListBox,
									 psItem);
			ControlListBoxRemoveItemSorted(psControlListBox,
										   psItem);

			// Adjust item counts
			if (psItem->bFilterVisible)
			{
				BASSERT(psControlListBox->u32FilteredItems);
				psControlListBox->u32FilteredItems--;

				eStatus = ControlSliderSetRange(psControlListBox->eSelectSlider,
												0,
												psControlListBox->u32FilteredItems - 1);
				ERR_GOTO();
			}

			// And total count
			BASSERT(psControlListBox->u32TotalItems);
			psControlListBox->u32TotalItems--;

			// Make sure the # of filtered items are smaller than or equal to the total # of items
			BASSERT(psControlListBox->u32FilteredItems <= psControlListBox->u32TotalItems);

			// And now deallocate the item from memory
			ListBoxItemDestroy(psItem);

			bListBoxItemsDestroyed = TRUE;
		}
		else
		{
			// Item not found
			eStatus = ESTATUS_UI_ITEM_NOT_FOUND;
		}
	}

	// Now force an update if the selected item has changed
    if( psOldSelected != psControlListBox->psItemSelected )
    {
		ControlListBoxProcessCallback(psControl,
									  ECTRLACTION_ENTER,
									  psControlListBox,
									  psControlListBox->psItemSelected);
    }

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);

	if (bListBoxItemsDestroyed)
	{
		WindowUpdated();
	}

errorExit:
	return(eStatus);
}

// Run through the entire list and set filtered items. Calling with
// FilterVisibleCheck set to NULL will make everything visible.
EStatus ControlListBoxItemSetFilters(EControlListBoxHandle eControlListBoxHandle,
									 BOOL bFilteredList,
									 void *pvTempUserData,
									 BOOL (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																void *pvTempUserData,
																char *peText,
																UINT64 u64Index,
																void *pvUserData))
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;
	BOOL bVisibilityChanged = FALSE;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	// Run through all items and filter it.
	psItem = psControlListBox->psItems;
	psControlListBox->u32FilteredItems = 0;

	while (psItem)
	{
		BOOL bOldFilterVisible;

		bOldFilterVisible = psItem->bFilterVisible;
		if (FilterVisibleCheck)
		{
			psItem->bFilterVisible = FilterVisibleCheck(eControlListBoxHandle,
														pvTempUserData,
														psItem->peText,
														psItem->u64Index,
														psItem->pvUserData);
		}
		else
		{
			psItem->bFilterVisible = TRUE;
		}

		// If it's visible via the filter, increment the count
		if (psItem->bFilterVisible)
		{
			UINT32 u32FilteredItems = 0;

			++psControlListBox->u32FilteredItems;
			BASSERT(psControlListBox->u32FilteredItems <= psControlListBox->u32TotalItems);

			if (psControlListBox->u32FilteredItems)
			{
				u32FilteredItems = psControlListBox->u32FilteredItems - 1;
			}

			eStatus = ControlSliderSetRange(psControlListBox->eSelectSlider,
											0,
											u32FilteredItems);
			ERR_GOTO();
		}

		// If our visibility has changed, then record it for later
		if (psItem->bFilterVisible != bOldFilterVisible)
		{
			bVisibilityChanged = TRUE;
		}

		psItem = psItem->psNextLink;
	}

	// If our visibility changed, then rerender the list
	if (bVisibilityChanged)
	{
		psControlListBox->bTextOverlayRender = TRUE;
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
errorExit:
	return(eStatus);
}

// Sets one of many parameters depending upon the caller
static EStatus ControlListBoxItemSetInternal(EControlListBoxHandle eControlListBoxHandle,
											 UINT64 u64Index,
											 char **ppeText,
											 void **ppvUserData,
											 BOOL *pbFilterVisible)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;
	BOOL bVisibilityChanged = FALSE;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	psItem = ControlListBoxFindInternal(psControlListBox,
										u64Index,
										NULL,
										FALSE,
										FALSE);

	if (psItem)
	{
		// Found it! Let's set whatever we wanted to
		if (ppeText)
		{
			if (psItem->peText != *ppeText)
			{
				psControlListBox->bTextOverlayRender = TRUE;
			}

			SafeMemFree(psItem->peText);
			if (*ppeText)
			{
				psItem->peText = strdupHeap(*ppeText);
			}
		}

		if (ppvUserData)
		{
			if (psItem->pvUserData != *ppvUserData)
			{
				psControlListBox->bTextOverlayRender = TRUE;
			}

			psItem->pvUserData = *ppvUserData;
		}

		if (pbFilterVisible)
		{
			if (psItem->bFilterVisible != *pbFilterVisible)
			{
				psControlListBox->bTextOverlayRender = TRUE;
			}

			psItem->bFilterVisible = *pbFilterVisible;
		}
	}
	else
	{
		// Item not found
		eStatus = ESTATUS_UI_ITEM_NOT_FOUND;
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
errorExit:
	return(eStatus);
}

// Sets a list box item's text
EStatus ControlListBoxItemSetText(EControlListBoxHandle eControlListBoxHandle,
								  UINT64 u64Index,
								  char *peText)
{
	return(ControlListBoxItemSetInternal(eControlListBoxHandle,
										 u64Index,
										 &peText,
										 NULL,
										 NULL));
}

// Sets a list box item's user data
EStatus ControlListBoxItemSetUserData(EControlListBoxHandle eControlListBoxHandle,
									  UINT64 u64Index,
									  void *pvUserData)
{
	return(ControlListBoxItemSetInternal(eControlListBoxHandle,
										 u64Index,
										 NULL,
										 &pvUserData,
										 NULL));
}

// Sets a list box item's filter visibility
EStatus ControlListBoxItemSetFilterVisible(EControlListBoxHandle eControlListBoxHandle,
										   UINT64 u64Index,
										   BOOL bFilterVisible)
{
	return(ControlListBoxItemSetInternal(eControlListBoxHandle,
										 u64Index,
										 NULL,
										 NULL,
										 &bFilterVisible));
}

EStatus ControlListBoxDump(EControlListBoxHandle eControlListBoxHandle)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControlListBoxItem *psItem = NULL;
	SControlListBoxItem *psItemSorted = NULL;
	BOOL bVisibilityChanged = FALSE;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	psItem = psControlListBox->psItems;
	psItemSorted = psControlListBox->psItemSorted;

	while (psItem || psItemSorted)
	{
		if (psItem && psItemSorted)
		{
			DebugOutFunc("Item='%-30s'  Sorted='%-30s'\n", psItem->peText, psItemSorted->peText);
		}
		else
		if (psItem)
		{
			DebugOutFunc("Item='%-30s'\n", psItem->peText);
		}
		else
		if (psItemSorted)
		{
			DebugOutFunc("Item='                              '  Sorted='%-30s'\n", psItemSorted->peText);
		}

		if (psItem)
		{
			psItem = psItem->psNextLink;
		}

		if (psItemSorted)
		{
			psItemSorted = psItemSorted->psNextSorted;
		}
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
errorExit:
	return(eStatus);
}

// Render the separators on the provided image
static void RenderSeparators(SControlListBoxConfig *psControlListBoxConfig,
							 UINT32 *pu32RGBA,
							 UINT32 u32XControlSize,
							 UINT32 u32YControlSize,
							 INT32 *ps32XTextBase,
							 INT32 *ps32YTextBase,
							 UINT32 *pu32TextSpacing,
							 UINT32 *pu32XSeparatorSize,
							 UINT32 u32YFontSize)
{
	UINT32 u32XSize = u32XControlSize;
	UINT32 u32LineCount = psControlListBoxConfig->u32Lines;
	UINT32 u32XSeparatorSize;

	// Subtract out the edge thicknesses
	u32XSize -= (psControlListBoxConfig->u32EdgeYSize << 1);
	
	// And the offset
	u32XSize -= (psControlListBoxConfig->u32SeparatorXOffset << 1);

	if (ps32XTextBase)
	{
		*ps32XTextBase = (psControlListBoxConfig->u32EdgeYSize + psControlListBoxConfig->u32SeparatorXOffset);
	}

	if (ps32YTextBase)
	{
		*ps32YTextBase = (psControlListBoxConfig->u32EdgeYSize + psControlListBoxConfig->u32SeparatorSpacing);
	}

	if (pu32TextSpacing)
	{
		*pu32TextSpacing = (psControlListBoxConfig->u32SeparatorSpacing << 1) + u32YFontSize + psControlListBoxConfig->u32SeparatorThickness;
	}

	u32XSeparatorSize = u32XControlSize - ((psControlListBoxConfig->u32EdgeYSize << 1) + (psControlListBoxConfig->u32SeparatorXOffset << 1));

	if (psControlListBoxConfig->pu32RGBAThumb)
	{
		u32XSeparatorSize = (u32XSeparatorSize + psControlListBoxConfig->u32EdgeYSize) - psControlListBoxConfig->u32YSizeThumb;
	}

	if (pu32XSeparatorSize)
	{
		*pu32XSeparatorSize = u32XSeparatorSize;
	}

	// Find the starting location
	pu32RGBA += (psControlListBoxConfig->u32EdgeYSize + psControlListBoxConfig->u32SeparatorXOffset) +	// Adjust to the right
				((psControlListBoxConfig->u32EdgeYSize + psControlListBoxConfig->u32SeparatorSpacing + u32YFontSize) * u32XControlSize);

	BASSERT(u32LineCount);
	u32LineCount--;

	while (u32LineCount)
	{
		UINT32 *pu32RGBATemp;
		UINT32 u32Loop;
		UINT32 u32Loop2;

		for (u32Loop = 0; u32Loop < psControlListBoxConfig->u32SeparatorThickness; u32Loop++)
		{
			pu32RGBATemp = pu32RGBA + (u32Loop * u32XControlSize);
			for (u32Loop2 = 0; u32Loop2 < u32XSeparatorSize; u32Loop2++)
			{
				*pu32RGBATemp = psControlListBoxConfig->u32RGBASeparatorColor;
				++pu32RGBATemp;
			}
		}

		pu32RGBA += ((psControlListBoxConfig->u32SeparatorSpacing << 1) + u32YFontSize + psControlListBoxConfig->u32SeparatorThickness) * u32XControlSize;
		u32LineCount--;
	}
}

// Called when this control's subordinate slider calls us back
static EStatus ControlListBoxSliderCallback(EControlSliderHandle eControlSliderHandle,
											void *pvUserData,
											BOOL bDrag,
											BOOL bSet,
											INT32 s32Value)
{
	SControl *psControl = NULL;
	SControlListBox *psControlListBox = NULL;
	EControlListBoxHandle eControlListBoxHandle = (EControlListBoxHandle) pvUserData;
	EStatus eStatus = ESTATUS_OK;
	BOOL bLocked = FALSE;
	BOOL bUpdated = FALSE;
	SControlListBoxItem *psItem;

	// If someone is setting the slider value, ignore it
	if (bSet)
	{
		goto errorExit;
	}

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 &psControl,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	// If we aren't setting and not dragging...
	if ((FALSE == bSet) &&
		(FALSE == bDrag))
	{
		// Set the focus to the list box select if we've released the slider
		eStatus = WindowControlSetFocus(psControl->eWindowHandle,
										psControl->eControlHandle);
		ERR_GOTO();
	}


	// If there are no items in the list, just return
	if (NULL == psControlListBox->psItemSorted)
	{
		goto errorExit;
	}

	// Find the "s32Value"th item
	psItem = ControlListBoxFindVisible(psControlListBox->psItemSorted,
										(UINT32) s32Value,
										TRUE);
	// This should never happen
	BASSERT(psItem);

	if (psItem != psControlListBox->psItemTopOfVisibleList)
	{
		psControlListBox->psItemTopOfVisibleList = psItem;
		bUpdated = TRUE;
	}


	// If we've hit the end of the page before the end of the list, then
	// reset the top of visible list to the most recent that'd fill the
	// page
	if (NULL == ControlListBoxFindVisible(psControlListBox->psItemTopOfVisibleList,
											psControlListBox->u32Lines - 1,
											TRUE))
	{
		// Find the last item in the list
		psItem = ControlListBoxFindVisible(psControlListBox->psItemSorted,
											psControlListBox->u32FilteredItems - 1,
											TRUE);

		BASSERT(psItem);

		// Now work back a page
		psItem = ControlListBoxFindVisible(psItem,
											psControlListBox->u32Lines - 1,
											FALSE);
		if (NULL == psItem)
		{
			// Make this the top of the list
			psItem = psControlListBox->psItemSorted;
		}

		psControlListBox->psItemTopOfVisibleList = psItem;
	}

errorExit:
	if (bUpdated)
	{
		// Update everything!
		psControlListBox->bTextOverlayRender = TRUE;
		WindowUpdated();
	}

	if (bLocked)
	{
		eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
											 FALSE,
											 NULL,
											 NULL,
											 eStatus);
	}


	return(ESTATUS_OK);
}

// This configures a list box for operation (or reconfigures it)
EStatus ControlListBoxSetAssets(EControlListBoxHandle eControlListBoxHandle,
								SControlListBoxConfig *psControlListBoxConfig)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;
	SControlListBox *psControlListBox = NULL;
	EFontHandle eFont;
	UINT16 u16FontSize;
	BOOL bLocked = FALSE;
	BOOL bTextOverlayRender = FALSE;
	UINT32 u32YFontSize;
	UINT32 u32XSize = 0;
	UINT32 u32YSize = 0;
	UINT32 *pu32RGBAControlGuide = NULL;
	UINT32 u32Lines;

	u32Lines = psControlListBoxConfig->u32Lines;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 &psControl,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	// Get our existing font handle and size
	eFont = psControlListBox->eFont;
	u16FontSize = psControlListBox->u16FontSize;

	if ((psControlListBox->eFont != psControlListBoxConfig->eFont) ||
		(psControlListBox->u16FontSize != psControlListBoxConfig->u16FontSize))
	{
		eFont = psControlListBoxConfig->eFont;
		u16FontSize = psControlListBoxConfig->u16FontSize;
		bTextOverlayRender = TRUE;
	}

	u32XSize = psControlListBoxConfig->u32XSize;
	u32YSize = psControlListBoxConfig->u32YSize;

	// Get some font info
	eStatus = FontGetCharInfo(eFont,
							  u16FontSize,
							  (EUnicode) ' ',
							  NULL,
							  NULL,
							  NULL,
							  &u32YFontSize);
	ERR_GOTO();

	// We're deriving the Y size from the # of lines we want
	if (0 == u32YSize)
	{
		u32YSize = (psControlListBoxConfig->u32EdgeYSize << 1) + 
				   ((u32YFontSize + psControlListBoxConfig->u32SeparatorThickness + (psControlListBoxConfig->u32SeparatorSpacing << 1)) * (u32Lines - 1)) + 
				   (psControlListBoxConfig->u32SeparatorSpacing << 1) + u32YFontSize;
	}

	// Corners need to be filled in
	if ((NULL == psControlListBoxConfig->pu32RGBACornerImage) ||
		(0 == psControlListBoxConfig->u32CornerXSize) ||
		(0 == psControlListBoxConfig->u32CornerYSize))
	{
		eStatus = ESTATUS_UI_IMAGE_MISSING;
		goto errorExit;
	}

	// Edge needs to be filled in
	if ((NULL == psControlListBoxConfig->pu32RGBAEdgeImage) ||
		(0 == psControlListBoxConfig->u32EdgeXSize) ||
		(0 == psControlListBoxConfig->u32EdgeYSize))
	{
		eStatus = ESTATUS_UI_IMAGE_MISSING;
		goto errorExit;
	}

	// Render the control guide
	eStatus = WindowRenderStretch(psControlListBoxConfig->pu32RGBACornerImage,
								  psControlListBoxConfig->u32CornerXSize,
								  psControlListBoxConfig->u32CornerYSize,
								  psControlListBoxConfig->pu32RGBAEdgeImage,
								  psControlListBoxConfig->u32EdgeXSize,
								  psControlListBoxConfig->u32EdgeYSize,
								  psControlListBoxConfig->u32RGBAFillPixel,
								  u32XSize,
								  u32YSize,
								  &pu32RGBAControlGuide);
	ERR_GOTO();

	// Render the separators
	RenderSeparators(psControlListBoxConfig,
					 pu32RGBAControlGuide,
					 u32XSize,
					 u32YSize,
					 &psControlListBox->s32XTextBase,
					 &psControlListBox->s32YTextBase,
					 &psControlListBox->u32TextSpacing,
					 &psControlListBox->u32XSeparatorSize,
					 u32YFontSize);

	// Ditch the existing control guide (if there is one)
	GraphicsClearImage(psControlListBox->psControlGuide);

	// Set our new control guide
	GraphicsSetImage(psControlListBox->psControlGuide,
					 pu32RGBAControlGuide,
					 u32XSize,
					 u32YSize,
					 TRUE);

	// Now the text
	GraphicsClearImage(psControlListBox->psTextOverlay);

	// And now set the text overlay - subtract off the 
	eStatus = GraphicsAllocateImage(psControlListBox->psTextOverlay,
									u32XSize,
									u32YSize);

	// Need to rerender
	psControlListBox->bTextOverlayRender = TRUE;

	// Copy in the # of lines we're displaying
	psControlListBox->u32Lines = u32Lines;

	// And the text color
	psControlListBox->u32TextColor = psControlListBoxConfig->u32TextColor;

	// And the font size
	psControlListBox->eFont = eFont;
	psControlListBox->u16FontSize = u16FontSize;

	// And text offset
	psControlListBox->s32XTextOffset = psControlListBoxConfig->s32XTextOffset;
	psControlListBox->s32YTextOffset = psControlListBoxConfig->s32YTextOffset;

	// Set the new control's size
	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	// Now render a slider if we have one
	if (psControlListBoxConfig->pu32RGBALeft && 
		psControlListBoxConfig->pu32RGBACenter && 
		psControlListBoxConfig->pu32RGBARight && 
		psControlListBoxConfig->pu32RGBAThumb)
	{
		UINT32 *pu32RGBATrack;
		UINT32 u32YSliderSize;
		SControlSliderConfig sSliderConfig;
		UINT32 u32FilteredItems = 0;

		ZERO_STRUCT(sSliderConfig);

		// Time to render the slider graphic
		eStatus = ControlRenderStretch(psControlListBoxConfig->pu32RGBALeft,
									   psControlListBoxConfig->u32XSizeLeft,
									   psControlListBoxConfig->u32YSizeLeft,
									   psControlListBoxConfig->pu32RGBACenter,
									   psControlListBoxConfig->u32XSizeCenter,
									   psControlListBoxConfig->u32YSizeCenter,
									   psControlListBoxConfig->pu32RGBARight,
									   psControlListBoxConfig->u32XSizeRight,
									   psControlListBoxConfig->u32YSizeRight,
									   psControl->u32YSize - (psControlListBox->s32YTextBase << 1),
									   &u32YSliderSize,
									   &pu32RGBATrack);
		ERR_GOTO();

		sSliderConfig.u32TrackLength = psControl->u32YSize - (psControlListBox->s32YTextBase << 1);
		sSliderConfig.pu32RGBATrack = pu32RGBATrack;
		sSliderConfig.u32XSizeTrack = psControl->u32YSize - (psControlListBox->s32YTextBase << 1);
		sSliderConfig.u32YSizeTrack = u32YSliderSize;
		sSliderConfig.pu32RGBAThumb = psControlListBoxConfig->pu32RGBAThumb;
		sSliderConfig.u32XSizeThumb = psControlListBoxConfig->u32XSizeThumb;
		sSliderConfig.u32YSizeThumb = psControlListBoxConfig->u32YSizeThumb;
		sSliderConfig.bAutoDeallocate = TRUE;

		// Now set the assets with the slider
		eStatus = ControlSliderSetAssets(psControlListBox->eSelectSlider,
										 &sSliderConfig);
		ERR_GOTO();

		// Now rotate the slider 90 degrees
		eStatus = ControlSetAngle((EControlHandle) psControlListBox->eSelectSlider,
								  90.0);
		ERR_GOTO();

		// Set slider range
		if (psControlListBox->u32FilteredItems)
		{
			u32FilteredItems = psControlListBox->u32FilteredItems - 1;
		}

		eStatus = ControlSliderSetRange(psControlListBox->eSelectSlider,
										0,
										u32FilteredItems);
		ERR_GOTO();

		// Set the slider's position so it's relative/on top of us
		eStatus = ControlSetPosition((EControlHandle) psControlListBox->eSelectSlider,
									 (psControl->s32XPos + psControl->u32XSize),
									 psControl->s32YPos);
		ERR_GOTO();

		// Now set the slider's callback for the listbox so we can move our list around
		eStatus = ControlSliderSetCallback(psControlListBox->eSelectSlider,
										   (void *) eControlListBoxHandle,
										   ControlListBoxSliderCallback);
		ERR_GOTO();
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
											 FALSE,
											 NULL,
											 NULL,
											 eStatus);
	}

	// Update everything!
	WindowUpdated();

	return(eStatus);
}

EStatus ControlListBoxSetCallback(EControlListBoxHandle eControlListBoxHandle,
								  void *pvUserData,
								  void (*SelectCallback)(EControlListBoxHandle eControlListBoxHandle,
														 void *pvUserData,
														 EControlCallbackReason eCallbackReason,
														 char *peText,
														 UINT64 u64Index,
														 void *pvItemUserData))
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	BOOL bVisibilityChanged = FALSE;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 NULL,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();

	// Set up the callback
	psControlListBox->pvUserData = pvUserData;
	psControlListBox->SelectCallback = SelectCallback;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 FALSE,
										 NULL,
										 NULL,
										 eStatus);
errorExit:
	return(eStatus);
}

// This sets the selected item in a list box. If u64Index is set to LISTBOX_NOT_SELECTED
// and pvUserData is set to NULL, the listbox's item will be deselected entirely
EStatus ControlListBoxSetSelected(EControlListBoxHandle eControlListBoxHandle,
								  UINT64 u64Index,
								  void *pvUserData,
								  BOOL bfilterVisible)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControl *psControl = NULL;
	SControlListBoxItem *psControlListBoxItem = NULL;
	BOOL bLocked = FALSE;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 &psControl,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlListBoxItem = ControlListBoxFindInternal(psControlListBox,
													  u64Index,
													  pvUserData,
													  bfilterVisible,
													  FALSE);
	ERR_GOTO();

	if (psControlListBox->psItemSelected != psControlListBoxItem)
	{
		// It's a change!
		psControlListBox->psItemSelected = psControlListBoxItem;

		// Now force an update
		ControlListBoxProcessCallback(psControl,
									  ECTRLACTION_ENTER,
									  psControlListBox,
									  psControlListBox->psItemSelected);

		// Things changed. Time for an update
		psControlListBox->bTextOverlayRender = TRUE;
		WindowUpdated();
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
											 FALSE,
											 NULL,
											 NULL,
											 eStatus);
	}
	return(eStatus);
}

// This will get the currently selected list box item and will make a copy of the tag
// if ppeTag is non-NULL.
EStatus ControlListBoxGetSelected(EControlListBoxHandle eControlListBoxHandle,
								  UINT64 *pu64Index,
								  char **ppeTag,
								  void **ppvUserData)
{
	EStatus eStatus = ESTATUS_OK;
	SControlListBox *psControlListBox = NULL;
	SControl *psControl = NULL;
	SControlListBoxItem *psControlListBoxItem = NULL;
	BOOL bLocked = FALSE;
	UINT64 u64Index;

	eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
										 TRUE,
										 &psControl,
										 &psControlListBox,
										 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	if (NULL == psControlListBox->psItemSelected)
	{
		u64Index = LISTBOX_NOT_SELECTED;
	}
	else
	{
		u64Index = psControlListBox->psItemSelected->u64Index;

		if (ppvUserData)
		{
			*ppvUserData = psControlListBox->psItemSelected->pvUserData;
		}

		if (ppeTag)
		{
			*ppeTag = strdupHeap(psControlListBox->psItemSelected->peText);
			if (NULL == *ppeTag)
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				goto errorExit;
			}
		}
	}

	if (pu64Index)
	{
		*pu64Index = u64Index;
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlListBoxLockPointers(eControlListBoxHandle,
											 FALSE,
											 NULL,
											 NULL,
		
											 eStatus);
	}
	return(eStatus);}


EStatus ControlListBoxInit(void)
{
	return(ControlRegister(sg_sControlListBoxMethods.eType,
						   &sg_sControlListBoxMethods));
}