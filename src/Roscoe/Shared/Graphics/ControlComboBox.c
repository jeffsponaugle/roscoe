#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"
#include "Shared/Graphics/Font.h"

// Combo box structure
typedef struct SControlComboBox
{
	// Font handle/size used for everything
	EFontHandle eFont;
	UINT16 u16FontSize;
	UINT32 u32RGBATextSelectedColor;

	// Selected text display
	EControlTextHandle eSelectedText;			// Text control for selection
	char *peTextNotSelected;					// Text to display when nothing selected

	// Offset for both selected text and line input
	INT32 s32XSelectedOffset;
	INT32 s32YSelectedOffset;

	// Selected item
	char *peSelectedItemText;
	UINT64 u64SelectedItemIndex;
	void *pvItemUserData;

	// Function to call back when a selection has been made
	void *pvCallbackUserData;
	void (*SelectCallback)(EControlComboBoxHandle eControlComboBoxHandle,
						   void *pvCallbackUserData,
						   EControlCallbackReason eCallbackReason,
						   char *peText,
						   UINT64 u64Index,
						   void *pvItemUserData);

	// Line input control (filter)
	BOOL bLineInputDisplayed;					// Set TRUE if the line input should be displayed
	EControlLineInputHandle eFilterInput;		// Filter text input
	UINT32 u32FilterMaxCharInput;				// Max # of chars to input (or 0 if disabled)

	// List open/close button
	EControlButtonHandle eListOpenButton;		// List open button

	// And the hit test for clicking on the text to drop the list
	EControlHitHandle eHitHandle;				// Hit handle (for text)

	// List box
	BOOL bListBoxShown;							// TRUE If the list box is shown
	EControlListBoxHandle eListBox;				// List box!
} SControlComboBox;

static BOOL ControlComboBoxListBoxFilter(EControlListBoxHandle eControlListBoxHandle,
										 void *pvTempUserData,
										 char *peText,
										 UINT64 u64Index,
										 void *pvUserData)
{
	// peText = text of the item
	// pvTempUserData = text of the filter

	if (Sharedstrcasestr(peText, (const char *) pvTempUserData))
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}

// Called when the filter input characters have changed. pvUserData is the combo box handle
static EStatus ControlComboBoxLineInputCallback(EControlLineInputHandle eLineInputHandle,
												void *pvUserData,
												EControlCallbackReason eLineInputCallbackReason,
												char *peText,
												UINT32 u32CharCount,
												UINT32 u32CharSizeBytes)
{
	EStatus eStatus;

	eStatus = ControlComboBoxItemSetFilters((EControlComboBoxHandle) pvUserData,
											TRUE,
											peText,
											ControlComboBoxListBoxFilter);

	return(eStatus);
}

// Called to update either the selected text or the input/edit box
static EStatus ControlComboBoxUpdateSelectedAndInput(SControl *psControl,
													 SControlComboBox *psControlComboBox)
{
	EStatus eStatus;
	BOOL bVisible;
	char *peSelectedText = NULL;

	// Set our visibility base on this control's visibility
	bVisible = psControl->bVisible;

	if (psControlComboBox->bLineInputDisplayed)
	{
		// Show the filter input line edit
		eStatus = ControlSetVisible((EControlHandle) psControlComboBox->eFilterInput,
									bVisible);
		ERR_GOTO();

		// Hide the text field
		eStatus = ControlSetVisible((EControlHandle) psControlComboBox->eSelectedText,
									 FALSE);
	}
	else
	{
		// Hide the filter input line edit
		eStatus = ControlSetVisible((EControlHandle) psControlComboBox->eFilterInput,
									FALSE);
		ERR_GOTO();

		// Show the text field
		eStatus = ControlSetVisible((EControlHandle) psControlComboBox->eSelectedText,
									 bVisible);
	}

	ERR_GOTO();

	if (LISTBOX_NOT_SELECTED == psControlComboBox->u64SelectedItemIndex)
	{
		// Select the "not selected" text
		peSelectedText = psControlComboBox->peTextNotSelected;
	}
	else
	{
		peSelectedText = psControlComboBox->peSelectedItemText;
	}

	// Now set the text
	eStatus = ControlTextSet(psControlComboBox->eSelectedText,
							 psControlComboBox->eFont,
							 psControlComboBox->u16FontSize,
							 peSelectedText,
							 TRUE,
							 psControlComboBox->u32RGBATextSelectedColor);
	ERR_GOTO();

	// And set the list box
	eStatus = ControlSetVisible((EControlHandle) psControlComboBox->eListBox,
								psControlComboBox->bListBoxShown);

errorExit:
	return(eStatus);
}

