#ifndef _CONTROL_H_
#define _CONTROL_H_

#include "Shared/HandlePool.h"
#include "Shared/libsdl/libsdlsrc/include/SDL.h"

typedef EHandleGeneric EControlHandle;

// Enums for various control types
typedef enum EControlType
{
	ECTRLTYPE_IMAGE,
	ECTRLTYPE_TEXT,
	ECTRLTYPE_BUTTON,
	ECTRLTYPE_SLIDER,
	ECTRLTYPE_LINEINPUT,
	ECTRLTYPE_COMBOBOX,
	ECTRLTYPE_LISTBOX,
	ECTRLTYPE_HIT,

	// This needs to be the last on the list
	ECTRLTYPE_MAX
} EControlType;

// Reasons for a callback
typedef enum 
{
	ECTRLACTION_UNKNOWN,				// Unknown/do not use
	ECTRLACTION_ENTER,					// User hit enter
	ECTRLACTION_TAB,					// User hit tab
	ECTRLACTION_SHIFTTAB,				// User hit shit tab
	ECTRLACTION_ESCAPE,					// User hit escape
	ECTRLACTION_LOSTFOCUS,				// Input field lost focus
	ECTRLACTION_TEXTCHANGED,			// Input text changed (char added/removed)
	ECTRLACTION_SELECTED,				// Item field changed
	ECTRLACTION_MOUSEBUTTON,			// Mouse button clicked on it
} EControlCallbackReason;

// Forward reference
struct SControl;

// Repeat schedule 
typedef struct SControlRepeatSchedule
{
	UINT32 u32RepeatTime;
	UINT32 u32RepeatRateMS;

	// THESE VARIABLES ARE CALCULATED WHEN THE REPEAT SCHEDULE IS ASSIGNED
	UINT64 u64RepeatTimestamp;
} SControlRepeatSchedule;

// Control methods
typedef struct SControlMethods
{
	EControlType eType;										// What type of control is this?
	char *peControlName;									// Textual name of control
	UINT32 u32ControlSpecificStructureSize;					// Control specific structure size

	EStatus (*Create)(struct SControl *psControl,			// Actual control structure
					  void *pvControlInitConfiguration);	// Configuration structure

	EStatus (*Destroy)(struct SControl *psControl);			// Destroy a control

	EStatus (*Disable)(struct SControl *psControl,			// Set control disabled/enabled
					   BOOL bDisableOldState,
					   BOOL bDisable);

	EStatus (*Position)(struct SControl *psControl,			// When a new position is set
						INT32 s32WindowXPos,
						INT32 s32WindowYPos);

	EStatus (*Visible)(struct SControl *psControl,			// Called when a control's visibility is set/cleared
					   BOOL bVisible);

	EStatus (*Angle)(struct SControl *psControl,			// Called when the angle of a control changes
					 double dAngle);

	EStatus (*Draw)(struct SControl *psControl,				// Draw a control
					UINT8 u8WindowIntensity,				// Intensity from window
					INT32 s32WindowXPos,					// Window X position
					INT32 s32WindowYPos);					// Window Y position

	EStatus (*Configure)(struct SControl *psControl,		// Runtime configuration of a control
						 void *pvControlConfiguration);

	EStatus (*HitTest)(struct SControl *psControl,			// Hit test call (assume hit if NULL)
					   UINT8 u8ButtonMask,					// Which buttons are being pressed?
					   INT32 s32ControlRelativeXPos,
					   INT32 s32ControlRelativeYPos);

	EStatus (*Focus)(struct SControl *psControl,			// Called when this control gains focus
					 EControlHandle eControlHandleOld,
					 EControlHandle eControlHandleNew,
					 UINT8 u8ButtonMask,
					 INT32 s32WindowRelativeX,
					 INT32 s32WindowRelativeY);

	EStatus (*Button)(struct SControl *psControl,			// Called when a button is pressed or released
					  UINT8 u8ButtonMask,
					  INT32 s32ControlRelativeXPos,			// X/Y control relative position where pressed
					  INT32 s32ControlRelativeYPos,
					  BOOL bPressed);

	EStatus (*Drag)(struct SControl *psControl,				// Called when a "drag" occurs for this control (while button held)
					INT32 s32ControlRelativeXPos,
					INT32 s32ControlRelativeYPos);

	EStatus (*Wheel)(struct SControl *psControl,			// Called when a mouse wheel is changed
					 INT32 s32WheelChange);

	EStatus (*UTF8Input)(struct SControl *psControl,		// UTF8 input from keyboard
						 EControlHandle eControlInterceptorHandle,	// The handle that intercepted this call
						 char *peUTF8Character);

	EStatus (*Keyscan)(struct SControl *psControl,			// Keyscan code input
					   EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call
					   SDL_Scancode eScancode,				// SDL Scancode for key hit
					   BOOL bPressed);						// TRUE=Pressed, FALSE=Released

	EStatus (*Periodic)(struct SControl *psControl,			// Periodic callback for focused controls
						UINT32 u32TimerIntervalMs);

	EStatus (*Mouseover)(struct SControl *psControl,		// Called when a mouse is moved over a control when not buttons pressed
						 INT32 s32ControlRelativeXPos,
						 INT32 s32ControlRelativeYPos);

	EStatus (*Size)(struct SControl *psControl,				// Called when the control's size needs to be set
					UINT32 u32XSize,
					UINT32 u32YSize);
} SControlMethods;

