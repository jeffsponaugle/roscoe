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
	uint32_t u32RepeatTime;
	uint32_t u32RepeatRateMS;

	// THESE VARIABLES ARE CALCULATED WHEN THE REPEAT SCHEDULE IS ASSIGNED
	uint64_t u64RepeatTimestamp;
} SControlRepeatSchedule;

// Control methods
typedef struct SControlMethods
{
	EControlType eType;										// What type of control is this?
	char *peControlName;									// Textual name of control
	uint32_t u32ControlSpecificStructureSize;					// Control specific structure size

	EStatus (*Create)(struct SControl *psControl,			// Actual control structure
					  void *pvControlInitConfiguration);	// Configuration structure

	EStatus (*Destroy)(struct SControl *psControl);			// Destroy a control

	EStatus (*Disable)(struct SControl *psControl,			// Set control disabled/enabled
					   bool bDisableOldState,
					   bool bDisable);

	EStatus (*Position)(struct SControl *psControl,			// When a new position is set
						int32_t s32WindowXPos,
						int32_t s32WindowYPos);

	EStatus (*Visible)(struct SControl *psControl,			// Called when a control's visibility is set/cleared
					   bool bVisible);

	EStatus (*Angle)(struct SControl *psControl,			// Called when the angle of a control changes
					 double dAngle);

	EStatus (*Draw)(struct SControl *psControl,				// Draw a control
					uint8_t u8WindowIntensity,				// Intensity from window
					int32_t s32WindowXPos,					// Window X position
					int32_t s32WindowYPos);					// Window Y position

	EStatus (*Configure)(struct SControl *psControl,		// Runtime configuration of a control
						 void *pvControlConfiguration);

	EStatus (*HitTest)(struct SControl *psControl,			// Hit test call (assume hit if NULL)
					   uint8_t u8ButtonMask,					// Which buttons are being pressed?
					   int32_t s32ControlRelativeXPos,
					   int32_t s32ControlRelativeYPos);

	EStatus (*Focus)(struct SControl *psControl,			// Called when this control gains focus
					 EControlHandle eControlHandleOld,
					 EControlHandle eControlHandleNew,
					 uint8_t u8ButtonMask,
					 int32_t s32WindowRelativeX,
					 int32_t s32WindowRelativeY);

	EStatus (*Button)(struct SControl *psControl,			// Called when a button is pressed or released
					  uint8_t u8ButtonMask,
					  int32_t s32ControlRelativeXPos,			// X/Y control relative position where pressed
					  int32_t s32ControlRelativeYPos,
					  bool bPressed);

	EStatus (*Drag)(struct SControl *psControl,				// Called when a "drag" occurs for this control (while button held)
					int32_t s32ControlRelativeXPos,
					int32_t s32ControlRelativeYPos);

	EStatus (*Wheel)(struct SControl *psControl,			// Called when a mouse wheel is changed
					 int32_t s32WheelChange);

	EStatus (*UTF8Input)(struct SControl *psControl,		// UTF8 input from keyboard
						 EControlHandle eControlInterceptorHandle,	// The handle that intercepted this call
						 char *peUTF8Character);

	EStatus (*Keyscan)(struct SControl *psControl,			// Keyscan code input
					   EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call
					   SDL_Scancode eScancode,				// SDL Scancode for key hit
					   bool bPressed);						// true=Pressed, false=Released

	EStatus (*Periodic)(struct SControl *psControl,			// Periodic callback for focused controls
						uint32_t u32TimerIntervalMs);

	EStatus (*Mouseover)(struct SControl *psControl,		// Called when a mouse is moved over a control when not buttons pressed
						 int32_t s32ControlRelativeXPos,
						 int32_t s32ControlRelativeYPos);

	EStatus (*Size)(struct SControl *psControl,				// Called when the control's size needs to be set
					uint32_t u32XSize,
					uint32_t u32YSize);
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
	int32_t s32XPos;
	int32_t s32YPos;

	// Horizontal and vertical flip
	bool bHFlip;
	bool bVFlip;

	// Angle of object in degrees
	double dAngle;

	// Origin of image
	bool bOriginCenter;					// true=Origin is center, false=Origin is upper left

	// Bounding box for this control
	uint32_t u32XSize;
	uint32_t u32YSize;

	// Misc attributes common to all controls
	bool bVisible;			// true If visible
	bool bDisabled;			// true If disabled
	bool bFocusable;		// true If control can have focus
	uint8_t u8Intensity;		// 0xff=Fully, 0x00=Invisible
	bool bAsserted;			// true If this control is focused and the mouse was clicked on it
	bool bFocused;			// true If focus set, false if not in focus
	bool bHitTestAlways;	// true If it checks for a hit test regardless of the other settings

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
	uint64_t u64HeldTimestampCounter;					// # Of milliseconds have passed since 
	uint32_t u32RepeatTimeAccumulator;				// Repeat accumulator
	uint32_t u32RepeatRateMS;							// Repeat rate (in milliseconds)
	int32_t s32RepeatNormalizedRelativeX;				// Relative X position hit/repeat
	int32_t s32RepeatNormalizedRelativeY;				// Relative Y position hit/repeat
	uint8_t u8ButtonMap;								// Which button(s) are held?
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
							 int32_t s32XPos,
							 int32_t s32YPos,
							 uint32_t u32XSize,
							 uint32_t u32YSize,
							 EControlType eType,
							 void *pvControlInitConfiguration);