// This clears the combo box filter line input
static EStatus ControlComboBoxClearFilter(SControl *psControl,
										  SControlComboBox *psControlComboBox)
{
	EStatus eStatus;

	psControlComboBox->bLineInputDisplayed = FALSE;

	// Clear filter
	eStatus = ControlLineInputSetText(psControlComboBox->eFilterInput,
									  "",
									  psControlComboBox->u32FilterMaxCharInput);
	ERR_GOTO();

	// Make everything visible again
	eStatus = ControlComboBoxLineInputCallback(psControlComboBox->eFilterInput,
											   (void *) psControl->eControlHandle,
											   ECTRLACTION_TEXTCHANGED,
											   "",
											   0,
											   0);
	ERR_GOTO();

	// Turn off the cursor
	eStatus = ControlLineInputSetCursorOverride(psControlComboBox->eFilterInput,
												FALSE);

errorExit:
	return(eStatus);
}

// Called when someone wants to create a combo box control
static EStatus ControlComboBoxCreateMethod(SControl *psControl,
										   void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlComboBox *psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// Make it focusable by default
	psControl->bFocusable = TRUE;

	// And nothing selected
	psControlComboBox->u64SelectedItemIndex = LISTBOX_NOT_SELECTED;
	
	return(eStatus);
}

// Called when this control is enabled or disabled
static EStatus ControlComboBoxSetDisableMethod(SControl *psControl,
											   BOOL bDisabledBefore,
											   BOOL bDisabled)
{
	EStatus eStatus;
	SControlComboBox *psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// Disable/enable the subordinate controls
	eStatus = ControlSetDisable(psControlComboBox->eSelectedText,
								bDisabled);
	ERR_GOTO();

	eStatus = ControlSetDisable(psControlComboBox->eFilterInput,
								bDisabled);
	ERR_GOTO();

	eStatus = ControlSetDisable(psControlComboBox->eListOpenButton,
								bDisabled);
	ERR_GOTO();

	eStatus = ControlSetDisable(psControlComboBox->eListBox,
								bDisabled);
	ERR_GOTO();

	// If we're disabling the control and the list box is shown, drop it
	if ((bDisabled) &&
		(psControlComboBox->bListBoxShown))
	{
		psControlComboBox->bListBoxShown = FALSE;

		eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
														psControlComboBox);
	}

errorExit:
	return(eStatus);
}

static EStatus ControlComboBoxSetVisibleMethod(SControl *psControl,
											   BOOL bVisible)
{
	EStatus eStatus;
	SControlComboBox *psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// Set all the subordinate controls' visibility, too

	// Filter input box
	(void) ControlSetVisible((EControlHandle) psControlComboBox->eFilterInput,
							 bVisible);

	// List open button
	(void) ControlSetVisible((EControlHandle) psControlComboBox->eListOpenButton,
							 bVisible);

	// List box
	if (psControlComboBox->bListBoxShown &&
		bVisible)
	{
		bVisible = TRUE;
	}
	else
	{
		bVisible = FALSE;
	}

	(void) ControlSetVisible((EControlHandle) psControlComboBox->eListBox,
							 bVisible);

	eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
													psControlComboBox);

	return(eStatus);
}