// Control list
typedef struct SControlList
{
	EControlHandle eControlHandle;
	struct SControlList *psNextLink;
} SControlList;

// Control structure
typedef struct SControl
{
	EWindowHandle eWindowHandle;		// What window does this control belong to?
	EControlHandle eControlHandle;		// What is this control's handle?
	SControlMethods *psMethods;			// This control's methods
	char *peControlUserName;			// Name for this control (user assigned)

	// X/Y position (window relative)
	INT32 s32XPos;
	INT32 s32YPos;

	// Horizontal and vertical flip
	BOOL bHFlip;
	BOOL bVFlip;

	// Angle of object in degrees
	double dAngle;

	// Origin of image
	BOOL bOriginCenter;					// TRUE=Origin is center, FALSE=Origin is upper left

	// Bounding box for this control
	UINT32 u32XSize;
	UINT32 u32YSize;

	// Misc attributes common to all controls
	BOOL bVisible;			// TRUE If visible
	BOOL bDisabled;			// TRUE If disabled
	BOOL bFocusable;		// TRUE If control can have focus
	UINT8 u8Intensity;		// 0xff=Fully, 0x00=Invisible
	BOOL bAsserted;			// TRUE If this control is focused and the mouse was clicked on it
	BOOL bFocused;			// TRUE If focus set, FALSE if not in focus
	BOOL bHitTestAlways;	// TRUE If it checks for a hit test regardless of the other settings

	// Control specific data
	void *pvControlSpecificData;

	// User specific data
	void *pvUserData;

	// Children
	EControlHandle eControlTop;			// Topmost level control
	EControlHandle eControlParent;		// Direct parental control
	SControlList *psControlChildren;	// Linked list of children for this control

	// Interception handles and methods
	EControlHandle eControlInterceptorHandle;		// The handle that gets the interception opportunity
	SControlMethods *psControlInterceptedMethods;	// Methods to call for an interception

	// This control's critical section
	SOSCriticalSection sCriticalSection;

	// Repeat schedule
	UINT64 u64HeldTimestampCounter;					// # Of milliseconds have passed since 
	UINT32 u32RepeatTimeAccumulator;				// Repeat accumulator
	UINT32 u32RepeatRateMS;							// Repeat rate (in milliseconds)
	INT32 s32RepeatNormalizedRelativeX;				// Relative X position hit/repeat
	INT32 s32RepeatNormalizedRelativeY;				// Relative Y position hit/repeat
	UINT8 u8ButtonMap;								// Which button(s) are held?
	SControlRepeatSchedule *psRepeatScheduleHead;	// Repeat schedule (head of)
	SControlRepeatSchedule *psRepeatSchedule;		// Repeat schedule (currently active)

	// Next link for all control structures
	struct SControl *psAllNextLink;
} SControl;