extern EStatus ControlCreateCallback(SControl *psControl,
									 void *pvControlInitConfiguration);
extern EStatus ControlConfigureCallback(EControlHandle eControlHandle,
										void *pvConfigurationStructure);
extern EStatus ControlDestroyCallback(EControlHandle eControlHandle);
extern EStatus ControlDraw(EControlHandle eControlHandle,
						   uint8_t u8Intensity,
						   int32_t s32WindowXPos,
						   int32_t s32WindowYPos);
extern EStatus ControlConfigure(EControlHandle eControlHandle,
								void *pvConfigurationStructure);
extern EStatus ControlHitTest(EWindowHandle eWindowHandle,
							  EControlHandle eControlHandle,
							  uint8_t u8ButtonMask,
							  int32_t *ps32WindowRelativeX,
							  int32_t *ps32WindowRelativeY,
							  bool bTestOnly);
extern EStatus ControlButtonProcess(EControlHandle eControlHandle,
									uint8_t u8ButtonMask,
									int32_t s32NormalizedControlRelativeX,
									int32_t s32NormalizedControlRelativeY,
									bool bPressed);
extern EStatus ControlProcessDragMouseover(EControlHandle eControlHandle,
										   uint8_t u8ButtonMask,
										   int32_t s32NormalizedControlRelativeX,
										   int32_t s32NormalizedControlRelativeY);
extern EStatus ControlSetLock(EControlHandle eControlHandle,
							  bool bLock,
							  SControl **ppsControl,
							  EStatus eStatusIncoming);
extern EStatus ControlUTF8InputProcess(EControlHandle eControlHandle,
									   char *peUTF8,
									   bool bDirect);
extern EStatus ControlScancodeInputProcess(EControlHandle eControlHandle,
										   SDL_Scancode eScancode,
										   bool bPressed,
										   bool bDirect);
extern EStatus ControlProcessWheel(EControlHandle eControlHandle,
								   int32_t s32Wheel);
extern EStatus ControlProcessFocus(EControlHandle eControlOld,
								   EControlHandle eControlNew,
								   uint8_t u8ButtonMask,
								   int32_t s32WindowRelativeXOriginal,
								   int32_t s32WindowRelativeYOriginal);
extern EStatus ControlPeriodicTimer(EControlHandle eControlHandle,
									uint32_t u32IntervalTimerMs);
extern EStatus ControlSetMethodInterceptTree(EControlHandle eControlInterceptorHandle,
											 SControlMethods *psInterceptedMethodTable);