// List of methods for this control
static SControlMethods sg_sControlComboBoxMethods =
{
	ECTRLTYPE_COMBOBOX,							// Type of control
	"Combo box",								// Name of control
	sizeof(SControlComboBox),					// Size of control specific structure

	ControlComboBoxCreateMethod,				// Create a control
	NULL,										// Destroy a control
	ControlComboBoxSetDisableMethod,			// Control enable/disable
	NULL,										// New position
	ControlComboBoxSetVisibleMethod,			// Set visible
	NULL,										// Set angle
	NULL,										// Draw the control
	NULL,										// Runtime configuration
	NULL,										// Hit test
	NULL,										// Focus set/loss
	NULL,										// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	NULL,										// UTF8 Input
	NULL,										// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

// Called when this combo box's button is pressed or released
static void ControlComboBoxButtonCallback(EControlButtonHandle eButtonHandle,
										  EControlCallbackReason eReason,
										  UINT32 u32ReasonData,
										  INT32 s32ControlRelativeXPos,
										  INT32 s32ControlRelativeYPos)
{
	EStatus eStatus;
	EControlComboBoxHandle eControlComboBoxHandle;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox;
	BOOL bLocked = FALSE;

	// Get the user data, as it points to the parent control ID
	eStatus = ControlGetUserData((EControlHandle) eButtonHandle,
								 (void **) &eControlComboBoxHandle);
	BASSERT(ESTATUS_OK == eStatus);

	// Got the combo box handle. Now we need the combo box structure.
	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	if (u32ReasonData & BUTTON_PRESSED)
	{
		// Need to toggle whether or not we're visible.
		if (psControlComboBox->bListBoxShown)
		{
			// Hide it
			psControlComboBox->bListBoxShown = FALSE;

			eStatus = ControlComboBoxClearFilter(psControl,
												 psControlComboBox);
			ERR_GOTO();
		}
		else
		{
			// Show it
			psControlComboBox->bListBoxShown = TRUE;
		}

		eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
														psControlComboBox);
	}
	else
	if (psControlComboBox->bListBoxShown)
	{
		// If the list box is shown, then we want to process the release and
		// set the focus on to the list box instead of the button
		eStatus = WindowControlSetFocus(psControl->eWindowHandle,
										psControlComboBox->eListBox);
	}

errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eControlComboBoxHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
}

// Called when the list box is updated
static void ComboBoxListBoxCallback(EControlListBoxHandle eControlListBoxHandle,
									void *pvUserData,
									EControlCallbackReason eCallbackReason,
									char *peText,
									UINT64 u64Index,
									void *pvItemUserData)
{
	EStatus eStatus;
	EControlComboBoxHandle eControlComboBoxHandle;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox;
	BOOL bLocked = FALSE;
	BOOL bUpdated = FALSE;

	// Get the user data, as it points to the parent control ID
	eStatus = ControlGetUserData((EControlHandle) eControlListBoxHandle,
								 (void **) &eControlComboBoxHandle);
	BASSERT(ESTATUS_OK == eStatus);

	// Got the combo box handle. Now we need the combo box structure.
	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// If the user hits escape
	if (ECTRLACTION_ESCAPE == eCallbackReason)
	{
		// User hit escape. Hide the list box.
		if (psControlComboBox->bListBoxShown)
		{
			psControlComboBox->bListBoxShown = FALSE;
			bUpdated = TRUE;
		}
	}

	// If the user hits enter - they've selected it
	if ((ECTRLACTION_ENTER == eCallbackReason) ||
		(ECTRLACTION_ESCAPE == eCallbackReason) ||
		(ECTRLACTION_MOUSEBUTTON == eCallbackReason))
	{
		if ((ECTRLACTION_ENTER == eCallbackReason) ||
			(ECTRLACTION_MOUSEBUTTON == eCallbackReason))
		{
			SafeMemFree(psControlComboBox->peSelectedItemText);

            if( peText )
            {
				psControlComboBox->peSelectedItemText = strdupHeap(peText);
				if (NULL == psControlComboBox->peSelectedItemText)
				{
					eStatus = ESTATUS_OUT_OF_MEMORY;
					goto errorExit;
				}
            }

			psControlComboBox->u64SelectedItemIndex = u64Index;
			psControlComboBox->pvItemUserData = pvItemUserData;

			if (psControlComboBox->SelectCallback)
			{
				psControlComboBox->SelectCallback(eControlComboBoxHandle,
												  psControlComboBox->pvCallbackUserData,
												  eCallbackReason,
												  psControlComboBox->peSelectedItemText,
												  psControlComboBox->u64SelectedItemIndex,
												  psControlComboBox->pvItemUserData);

			}

		}

		eStatus = ControlComboBoxClearFilter(psControl,
												psControlComboBox);
		ERR_GOTO();

		// User hit escape. Hide the list box.
		if (psControlComboBox->bListBoxShown)
		{
			psControlComboBox->bListBoxShown = FALSE;
			bUpdated = TRUE;
		}
	}

errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eControlComboBoxHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}

	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Returned %s\n", GetErrorText(eStatus));
	}

	if (bUpdated)
	{
		eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
														psControlComboBox);
		if (eStatus != ESTATUS_OK)
		{
			SyslogFunc("Returned %s\n", GetErrorText(eStatus));
		}

		WindowUpdated();
	}
}