#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/ControlImage.h"
#include "Shared/Graphics/ControlText.h"
#include "Shared/Graphics/ControlButton.h"
#include "Shared/Graphics/ControlSlider.h"
#include "Shared/Graphics/ControlLineInput.h"
#include "Shared/Graphics/ControlListBox.h"
#include "Shared/Graphics/ControlComboBox.h"
#include "Shared/Graphics/ControlHit.h"

// Internal APIs
extern EStatus ControlInit(void);
extern EStatus ControlRegister(EControlType eControlType,
							   SControlMethods *psMethods);
extern EStatus ControlCreate(EWindowHandle eWindowHandle,
							 EControlHandle *peControlHandle,
							 INT32 s32XPos,
							 INT32 s32YPos,
							 UINT32 u32XSize,
							 UINT32 u32YSize,
							 EControlType eType,
							 void *pvControlInitConfiguration);
extern EStatus ControlCreateCallback(SControl *psControl,
									 void *pvControlInitConfiguration);
extern EStatus ControlConfigureCallback(EControlHandle eControlHandle,
										void *pvConfigurationStructure);
extern EStatus ControlDestroyCallback(EControlHandle eControlHandle);
extern EStatus ControlDraw(EControlHandle eControlHandle,
						   UINT8 u8Intensity,
						   INT32 s32WindowXPos,
						   INT32 s32WindowYPos);
extern EStatus ControlConfigure(EControlHandle eControlHandle,
								void *pvConfigurationStructure);
extern EStatus ControlHitTest(EWindowHandle eWindowHandle,
							  EControlHandle eControlHandle,
							  UINT8 u8ButtonMask,
							  INT32 *ps32WindowRelativeX,
							  INT32 *ps32WindowRelativeY,
							  BOOL bTestOnly);
extern EStatus ControlButtonProcess(EControlHandle eControlHandle,
									UINT8 u8ButtonMask,
									INT32 s32NormalizedControlRelativeX,
									INT32 s32NormalizedControlRelativeY,
									BOOL bPressed);
extern EStatus ControlProcessDragMouseover(EControlHandle eControlHandle,
										   UINT8 u8ButtonMask,
										   INT32 s32NormalizedControlRelativeX,
										   INT32 s32NormalizedControlRelativeY);
extern EStatus ControlSetLock(EControlHandle eControlHandle,
							  BOOL bLock,
							  SControl **ppsControl,
							  EStatus eStatusIncoming);
extern EStatus ControlUTF8InputProcess(EControlHandle eControlHandle,
									   char *peUTF8,
									   BOOL bDirect);
extern EStatus ControlScancodeInputProcess(EControlHandle eControlHandle,
										   SDL_Scancode eScancode,
										   BOOL bPressed,
										   BOOL bDirect);
extern EStatus ControlProcessWheel(EControlHandle eControlHandle,
								   INT32 s32Wheel);
extern EStatus ControlProcessFocus(EControlHandle eControlOld,
								   EControlHandle eControlNew,
								   UINT8 u8ButtonMask,
								   INT32 s32WindowRelativeXOriginal,
								   INT32 s32WindowRelativeYOriginal);
extern EStatus ControlPeriodicTimer(EControlHandle eControlHandle,
									UINT32 u32IntervalTimerMs);
extern EStatus ControlSetMethodInterceptTree(EControlHandle eControlInterceptorHandle,
											 SControlMethods *psInterceptedMethodTable);
extern BOOL ControlIsChild(SControl *psControl,
						   EControlHandle eChildHandle);