extern bool ControlIsChild(SControl *psControl,
						   EControlHandle eChildHandle);

// Public APIs meant to be used by UI code
extern EStatus ControlDestroy(EControlHandle *peControlHandle);
extern EStatus ControlSetHitTestAlways(EControlHandle eControlHandle,
									   bool bHitTestAlways);
extern EStatus ControlSetVisible(EControlHandle eControlHandle,
								 bool bVisible);
extern EStatus ControlSetDisable(EControlHandle eControlHandle,
								 bool bDisabled);
extern EStatus ControlSetAngle(EControlHandle eControlHandle,
							   double dAngle);
extern EStatus ControlSetOriginCenter(EControlHandle eControlHandle,
									  bool bOriginCenter);
extern EStatus ControlSetFlip(EControlHandle eControlHandle,
							  bool bHflip,
							  bool bVFlip);
extern uint32_t ControlGetGfxAttributes(SControl *psControl);
extern EStatus ControlSetUserData(EControlHandle eControlHandle,
								  void *pvUserData);
extern EStatus ControlGetUserData(EControlHandle eControlHandle,
								  void **ppvUserData);
extern EStatus ControlConfigure(EControlHandle eControlHandle,
								void *pvControlConfiguration);
extern bool ControlHandleIsValid(EControlHandle eControlHandle);
extern EStatus ControlHitTestImage(SGraphicsImage *psGraphicsImage,
								   int32_t s32XPos,
								   int32_t s32YPos);
extern EStatus ControlSetPosition(EControlHandle eControlHandle,
								  int32_t s32WindowXPos,
								  int32_t s32WindowYPos);
extern EStatus ControlSetFocusable(EControlHandle eControlHandle,
								   bool bFocusable);
extern EStatus ControlIsAsserted(EControlHandle eControlHandle,
								 bool *pbIsAsserted);
extern EStatus ControlGetSizePos(EControlHandle eControlHandle,
								 int32_t *ps32WindowRelativeXPos,
								 int32_t *ps32WindowRelativeYPos,
								 uint32_t *pu32XSize,
								 uint32_t *pu32YSize);
extern EStatus ControlSetSize(EControlHandle eControlHandle,
							  uint32_t u32XSize,
							  uint32_t u32YSize);
extern void ControlRotateClockwise(double dAngle,
								   int32_t *ps32XPos,
								   int32_t *ps32YPos);
extern void ControlRotateCounterClockwise(double dAngle,
										  int32_t *pu32XPos,
										  int32_t *pu32YPos);
extern EControlCallbackReason ControlKeyscanToCallbackReason(SDL_Scancode eScancode);
extern EStatus ControlRenderStretch(uint32_t *pu32RGBALeftImage,
									uint32_t u32LeftXSize,
									uint32_t u32LeftYSize,
									uint32_t *pu32RGBAStretchImage,
									uint32_t u32StretchXSize,
									uint32_t u32StretchYSize,
									uint32_t *pu32RGBARightImage,
									uint32_t u32RightXSize,
									uint32_t u32RightYSize,
									uint32_t u32XSize,
									uint32_t *pu32YSize,
									uint32_t **ppu32RGBARenderedImage);
extern char *ControlGetName(EControlHandle eControlHandle);
extern EStatus ControlAddChild(EControlHandle eControlHandleParent,
							   EControlHandle eControlHandleChild);
extern EStatus ControlIsInTree(EControlHandle eControlTop,
							   EControlHandle eControlToFind,
							   EControlHandle eControlLockSkip,
							   bool *pbIsControlInTree);
extern EStatus ControlSetUserName(EControlHandle eControlHandle,
								  char *peControlUserName);
extern EStatus ControlGetUserName(EControlHandle eControlHandle,
								  char **ppeControlUserName);
extern EStatus ControlSetButtonRepeatSchedule(EControlHandle eControlHandle,
											  uint8_t u8ButtonMap,
											  SControlRepeatSchedule *psRepeatSchedule);

#endif