// Called for focus changes/interception of list box. psControl points to the
// combo box structure.
static EStatus ControComboBoxFocusInterceptMethod(SControl *psControl,					// Called when this control gains/loses focus
												  EControlHandle eControlHandleOld,
												  EControlHandle eControlHandleNew,
												  UINT8 u8ButtonMask,
												  INT32 s32WindowRelativeX,
												  INT32 s32WindowRelativeY)
{
	EStatus eStatus = ESTATUS_OK;
	BOOL bHideListBox = FALSE;
	SControlComboBox *psControlComboBox;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// This had better be a list box
	BASSERT(ECTRLTYPE_COMBOBOX == psControl->psMethods->eType);

	// This means they've clicked somewhere else
	if (FALSE == ControlHandleIsValid(eControlHandleNew))
	{
		bHideListBox = TRUE;
	}
	else
	{
		// If this is one of our controls, then allow it
		if (ControlIsChild(psControl,
						   eControlHandleNew))
		{
			// It's one of our children. Intercept it.
			eStatus = ESTATUS_OK;
		}
		else
		{
			// Not one of ours - handle it
			eStatus = ESTATUS_UI_UNHANDLED;
			bHideListBox = TRUE;
		}
	}

	// If we want to hide the list box, do it
	if ((bHideListBox) &&
		(psControlComboBox->bListBoxShown))
	{
		EControlComboBoxHandle eControlComboBoxHandle = (EControlComboBoxHandle) psControl->pvUserData;

		// This means we need to collapse the list box if it's dropped.
		if (psControlComboBox->bListBoxShown)
		{
			psControlComboBox->bListBoxShown = FALSE;

			eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
															psControlComboBox);
		}
	}

	return(eStatus);
}