// Public APIs meant to be used by UI code
extern EStatus ControlDestroy(EControlHandle *peControlHandle);
extern EStatus ControlSetHitTestAlways(EControlHandle eControlHandle,
									   BOOL bHitTestAlways);
extern EStatus ControlSetVisible(EControlHandle eControlHandle,
								 BOOL bVisible);
extern EStatus ControlSetDisable(EControlHandle eControlHandle,
								 BOOL bDisabled);
extern EStatus ControlSetAngle(EControlHandle eControlHandle,
							   double dAngle);
extern EStatus ControlSetOriginCenter(EControlHandle eControlHandle,
									  BOOL bOriginCenter);
extern EStatus ControlSetFlip(EControlHandle eControlHandle,
							  BOOL bHflip,
							  BOOL bVFlip);
extern UINT32 ControlGetGfxAttributes(SControl *psControl);
extern EStatus ControlSetUserData(EControlHandle eControlHandle,
								  void *pvUserData);
extern EStatus ControlGetUserData(EControlHandle eControlHandle,
								  void **ppvUserData);
extern EStatus ControlConfigure(EControlHandle eControlHandle,
								void *pvControlConfiguration);
extern BOOL ControlHandleIsValid(EControlHandle eControlHandle);
extern EStatus ControlHitTestImage(SGraphicsImage *psGraphicsImage,
								   INT32 s32XPos,
								   INT32 s32YPos);
extern EStatus ControlSetPosition(EControlHandle eControlHandle,
								  INT32 s32WindowXPos,
								  INT32 s32WindowYPos);
extern EStatus ControlSetFocusable(EControlHandle eControlHandle,
								   BOOL bFocusable);
extern EStatus ControlIsAsserted(EControlHandle eControlHandle,
								 BOOL *pbIsAsserted);
extern EStatus ControlGetSizePos(EControlHandle eControlHandle,
								 INT32 *ps32WindowRelativeXPos,
								 INT32 *ps32WindowRelativeYPos,
								 UINT32 *pu32XSize,
								 UINT32 *pu32YSize);
extern EStatus ControlSetSize(EControlHandle eControlHandle,
							  UINT32 u32XSize,
							  UINT32 u32YSize);
extern void ControlRotateClockwise(double dAngle,
								   INT32 *ps32XPos,
								   INT32 *ps32YPos);
extern void ControlRotateCounterClockwise(double dAngle,
										  INT32 *pu32XPos,
										  INT32 *pu32YPos);
extern EControlCallbackReason ControlKeyscanToCallbackReason(SDL_Scancode eScancode);
extern EStatus ControlRenderStretch(UINT32 *pu32RGBALeftImage,
									UINT32 u32LeftXSize,
									UINT32 u32LeftYSize,
									UINT32 *pu32RGBAStretchImage,
									UINT32 u32StretchXSize,
									UINT32 u32StretchYSize,
									UINT32 *pu32RGBARightImage,
									UINT32 u32RightXSize,
									UINT32 u32RightYSize,
									UINT32 u32XSize,
									UINT32 *pu32YSize,
									UINT32 **ppu32RGBARenderedImage);
extern char *ControlGetName(EControlHandle eControlHandle);
extern EStatus ControlAddChild(EControlHandle eControlHandleParent,
							   EControlHandle eControlHandleChild);
extern EStatus ControlIsInTree(EControlHandle eControlTop,
							   EControlHandle eControlToFind,
							   EControlHandle eControlLockSkip,
							   BOOL *pbIsControlInTree);
extern EStatus ControlSetUserName(EControlHandle eControlHandle,
								  char *peControlUserName);
extern EStatus ControlGetUserName(EControlHandle eControlHandle,
								  char **ppeControlUserName);
extern EStatus ControlSetButtonRepeatSchedule(EControlHandle eControlHandle,
											  UINT8 u8ButtonMap,
											  SControlRepeatSchedule *psRepeatSchedule);

#endif