// Called when any subordinate control to this combo box control has a UTF8
// input. To intercept and not pass to the subordinate controls, return
// ESTATUS_OK. To pass it along, return ESTATUS_UI_UNHANDLED.
static EStatus ControlComboBoxUTF8InterceptMethod(struct SControl *psControl,		// UTF8 input from keyboard
												  EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call
												  char *peUTF8Character)
{
	EStatus eStatus;
	SControl *psControlComboBox = NULL;
	SControlComboBox *psControlComboBoxSpecific = NULL;

	// Got the combo box handle. Now we need the combo box structure.
	eStatus = ControlSetLock((EControlHandle) eControlInterceptedHandle,
							 TRUE,
							 &psControlComboBox,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBoxSpecific = (SControlComboBox *) psControlComboBox->pvControlSpecificData;

	if (0 == psControlComboBoxSpecific->u32FilterMaxCharInput)
	{
		// We pass it along - no processing
		eStatus = ESTATUS_UI_UNHANDLED;
	}
	else
	{
		// Only allow the line input to be displayed if the list box is shown
		if (psControlComboBoxSpecific->bListBoxShown)
		{
			psControlComboBoxSpecific->bLineInputDisplayed = TRUE;
			(void) ControlComboBoxUpdateSelectedAndInput(psControlComboBox,
														 psControlComboBoxSpecific);

			// Override and make the cursor blink
			eStatus = ControlLineInputSetCursorOverride(psControlComboBoxSpecific->eFilterInput,
														TRUE);
			ERR_GOTO();

			// Hand the character to the input field - go direct with the message
			eStatus = ControlUTF8InputProcess(psControlComboBoxSpecific->eFilterInput,
											  peUTF8Character,
											  TRUE);
			ERR_GOTO();
		}
	}

errorExit:
	eStatus = ControlSetLock((EControlHandle) eControlInterceptedHandle,
							 FALSE,
							 NULL,
							 eStatus);
	return(eStatus);
}

// Called for all scancode interceptions
static EStatus ControlComboBoxScancodeInterceptMethod(SControl *psControl,			// Keyscan code input
													  EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call
													  SDL_Scancode eScancode,		// SDL Scancode for key hit
													  BOOL bPressed)
{
	EStatus eStatus;
	SControl *psControlComboBox = NULL;
	SControlComboBox *psControlComboBoxSpecific = NULL;

	// Got the combo box handle. Now we need the combo box structure.
	eStatus = ControlSetLock((EControlHandle) eControlInterceptedHandle,
							 TRUE,
							 &psControlComboBox,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBoxSpecific = (SControlComboBox *) psControlComboBox->pvControlSpecificData;

	// If we're showing the filter input and it's a backspace, then pass it through
	if ((psControlComboBoxSpecific->bLineInputDisplayed) &&
		((SDL_SCANCODE_BACKSPACE == eScancode) ||
		 (SDL_SCANCODE_DELETE == eScancode) ||
		 (SDL_SCANCODE_LEFT == eScancode) ||
		 (SDL_SCANCODE_RIGHT == eScancode)))
	{
		// Send it to the text control
		eStatus = ControlScancodeInputProcess((EControlHandle) psControlComboBoxSpecific->eFilterInput,
											   eScancode,
											   bPressed,
											   TRUE);
	}
	else
	{
		// Pass it along
		eStatus = ESTATUS_UI_UNHANDLED;
	}

errorExit:
	eStatus = ControlSetLock((EControlHandle) eControlInterceptedHandle,
							 FALSE,
							 NULL,
							 eStatus);
	return(eStatus);
}

// Interception methods for combo box and all its children
static SControlMethods sg_sControlComboBoxInterceptionMethods =
{
	ECTRLTYPE_MAX,								// Type of control (not used)
	"Combo Box intercept",						// Control name
	0,											// Not used for interception

	NULL,										// Create a control
	NULL,										// Destroy a control
	NULL,										// Control enable/disable
	NULL,										// New position
	NULL,										// Set visible
	NULL,										// Set angle
	NULL,										// Draw the control
	NULL,										// Runtime configuration
	NULL,										// Hit test
	ControComboBoxFocusInterceptMethod,			// Focus set/loss
	NULL,										// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	ControlComboBoxUTF8InterceptMethod,			// UTF8 Input
	ControlComboBoxScancodeInterceptMethod,		// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

// Creates a combo box
EStatus ControlComboBoxCreate(EWindowHandle eWindowHandle,
							  EControlComboBoxHandle *peControlComboBoxHandle,
							  INT32 s32XPos,
							  INT32 s32YPos)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peControlComboBoxHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_COMBOBOX,
							NULL);
	ERR_GOTO();

	eStatus = ControlSetLock((EControlHandle) *peControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// Now create all the subodrinated controls

	// Selected text display
	eStatus = ControlTextCreate(eWindowHandle,
								&psControlComboBox->eSelectedText,
								s32XPos,
								s32YPos);
	ERR_GOTO();

	// Connect the selected text to the combo box
	eStatus = ControlAddChild(*peControlComboBoxHandle,
							  psControlComboBox->eSelectedText);
	ERR_GOTO();

	// Line input control for filtration
	eStatus = ControlLineInputCreate(eWindowHandle,
									 &psControlComboBox->eFilterInput,
									 s32XPos,
									 s32YPos,
									 0, 0);
	ERR_GOTO();

	// Connect the line input to the combo box
	eStatus = ControlAddChild(*peControlComboBoxHandle,
							  psControlComboBox->eFilterInput);
	ERR_GOTO();

	// Open/close button
	eStatus = ControlButtonCreate(eWindowHandle,
								  &psControlComboBox->eListOpenButton,
								  s32XPos,
								  s32YPos);
	ERR_GOTO();

	eStatus = ControlSetUserData((EControlHandle) psControlComboBox->eListOpenButton,
								 (void *) *peControlComboBoxHandle);
	ERR_GOTO();

	eStatus = ControlButtonSetCallback(psControlComboBox->eListOpenButton,
									   ControlComboBoxButtonCallback);
	ERR_GOTO();

	// Connect the button to the combo box
	eStatus = ControlAddChild(*peControlComboBoxHandle,
							  psControlComboBox->eListOpenButton);
	ERR_GOTO();

	// List box
	eStatus = ControlListBoxCreate(eWindowHandle,
								   &psControlComboBox->eListBox,
								   s32XPos,
								   s32YPos);
	ERR_GOTO();

	eStatus = ControlSetUserData((EControlHandle) psControlComboBox->eListBox,
								 (void *) *peControlComboBoxHandle);
	ERR_GOTO();

	eStatus = ControlListBoxSetCallback(psControlComboBox->eListBox,
										(void *) *peControlComboBoxHandle,
										ComboBoxListBoxCallback);
	ERR_GOTO();

	// Connect the list box to the combo box
	eStatus = ControlAddChild(*peControlComboBoxHandle,
							  psControlComboBox->eListBox);
	ERR_GOTO();

	// And the hit control. We'll set the size later
	eStatus = ControlHitCreate(eWindowHandle,
							   &psControlComboBox->eHitHandle,
							   s32XPos,
							   s32YPos,
							   0, 0);
	ERR_GOTO();

	// Now intercept all controls, subordinate or otherwise
	eStatus = ControlSetMethodInterceptTree(*peControlComboBoxHandle,
											&sg_sControlComboBoxInterceptionMethods);


errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(*peControlComboBoxHandle,
								 FALSE,
								 &psControl,
								 eStatus);
	}

	return(eStatus);
}

// Called when a hit test is successful for the region over the top of the selected item/edit box area
static void ComboBoxHitCallback(EControlHitHandle eHitHandle,
								EControlCallbackReason eReason,
								UINT32 u32ReasonData,
								INT32 s32ControlRelativeXPos,
								INT32 s32ControlRelativeYPos)
{
	EStatus eStatus;
	EControlComboBoxHandle eComboBoxHandle;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	// Get the combo box handle
	eStatus = ControlGetUserData((EControlHandle) eHitHandle,
								 (void **) &eComboBoxHandle);
	ERR_GOTO();

	// Got the combo box handle. Now we need to attempt to lock the thing
	eStatus = ControlSetLock(eComboBoxHandle,
							 TRUE,
							 &psControl,
							 eStatus);
	ERR_GOTO();

	bLocked = TRUE;

	// psControl points to the combo box control handle - get the combo box info
	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	if (FALSE == psControl->bDisabled)
	{
		if (FALSE == psControlComboBox->bListBoxShown)
		{
			// Not dropped - so show thelist
			psControlComboBox->bListBoxShown = TRUE;
		}
		else
		{
/*			// Dropped. Collapse it.
			psControlComboBox->bListBoxShown = FALSE;

			eStatus = ControlComboBoxClearFilter(psControl,
												 psControlComboBox);
			ERR_GOTO(); */
		}

		eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
														psControlComboBox);

		// Shift our focus to the list box
		if (psControlComboBox->bListBoxShown)
		{
			eStatus = WindowControlSetFocus(psControl->eWindowHandle,
											psControlComboBox->eListBox);
		}

		// Time for a redraw
		WindowUpdated();
	}


errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eComboBoxHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
}

EStatus ControlComboBoxSetAssets(EControlComboBoxHandle eControlComboBoxHandle,
								 UINT32 u32XSize,
								 UINT32 u32YSize,
								 SControlComboBoxConfig *psControlComboBoxConfig)
{
	EStatus eStatus = ESTATUS_OK;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	// Set the control's size
	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;
	psControlComboBox->u32RGBATextSelectedColor = psControlComboBoxConfig->u32RGBATextSelectedColor;
	psControlComboBox->peTextNotSelected = psControlComboBoxConfig->peTextNotSelected;
	psControlComboBox->eFont = psControlComboBoxConfig->eFilterBoxFont;
	psControlComboBox->u16FontSize = psControlComboBoxConfig->u16FilterBoxFontSize;

	psControlComboBox->s32XSelectedOffset = psControlComboBoxConfig->s32XSelectedOffset;
	psControlComboBox->s32YSelectedOffset = psControlComboBoxConfig->s32YSelectedOffset;

	// Now set the button's assets
	eStatus = ControlButtonSetAssets(psControlComboBox->eListOpenButton,
									 &psControlComboBoxConfig->sDropButtonConfig);
	ERR_GOTO();

	// Set the buttons' position
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eListOpenButton,
								  psControl->s32XPos + u32XSize - psControlComboBoxConfig->sDropButtonConfig.u32XSizeNotPressed,
								  psControl->s32YPos);
	ERR_GOTO();

	psControlComboBoxConfig->sListBoxConfig.eFont = psControlComboBoxConfig->eFilterBoxFont;
	psControlComboBoxConfig->sListBoxConfig.u16FontSize = psControlComboBoxConfig->u16FilterBoxFontSize;

	// Now the list box
	eStatus = ControlListBoxSetAssets(psControlComboBox->eListBox,
									  &psControlComboBoxConfig->sListBoxConfig);
	ERR_GOTO();

	// And set its position
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eListBox,
								 psControl->s32XPos, psControl->s32YPos + psControlComboBoxConfig->sDropButtonConfig.u32YSizeNotPressed);
	ERR_GOTO();
	
	// And the selected text box
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eSelectedText,
								 psControl->s32XPos + psControlComboBox->s32XSelectedOffset,
								 psControl->s32YPos + psControlComboBox->s32YSelectedOffset);
	ERR_GOTO();

	// Set the hit control to the same as the text region
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eHitHandle,
								 psControl->s32XPos + psControlComboBox->s32XSelectedOffset,
								 psControl->s32YPos + psControlComboBox->s32YSelectedOffset);
	ERR_GOTO();

	// And the size of the hit area
	eStatus = ControlSetSize((EControlHandle) psControlComboBox->eHitHandle,
							  u32XSize - psControlComboBoxConfig->sDropButtonConfig.u32XSizeNotPressed,
							  psControlComboBoxConfig->sDropButtonConfig.u32YSizeNotPressed);
	ERR_GOTO();

	// Set the user data as the combo box parent
	eStatus = ControlSetUserData((EControlHandle) psControlComboBox->eHitHandle,
								 (void *) eControlComboBoxHandle);
	ERR_GOTO();

	// And the callback
	eStatus = ControlHitSetCallback(psControlComboBox->eHitHandle,
									ComboBoxHitCallback);
	ERR_GOTO();

	// Line input/filter as well
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eFilterInput,
								 psControl->s32XPos + psControlComboBox->s32XSelectedOffset,
								 psControl->s32YPos + psControlComboBox->s32YSelectedOffset);
	ERR_GOTO();

	eStatus = ControlLineInputSetAssets(psControlComboBox->eFilterInput,
										psControlComboBox->eFont,
										psControlComboBox->u16FontSize,
										psControlComboBox->u32RGBATextSelectedColor);
	ERR_GOTO();

	psControlComboBox->u32FilterMaxCharInput = psControlComboBoxConfig->u32FilterMaxCharInput;

	eStatus = ControlLineInputSetText(psControlComboBox->eFilterInput,
									  "",
									  psControlComboBoxConfig->u32FilterMaxCharInput);
	ERR_GOTO();

	eStatus = ControlLineInputSetCursor(psControlComboBox->eFilterInput,
										psControlComboBoxConfig->u32BlinkdRateMs,
										0,
										psControlComboBoxConfig->s32CursorYAdj,
										psControlComboBoxConfig->u32CursorXSize,
										psControlComboBoxConfig->u32CursorYSize,
										psControlComboBox->u32RGBATextSelectedColor);
	ERR_GOTO();

	// Set up a callback for filtration entry
	eStatus = ControlLineInputSetCallback(psControlComboBox->eFilterInput,
										  (void *) eControlComboBoxHandle,
										  ControlComboBoxLineInputCallback);
	ERR_GOTO();
	
	// And its position and size (same as the selected text box)
	eStatus = ControlSetPosition((EControlHandle) psControlComboBox->eFilterInput,
								 psControl->s32XPos + psControlComboBox->s32XSelectedOffset,
								 psControl->s32YPos + psControlComboBox->s32YSelectedOffset);
	ERR_GOTO();

	eStatus = ControlSetSize((EControlHandle) psControlComboBox->eFilterInput,
							  u32XSize - psControlComboBoxConfig->sDropButtonConfig.u32XSizeNotPressed,
							  psControlComboBoxConfig->sDropButtonConfig.u32YSizeNotPressed);
	ERR_GOTO();

	eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
													psControlComboBox);

errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eControlComboBoxHandle,
								 FALSE,
								 &psControl,
								 eStatus);
	}

	return(eStatus);
}

// Adds an item to a combo box
EStatus ControComboBoxItemAdd(EControlComboBoxHandle eControlComboBoxHandle,
							  char *peText,
							  void *pvUserData,
							  UINT64 *pu64AssignedIndex,
							  INT8 (*SortCompareFunction)(EControlComboBoxHandle eControlComboBoxHandle,
														  char *peText1,
														  UINT64 u64Index1,
														  void *pvUserData1,
														  char *peText2,
														  UINT64 u64Index2,
														  void *pvUserData2))
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxItemAdd(psControlComboBox->eListBox,
									peText,
									pvUserData,
									pu64AssignedIndex,
									SortCompareFunction);

errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eControlComboBoxHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

// Sets up a callback when an item is selected
EStatus ControlComboBoxSetCallback(EControlComboBoxHandle eControlComboBoxHandle,
								   void *pvCallbackUserData,
								   void (*SelectCallback)(EControlComboBoxHandle eControlComboBoxHandle,
														  void *pvCallbackUserData,
														  EControlCallbackReason eCallbackReason,
														  char *peText,
														  UINT64 u64Index,
														  void *pvItemUserData))
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	psControlComboBox->pvCallbackUserData = pvCallbackUserData;
	psControlComboBox->SelectCallback = SelectCallback;

errorExit:
	if (bLocked)
	{
		// Now unlock the master control
		eStatus = ControlSetLock(eControlComboBoxHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

EStatus ControlComboBoxDeleteItem(EControlListBoxHandle eControlComboBoxHandle,
								  UINT64 u64Index,
								  void *pvUserData)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxDeleteItem(psControlComboBox->eListBox,
									   u64Index,
									   pvUserData);
	ERR_GOTO();

	// Get the currently selected item
	eStatus = ControlListBoxGetSelected(psControlComboBox->eListBox,
										&psControlComboBox->u64SelectedItemIndex,
										NULL,
										NULL);
	ERR_GOTO();

	// Repaint
	WindowUpdated();

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlComboBoxItemSetFilterVisible(EControlComboBoxHandle eControlComboBoxHandle,
											UINT64 u64Index,
											BOOL bFilterVisible)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxItemSetFilterVisible(psControlComboBox->eListBox,
												 u64Index,
												 bFilterVisible);

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlComboBoxFind(EControlComboBoxHandle eControlComboBoxHandle,
							UINT64 u64Index,
							void *pvUserData,
							char **ppeText,
							void **ppvUserData,
							UINT64 *pu64Index,
							BOOL *pbFilterVisible)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxFind(psControlComboBox->eListBox,
								 u64Index,
								 pvUserData,
								 ppeText,
								 ppvUserData,
								 pu64Index,
								 pbFilterVisible);

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlComboBoxSetSelected(EControlComboBoxHandle eControlComboBoxHandle,
								   UINT64 u64Index,
								   void *pvUserData,
								   BOOL bfilterVisible)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxSetSelected(psControlComboBox->eListBox,
										u64Index,
										pvUserData,
										bfilterVisible);
	ERR_GOTO();

	// Get the currently selected item
	SafeMemFree(psControlComboBox->peSelectedItemText);
	eStatus = ControlListBoxGetSelected(psControlComboBox->eListBox,
										&psControlComboBox->u64SelectedItemIndex,
										&psControlComboBox->peSelectedItemText,
										NULL);
	ERR_GOTO();

	eStatus = ControlComboBoxUpdateSelectedAndInput(psControl,
													psControlComboBox);

	ERR_GOTO();

	// Repaint
	WindowUpdated();

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

// This will get the currently selected combo box item and will make a copy of the tag
// if ppeTag is non-NULL.
EStatus ControlComboBoxGetSelected(EControlListBoxHandle eControlComboBoxHandle,
								   UINT64 *pu64Index,
								   char **ppeTag,
								   void **ppvUserData)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	// Get the currently selected item
	eStatus = ControlListBoxGetSelected(psControlComboBox->eListBox,
										pu64Index,
										ppeTag,
										ppvUserData);
	ERR_GOTO();

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlComboBoxItemSetFilters(EControlComboBoxHandle eControlComboBoxHandle,
									  BOOL bFilteredList,
									  void *pvTempUserData,
									  BOOL (*FilterVisibleCheck)(EControlListBoxHandle eControlListBoxHandle,
																 void *pvTempUserData,
																 char *peText,
																 UINT64 u64Index,
																 void *pvUserData))
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlComboBox *psControlComboBox = NULL;

	eStatus = ControlSetLock((EControlHandle) eControlComboBoxHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlComboBox = (SControlComboBox *) psControl->pvControlSpecificData;

	eStatus = ControlListBoxItemSetFilters(psControlComboBox->eListBox,
										   bFilteredList,
										   pvTempUserData,
										   FilterVisibleCheck);

errorExit:
	// Now unlock the master control
	eStatus = ControlSetLock(eControlComboBoxHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlComboBoxInit(void)
{
	return(ControlRegister(sg_sControlComboBoxMethods.eType,
						   &sg_sControlComboBoxMethods));
}
