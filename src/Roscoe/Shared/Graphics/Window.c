#include <stdio.h>
#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/HandlePool.h"
#include "Shared/Timer.h"
#include "Shared/Graphics/Font.h"
#include "Shared/Graphics/Control.h"

// Set up message structure
#define	CLEAR_MSG_STRUCT(x)	ZERO_STRUCT(x); HandleSetInvalid(&x.eHandle1); HandleSetInvalid(&x.eHandle1); 

// Startup/shutdown semaphore
static SOSSemaphore sg_sWindowStartupShutdownSemaphore;
static EStatus sg_eWindowStartupEStatus;

// Window currently in focus (if invalid, none)
static EWindowHandle sg_eWindowInFocus;

// Maximum # of windows
#define	MAX_WINDOWS		1024

// Handle pool for our windows
static SHandlePool *sg_psWindowHandlePool;

// Window list
typedef struct SWindowList
{
	EWindowHandle eWindowHandle;
	struct SWindowList *psNextLink;
} SWindowList;

// Window's control children
typedef struct SWindowControlChild
{
	EControlHandle eControlHandle;

	struct SWindowControlChild *psNextLink;
	struct SWindowControlChild *psPriorLink;
} SWindowControlChild;

// Window structure
typedef struct SWindow
{
	// This window's handle
	EWindowHandle eWindowHandle;

	// This window's control that has focus
	EControlHandle eControlHandleFocus;
	BOOL bControlButtonHeld;

	// Set TRUE if the window is modal
	BOOL bModal;

	// Window intensity (00=
	UINT8 u8Intensity;

	// Is this window visible?
	BOOL bVisible;

	// Is this window disabled?
	BOOL bDisabled;

	// This window's critical section
	SOSCriticalSection sCriticalSection;

	// X/Y position of window
	INT32 s32XPos;
	INT32 s32YPos;

	// X/Y size of window
	UINT32 u32XSize;
	UINT32 u32YSize;

	// Window's graphical image
	SGraphicsImage *psWindowImage;

	// Window's children
	SWindowList *psWindowChildList;

	// Window's controls
	SWindowControlChild *psWindowControlChildList;

	// Window's list of controls that need a periodic tick
	SControlList *psWindowControlPeriodicList;

	// Next and prior links
	struct SWindow *psNextLink;
	struct SWindow *psPriorLink;
} SWindow;

// # Of frames we want to burst to the display (for context restoration and to
// cause any OpenGL monitoring programs to display their logos)
static UINT32 sg_u32FrameBurst;

// Linked list of window. sg_psWindowFar is the farthest/lowest priority window
// and sg_psWindowNear is the closest/in front of everything else
static SOSCriticalSection sg_sWindowListLock;
static SWindow *sg_psWindowFar = NULL;
static SWindow *sg_psWindowNear = NULL;

// # Of window messages possible in the Window queue
#define		WINDOW_QUEUE_COUNT	4096

// Windowing message queue
static SOSQueue sg_sWindowQueue;

// Window messages
typedef enum
{
	EWMSG_NOT_DEFINED,			// Undefined window message

	EWMSG_FRAME,				// Go render a frame
	EWMSG_SHUTDOWN,				// Shut down entire windowing subsystem
	EWMSG_WINDOW_CREATE_GRAPHIC, // Create a graphical image with OpenGL context
	EWMSG_WINDOW_DESTROY,		// Destroy a window and its controls and children
	EWMSG_EVENTS,				// Message pump for SDL
	EWMSG_QUIT,					// UI initiazed quit message
	EWMSG_MOUSE_MOVEMENT,		// Mouse movement
	EWMSG_MOUSE_BUTTON,			// Mouse button
	EWMSG_MOUSE_WHEEL,			// Mouse wheel event
	EWMSG_KEYSCAN,				// Key scancode input
	EWMSG_UTF8_INPUT,			// UTF-8 Input
	EWMSG_CONTROL_CREATE,		// Create a control
	EWMSG_CONTROL_DESTROY,		// Destroy a control
	EWMSG_CONTROL_CONFIGURE,	// Configure a control
	EWMSG_WINDOW_FOCUS,			// Set (or loss) of focus
	EWMSG_CONTROL_FOCUS,		// Change control focus
	EWMSG_CONTROL_BUTTON,		// Control's button press
} EWindowMsg;

// Window message
typedef struct SWindowMsg
{
	EWindowMsg eMsg;				// Window message
	SOSSemaphore sSignal;			// Signal semaphore (For messages that need it)
	EHandleGeneric eHandle1;		// Generic handle #1
	EHandleGeneric eHandle2;		// Generic handle #1
	SGraphicsImage **ppsGraphicsImage; // Graphical image if needed
	EStatus *peStatusAsync;			// Asynchronos message results
	char *peString;					// Generic passing of a string
	BOOL bGenericBoolean;			// Generic boolean
	UINT64 u64Data1;				// Generic data variable
	UINT64 u64Data2;				// Second data variable
	void *pvData1;					// Generic data pointer #1
	void *pvData2;					// Generic data pointer #2
} SWindowMsg;

// Our frame timer
static STimer *sg_psFrameTimer;
static float sg_fFrameAccumulator;
static float sg_fMsecPerFrame;

// Set to TRUE if a frame has been submitted
volatile static BOOL sg_bFrameSubmitted = FALSE;

// Frame lock count - nonzero=frame is locked
volatile static UINT8 sg_u8FrameLockCount = 0;
static SOSCriticalSection sg_sFrameLock;

// Set TRUE if we want to do a frame blit
volatile static BOOL sg_bFramePending = FALSE;

// Lock or unlock the window list. Inherit the incoming status if it's
// not ESTATUS_OK.
#define	WindowListSetLock(bLock, eStatusIncoming)	WindowListSetLockInternal(__FUNCTION__, (UINT32) __LINE__, bLock, eStatusIncoming)
static EStatus WindowListSetLockInternal(char *peFunction,
										 UINT32 u32LineNumber,
										 BOOL bLock,
										 EStatus eStatusIncoming)
{
	EStatus eStatus;

	if (bLock)
	{
//		Syslog("%s(%u): Locking\n", peFunction, u32LineNumber);
		eStatus = OSCriticalSectionEnter(sg_sWindowListLock);
	}
	else
	{
//		Syslog("%s(%u): Unocking\n", peFunction, u32LineNumber);
		eStatus = OSCriticalSectionLeave(sg_sWindowListLock);
	}

	if (eStatusIncoming != ESTATUS_OK)
	{
		eStatus = eStatusIncoming;
	}

	return(eStatus);
}

// Called whenever a frame needs
void WindowUpdated(void)
{
	sg_bFramePending = TRUE;
}

// Wait for a (virtual) frame
static SOSEventFlag *sg_psFrameWaitEventFlag;

void WindowWaitFrame(void)
{
	EStatus eStatus;

	eStatus = OSEventFlagGet(sg_psFrameWaitEventFlag,
							 0x01,
							 OS_WAIT_INDEFINITE,
							 NULL);
	BASSERT(ESTATUS_OK == eStatus);
}

// Lock or unlock the frame
void WindowSetFrameLock(BOOL bLock)
{
	EStatus eStatus;

	eStatus = OSCriticalSectionEnter(sg_sFrameLock);
	BASSERT(ESTATUS_OK == eStatus);

	if (bLock)
	{
		// If this asserts then someone isn't releasing frame locks
		BASSERT(sg_u8FrameLockCount != 0xff);
	}
	else
	{
		// IF this asserts then the unlock/lock for the frame lock is imbalanced
		BASSERT(sg_u8FrameLockCount);
	}

	sg_u8FrameLockCount++;

	eStatus = OSCriticalSectionLeave(sg_sFrameLock);
	BASSERT(ESTATUS_OK == eStatus);
}

// Returns TRUE if the incoming window handle is valid
BOOL WindowHandleIsValid(EWindowHandle eWindowHandle)
{
	EStatus eStatus;

	eStatus = HandleIsValid(sg_psWindowHandlePool, eWindowHandle);
	if (ESTATUS_OK == eStatus)
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}

// Returns native desktop size (in pixels)
void WindowGetVirtualSize(UINT32 *pu32XSize,
						  UINT32 *pu32YSize)
{
	GraphicsGetVirtualSurfaceSize(pu32XSize,
								  pu32YSize);
}

// Frame timer callback
static void WindowFrameTimerCallback(STimer *psTimer,
									 void *pvCallbackValue)
{
	EStatus eStatus;
	float fMsecPerCallback = (float) ((UINT32) pvCallbackValue);
	SWindowMsg sMsg;

	// Add frame time to accumulator
	sg_fFrameAccumulator += fMsecPerCallback;

	// If we've overflowed our frame callback time, then subtract it
	// from the accumulator and attempt to process a frame
	while (sg_fFrameAccumulator >= sg_fMsecPerFrame)
	{
		// Always flag the frame wait flags
		(void) OSEventFlagSet(sg_psFrameWaitEventFlag,
							  0x01);

		if (sg_u8FrameLockCount)
		{
			// Don't do anything
		}
		else
		{
			CLEAR_MSG_STRUCT(sMsg);

			// Frame's not locked. If we have a submitted frame, don't submit another one.
			if (sg_bFrameSubmitted)
			{
				// Don't submit another frame
			}
			else
			if (sg_bFramePending)
			{
				// Indicate we've submitted a frame first becuase we may not get the chance
				//	before the service thread sets it FALSE
				sg_bFrameSubmitted = TRUE;

				// Need to submit a frame
				sMsg.eMsg = EWMSG_FRAME;
				eStatus = OSQueuePut(sg_sWindowQueue,
									 (void *) &sMsg,
									 OS_WAIT_INDEFINITE);
				if (ESTATUS_OK != eStatus)
				{
					// Some sort of failure
					SyslogFunc("Failed to submit frame - %s\n", GetErrorText(eStatus));

					// Didn't submit correctly, try again.
					sg_bFrameSubmitted = FALSE;
				}
			}
			else
			if (sg_u32FrameBurst)
			{
				sg_bFramePending = TRUE;
				sg_u32FrameBurst--;
			}
			else
			{
				// Nothing to do
			}
		}

		sg_fFrameAccumulator -= sg_fMsecPerFrame;
	}
}

// Gets this window's handle structure
static EStatus WindowGetStructByHandle(EWindowHandle eWindowHandle,
									   SWindow **ppsWindow)
{
	return(HandleSetLock(sg_psWindowHandlePool,
						    NULL,
							(EHandleGeneric) eWindowHandle,
							EHANDLE_NONE,
							(void **) ppsWindow,
							TRUE,
							ESTATUS_OK));
}

// Sets the window structure's critical section lock
static EStatus WindowSetLock(EWindowHandle eWindowHandle,
							 SWindow **ppsWindow,
							 BOOL bLock,
							 EStatus eStatusIncoming)
{
	BOOL bValidateAllocated = FALSE;
	EHandleLock eLock = EHANDLE_LOCK;

	if (FALSE == bLock)
	{
		eLock = EHANDLE_UNLOCK;
	}

	return(HandleSetLock(sg_psWindowHandlePool,
						 NULL,
						 (EHandleGeneric) eWindowHandle,
						 eLock,
						 (void **) ppsWindow,
						 TRUE,
						 eStatusIncoming));
}

// Adds psNew directly in front of psRear. If psRear is NULL, it is
// added to the farthest/bottom window in the stack. It is assumed
// the window list is locked when this function is called.
static void	WindowListAddFront(SWindow *psRear,
							   SWindow *psNew)
{
	// Our incoming new window should not have a next or prior link. This
	// means we have a stale pointer somewhere.
	BASSERT(NULL == psNew->psPriorLink);
	BASSERT(NULL == psNew->psNextLink);

	if (NULL == psRear)
	{
		// Add to the very back of the list.
		psNew->psNextLink = sg_psWindowFar;

		// Make sure the window on the bottom doesn't have
		if (sg_psWindowFar)
		{
			// We have a farthest window. Make sure it doesn't have
			// a prior link
			BASSERT(NULL == sg_psWindowFar->psPriorLink);

			sg_psWindowFar->psPriorLink = psNew;
		}
		else
		{
			// No window in front of it
		}

		sg_psWindowFar = psNew;
	}
	else
	{
		// In front of some other window
		psNew->psNextLink = psRear->psNextLink;
		psNew->psPriorLink = psRear;
		if (psNew->psNextLink)
		{
			psNew->psNextLink->psPriorLink = psNew;
		}

		psRear->psNextLink = psNew;

		if (NULL == psNew->psNextLink)
		{
			// Front of the list
			sg_psWindowNear = NULL;
		}
	}

	// If there is no nearest window, then this new window is the nearest.
	if (NULL == sg_psWindowNear)
	{
		sg_psWindowNear = psNew;
	}
}

// This removes the incoming window pointer from the window lists. Both
// far and near pointers will be adjusted
static void WindowListRemove(SWindow *psRemove)
{
	// Am I at the back of the list?
	if (NULL == psRemove->psPriorLink)
	{
		// Verify it's actually at the back
		BASSERT(sg_psWindowFar == psRemove);
		sg_psWindowFar = psRemove->psNextLink;
	}
	else
	{
		// Not at the back of the list. Something is before us.
		BASSERT(psRemove->psPriorLink->psNextLink == psRemove);
		psRemove->psPriorLink->psNextLink = psRemove->psNextLink;
	}

	// How about the front?
	if (NULL == psRemove->psNextLink)
	{
		// Verify it's actually at the front
		BASSERT(sg_psWindowNear == psRemove);
		sg_psWindowNear = psRemove->psPriorLink;
	}
	else
	{
		// Not at the front of the list. Something after us.
		BASSERT(psRemove == psRemove->psNextLink->psPriorLink);
		psRemove->psNextLink->psPriorLink = psRemove->psPriorLink;
	}

	// Get rid of next/prior link now that this is not part of the list
	// any longer.
	psRemove->psNextLink = NULL;
	psRemove->psPriorLink = NULL;
}

// Adds a control to a window's control child list
static EStatus WindowControlChildAdd(SWindowControlChild **ppsControlChildHead,
									 EControlHandle eControlHandle)
{
	EStatus eStatus;
	SWindowControlChild *psControlChildPrior = NULL;
	SWindowControlChild *psControlChild = NULL;

	psControlChild = *ppsControlChildHead;
	while (psControlChild)
	{
		if (psControlChild->eControlHandle == eControlHandle)
		{
			// This means we're attempting to add a control to a window that's already part of it
			BASSERT(0);
		}

		psControlChildPrior = psControlChild;
		psControlChild = psControlChild->psNextLink;
	}

	MEMALLOC(psControlChild, sizeof(*psControlChild));

	psControlChild->eControlHandle = eControlHandle;
	psControlChild->psPriorLink = psControlChildPrior;
	if (psControlChildPrior)
	{
		psControlChildPrior->psNextLink = psControlChild;
	}
	else
	{
		*ppsControlChildHead = psControlChild;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// This removes a control child from a window's control list
static void WindowControlChildRemove(SWindowControlChild **ppsControlChildHead,
									 EControlHandle eControlHandle)
{
	SWindowControlChild *psControlChildPrior = NULL;
	SWindowControlChild *psControlChild = NULL;

	psControlChild = *ppsControlChildHead;
	while (psControlChild)
	{
		if (psControlChild->eControlHandle == eControlHandle)
		{
			break;
		}

		psControlChildPrior = psControlChild;
		psControlChild = psControlChild->psNextLink;
	}

	// If thie asserts, then we're trying to remove a control child from
	// a list that doesn't exist. This should not happen.
	BASSERT(psControlChild);

	if (psControlChildPrior)
	{
		BASSERT(psControlChildPrior->psNextLink == psControlChild);
		psControlChildPrior->psNextLink = psControlChild->psNextLink;
	}
	else
	{
		BASSERT(*ppsControlChildHead == psControlChild);
		*ppsControlChildHead = psControlChild->psNextLink;
	}

	SafeMemFree(psControlChild);
}

// Called every time a window handle is allocated. Handle is not locked, but the
// handle list is, effectively locking everything.
static EStatus WindowHandleAllocate(EHandleGeneric eHandleGeneric,
									void *pvHandleStructBase)
{
	SWindow *psWindow = (SWindow *) pvHandleStructBase;

	// Add this list in front of the nearest window so it's in front of everything
	WindowListAddFront(sg_psWindowNear,
					   psWindow);

	// And make a copy of the window handle
	psWindow->eWindowHandle = (EWindowHandle) eHandleGeneric;
	
	// Set window to full intensity
	psWindow->u8Intensity = 0xff;

	return(ESTATUS_OK);
}

// Called every time a window handle is deallocated. Handle is locked.
static EStatus WindowHandleDeallocate(EHandleGeneric eHandleGeneric,
									  void *pvHandleStructBase)
{
	SWindow *psWindow = (SWindow *) pvHandleStructBase;

	// Remove from the master window list
	WindowListRemove(psWindow);

	// Get rid of the window image
	GraphicsDestroyImage(&psWindow->psWindowImage);

	psWindow->u32XSize = 0;
	psWindow->u32YSize = 0;

	psWindow->s32XPos = 0;
	psWindow->s32YPos = 0;

	psWindow->bVisible = FALSE;

	psWindow->u8Intensity = 0;

	// Set the handle invalid
	HandleSetInvalid(&psWindow->eWindowHandle);

	return(ESTATUS_OK);
}

static EStatus WindowSendSignalMessage(SWindowMsg *psMsg,
									   EWindowMsg eMsg,
									   EHandleGeneric *peHandle,
									   SGraphicsImage **ppsGraphicsImage,
									   EStatus *peStatusAsync,
									   char *peString,
									   void *pvData1,
									   void *pvData2)
{
	EStatus eStatus;

	ZERO_STRUCT(*psMsg);

	psMsg->eMsg = eMsg;
	if (peHandle)
	{
		psMsg->eHandle1 = (EHandleGeneric) *peHandle;
	}
	else
	{
		// Set it invalid
		HandleSetInvalid((EHandleGeneric *) &psMsg->eHandle1);
	}

	psMsg->peStatusAsync = peStatusAsync;
	psMsg->peString = peString;
	psMsg->pvData1 = pvData1;
	psMsg->pvData2 = pvData2;
	psMsg->ppsGraphicsImage = ppsGraphicsImage;

	eStatus = OSSemaphoreCreate(&psMsg->sSignal,
								0,
								1);
	ERR_GOTO();

	// Now submit it
	eStatus = OSQueuePut(sg_sWindowQueue,
						 (void *) psMsg,
						 OS_WAIT_INDEFINITE);
	
	if (eStatus != ESTATUS_OK)
	{
		// Couldn't submit the message. Destroy the semaphore and let
		// the error code fall through
		(void) OSSemaphoreDestroy(&psMsg->sSignal);
		goto errorExit;
	}

	// Wait for a response
	eStatus = OSSemaphoreGet(psMsg->sSignal,
							 OS_WAIT_INDEFINITE);


	// Regardless of what happens, destroy the semaphore
	if (ESTATUS_OK == eStatus)
	{
		eStatus = OSSemaphoreDestroy(&psMsg->sSignal);
	}
	else
	{
		(void) OSSemaphoreDestroy(&psMsg->sSignal);
	}

errorExit:
	return(eStatus);
}

// Function to call when we get a shutdown order
static void (*sg_psShutdownCallback)(void) = NULL;

// Sets the function called when a shutdown occurs from a UI perspective
void WindowShutdownSetCallback(void (*Callback)(void))
{
	sg_psShutdownCallback = Callback;
}

// This signals and sets the current window focus
static EStatus WindowSetFocusInternal(EWindowHandle eWindowHandle)
{
	EStatus eStatus = ESTATUS_OK;
	SWindowMsg sMsg;
	EWindowHandle eWindowHandleOld = sg_eWindowInFocus;

	// If we have a currently valid window in focus, signal it that it has lost
	// focus.
	if (WindowHandleIsValid(sg_eWindowInFocus))
	{
		CLEAR_MSG_STRUCT(sMsg);

		// Submit a focus change message
		sMsg.eMsg = EWMSG_WINDOW_FOCUS;
		sMsg.eHandle1 = (EHandleGeneric) sg_eWindowInFocus;
		eStatus = OSQueuePut(sg_sWindowQueue,
							 (void *) &sMsg,
							 OS_WAIT_INDEFINITE);

		if (eStatus != ESTATUS_OK)
		{
			SyslogFunc("Failed to deposit EWMSG_WINDOW_FOCUS (loss) - %s\n", GetErrorText(eStatus));
			ERR_GOTO();
		}

//		DebugOut("Window %.8x lost focus\n", (UINT32) sMsg.eHandle);

		// Clear the window in focus
		HandleSetInvalid((EHandleGeneric *) &sg_eWindowInFocus);
	}

	// If the incoming window is valid, then deposit a message indicating a gain
	if (WindowHandleIsValid(eWindowHandle))
	{
		CLEAR_MSG_STRUCT(sMsg);

		// Submit a focus change message
		sMsg.eMsg = EWMSG_WINDOW_FOCUS;
		sMsg.bGenericBoolean = TRUE;
		sMsg.eHandle1 = (EHandleGeneric) eWindowHandle;
		eStatus = OSQueuePut(sg_sWindowQueue,
							 (void *) &sMsg,
							 OS_WAIT_INDEFINITE);

		if (eStatus != ESTATUS_OK)
		{
			SyslogFunc("Failed to deposit EWMSG_WINDOW_FOCUS (gained) - %s\n", GetErrorText(eStatus));
			ERR_GOTO();
		}

//		DebugOut("Window %.8x gained focus\n", (UINT32) sMsg.eHandle);
		sg_eWindowInFocus = eWindowHandle;
	}

errorExit:
	return(eStatus);
}

// Returns TRUE if the window handle in question is modal
#define	WindowIsModal(eWindowHandle) WindowIsModalInternal(eWindowHandle, TRUE)
static BOOL WindowIsModalInternal(EWindowHandle eWindowHandle,
								  BOOL bLock)
{
	EStatus eStatus;
	BOOL bModal = FALSE;
	SWindow *psWindow = NULL;

	if (bLock)
	{
		eStatus = WindowListSetLock(TRUE,
									ESTATUS_OK);
		ERR_GOTO();
	}

	psWindow = sg_psWindowNear;
	while (psWindow)
	{
		if (psWindow->eWindowHandle == eWindowHandle)
		{
			bModal = psWindow->bModal;
			break;
		}

		psWindow = psWindow->psPriorLink;
	}

	if (bLock)
	{
		eStatus = WindowListSetLock(FALSE,
									eStatus);
		ERR_GOTO();
	}

errorExit:
	return(bModal);
}

// Called each time a window click or creation/destruction has changed
// something. If the incoming window handle changes from the current
// focus, then deposit appropriate messages.
static EStatus WindowFocusChangeCheck(EWindowHandle eWindowHandle)
{
	EStatus eStatus = ESTATUS_OK;

	// Disallow a focus change
	if (WindowIsModalInternal(sg_eWindowInFocus,
							  FALSE))
	{
		goto errorExit;
	}

	if (eWindowHandle != sg_eWindowInFocus)
	{
		eStatus = WindowSetFocusInternal(eWindowHandle);
	}

errorExit:
	return(eStatus);
}

// Sets focus to the incoming window (or HANDLE_INVALID to defocus everything)
EStatus WindowSetFocus(EWindowHandle eWindowHandle)
{
	return(WindowFocusChangeCheck(eWindowHandle));
}

// Creates a new window
EStatus WindowCreate(EWindowHandle *peWindowHandle,
					 INT32 s32XPos,
					 INT32 s32YPos,
					 UINT32 u32XSize,
					 UINT32 u32YSize)
{
	EStatus eStatus;
	BOOL bAllocated = FALSE;
	BOOL bWindowListAllocated = FALSE;
	SWindow *psWindow = NULL;

	// Now add it to the list of windows
	eStatus = WindowListSetLock(TRUE,
								ESTATUS_OK);
	ERR_GOTO();

	bWindowListAllocated = TRUE;

	// Go create a new window handle
	eStatus = HandleAllocate(sg_psWindowHandlePool,
							 peWindowHandle,
							 &psWindow);
	ERR_GOTO();

	// Handle allocated!
	bAllocated = TRUE;

	// Now we fill in the goodies
	psWindow->s32XPos = s32XPos;
	psWindow->s32YPos = s32YPos;
	psWindow->u32XSize = u32XSize;
	psWindow->u32YSize = u32YSize;

	// Set control handle focus to invalid
	HandleSetInvalid((EHandleGeneric *) &psWindow->eControlHandleFocus);

	// Set the focus on the newly created window
	eStatus = WindowFocusChangeCheck(*peWindowHandle);

	// No need to do an update since the window is created invisible
	eStatus = WindowSetLock(*peWindowHandle,
							NULL,
							FALSE,
							eStatus);

errorExit:
	if ((bAllocated) &&
		(eStatus != ESTATUS_OK))
	{
		// Deallocate this handle due to error
		eStatus = HandleDeallocate(sg_psWindowHandlePool,
								   peWindowHandle,
								   eStatus);
	}

	if (bWindowListAllocated)
	{
		// Unlock the window
		eStatus = WindowListSetLock(FALSE,
									eStatus);
	}

	return(eStatus);
}

EStatus WindowDestroy(EWindowHandle *peWindowHandle)
{
	EStatus eStatus;
	EStatus eStatusResult;
	SWindow *psWindow = NULL;
	SWindowMsg sMsg;

	// Set up a signaling message 
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_WINDOW_DESTROY,
									  peWindowHandle,
									  NULL,
									  &eStatusResult,
									  NULL,
									  NULL,
									  NULL);
	ERR_GOTO();

	// Clear out the window handle from the caller
	HandleSetInvalid((EHandleGeneric *) peWindowHandle);

	// Result should now be in eStatusResult
	eStatus = eStatusResult;

errorExit:
	return(eStatus);
}

// Set's an image file to a window. If peWindowImageFilename is NULL, the
// existing image is cleared.
EStatus WindowSetImageFromFile(EWindowHandle eWindowHandle,
							   char *peWindowImageFilename)
{
	EStatus eStatus;
	EStatus eStatusResult;
	SWindow *psWindow = NULL;
	SWindowMsg sMsg;
	SGraphicsImage *psGraphicsImage = NULL;
	BOOL bUpdate = FALSE;
	BOOL bWindowLocked = FALSE;

	// Go create a graphical image in OpenGL context
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_WINDOW_CREATE_GRAPHIC,
									  &eWindowHandle,
									  &psGraphicsImage,
									  &eStatusResult,
									  peWindowImageFilename,
									  NULL,
									  NULL);
	ERR_GOTO();

	// Result should now be in eStatusResult
	eStatus = eStatusResult;
	ERR_GOTO();

	// Now time to load everything up
	eStatus = GraphicsLoadImage(peWindowImageFilename,
								&psGraphicsImage->pu32RGBA,
								&psGraphicsImage->u32TotalXSize,
								&psGraphicsImage->u32TotalYSize);
	ERR_GOTO();

	bUpdate = TRUE;

	// Loaded!
	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	bWindowLocked = TRUE;

	// If we have an image currently and are getting rid of it, then update the image
	if ((psWindow->psWindowImage) &&
		(NULL == peWindowImageFilename))
	{
		bUpdate = TRUE;
	}

	// Destroy whatever window image might be there
	GraphicsDestroyImage(&psWindow->psWindowImage);

	// Assign the new image
	psWindow->psWindowImage = psGraphicsImage;

	// Signal an update if the window is visible
	if ((psWindow->bVisible) &&
		(bUpdate))
	{
		WindowUpdated();
	}

	// All good

errorExit:
	// If the status isn't successful, then kill whatever image we have loaded
	if (eStatus != ESTATUS_OK)
	{
		GraphicsDestroyImage(&psGraphicsImage);
	}

	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}

	return(eStatus);
}

EStatus WindowSetImage(EWindowHandle eWindowHandle,
					   UINT32 *pu32RGBA,
					   UINT32 u32XSize,
					   UINT32 u32YSize,
					   BOOL bAutoDeallocate)
{
	EStatus eStatus;
	SWindow *psWindow = NULL;
	BOOL bVisible = FALSE;
	SWindowMsg sMsg;
	SGraphicsImage *psGraphicsImage = NULL;
	EStatus eStatusResult;

	// Go create a graphical image in OpenGL context
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_WINDOW_CREATE_GRAPHIC,
									  &eWindowHandle,
									  &psGraphicsImage,
									  &eStatusResult,
									  NULL,
									  NULL,
									  NULL);
	ERR_GOTO();
	eStatus = eStatusResult;
	ERR_GOTO();

	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	psWindow->psWindowImage = psGraphicsImage;

	GraphicsSetImage(psWindow->psWindowImage,
					 pu32RGBA,
					 u32XSize,
					 u32YSize,
					 bAutoDeallocate);

	bVisible = psWindow->bVisible;

	eStatus = WindowSetLock(eWindowHandle,
							NULL,
							FALSE,
							eStatus);


	if (bVisible)
	{
		WindowUpdated();
	}

errorExit:
	return(eStatus);
}

EStatus WindowSetVisible(EWindowHandle eWindowHandle,
						 BOOL bVisible)
{
	EStatus eStatus = ESTATUS_OK;
	SGraphicsImage *psGraphicsImage = NULL;
	SWindow *psWindow = NULL;
	BOOL bWindowLocked = FALSE;
	BOOL bUpdated = FALSE;

	// Loaded!
	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	bWindowLocked = TRUE;

	// Did the visibility of the window change?
	if (psWindow->bVisible != bVisible)
	{
		bUpdated = TRUE;
	}

	psWindow->bVisible = bVisible;

	// All good

errorExit:
	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}
	
	// Signal an update if the window position has changed
	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// Sets window disabled/enabled
EStatus WindowSetDisabled(EWindowHandle eWindowHandle,
						  BOOL bDisabled)
{
	EStatus eStatus = ESTATUS_OK;
	SGraphicsImage *psGraphicsImage = NULL;
	SWindow *psWindow = NULL;
	BOOL bWindowLocked = FALSE;
	BOOL bUpdated = FALSE;

	// Loaded!
	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	bWindowLocked = TRUE;

	// Did the visibility of the window change?
	if (psWindow->bDisabled != bDisabled)
	{
		bDisabled = TRUE;
	}

	psWindow->bDisabled = bDisabled;

	// All good

errorExit:
	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}
	
	// Signal an update if the window position has changed
	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// Sets a window as modal or not
EStatus WindowSetModal(EWindowHandle eWindowHandle,
					   BOOL bModal)
{
	EStatus eStatus = ESTATUS_OK;
	SWindow *psWindow = NULL;
	BOOL bWindowLocked = FALSE;

	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();
	bWindowLocked = TRUE;

	psWindow->bModal = bModal;

	// All good

errorExit:
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}

	return(eStatus);
}


EStatus WindowSetPosition(EWindowHandle eWindowHandle,
						  INT32 s32XPos,
						  INT32 s32YPos)
{
	EStatus eStatus = ESTATUS_OK;
	SGraphicsImage *psGraphicsImage = NULL;
	SWindow *psWindow = NULL;
	BOOL bWindowLocked = FALSE;
	BOOL bUpdated = FALSE;

	// Loaded!
	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	bWindowLocked = TRUE;

	// Did the position change?
	if ((s32XPos != psWindow->s32XPos) ||
		(s32YPos != psWindow->s32YPos))
	{
		bUpdated = TRUE;
	}

	psWindow->s32XPos = s32XPos;
	psWindow->s32YPos = s32YPos;

	// All good

errorExit:
	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}
	
	// Signal an update if the window position has changed
	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// Sets the focus on s specific control within a window regardless of
// whatever is in focus.
EStatus WindowControlSetFocus(EWindowHandle eWindowHandle,
							  EHandleGeneric eControlHandle)
{
	EStatus eStatus;
	SWindow *psWindow = NULL;
	BOOL bWindowLocked = FALSE;
	BOOL bUpdated = FALSE;
	SWindowMsg sMsg;

	// Loaded!
	eStatus = WindowSetLock(eWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();
	bWindowLocked = TRUE;

	CLEAR_MSG_STRUCT(sMsg);

	sMsg.eMsg = EWMSG_CONTROL_FOCUS;
	sMsg.eHandle1 = (EHandleGeneric) psWindow->eControlHandleFocus;
	sMsg.eHandle2 = eControlHandle;

	eStatus = OSQueuePut(sg_sWindowQueue,
						 (void *) &sMsg,
						 OS_WAIT_INDEFINITE);

	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Failed to deposit EWMSG_QUIT - %s\n", GetErrorText(eStatus));
	}

errorExit:
	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(eWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}
	
	return(eStatus);
}


// This is called when it's time to redraw a frame
static void WindowFrame(void)
{
	EStatus eStatus;
	SWindow *psWindow = NULL;
	BOOL bFrameStarted = FALSE;
	BOOL bWindowListLocked = FALSE;
	UINT8 u8Intensity = 0xff;

	// Indicate that we've gotten the message we've been signaled
	sg_bFrameSubmitted = FALSE;

	// If it's locked, then don't draw anything
	if (sg_u8FrameLockCount)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// And indicate no frame pending
	sg_bFramePending = FALSE;

	// Lock the window list
	eStatus = WindowListSetLock(TRUE,
								ESTATUS_OK);
	ERR_GOTO();
	bWindowListLocked = TRUE;

	// We're starting a frame
	GraphicsStartFrame();
	bFrameStarted = TRUE;

	// Is the currently active window modal? If so, cut the intensity in half
	// until we hit the window that's modal
	if (WindowIsModalInternal(sg_eWindowInFocus,
							  FALSE))
	{
		u8Intensity >>= 2;
	}

	// Draw back to front. 
	psWindow = sg_psWindowFar;
	while (psWindow)
	{
		// If we've hit our modal window, then crank the intensity back up again
		if (psWindow->eWindowHandle == sg_eWindowInFocus)
		{
			u8Intensity = 0xff;
		}

		if (psWindow->bVisible)
		{
			SWindowControlChild *psControlChild;

			// Set the intensity (even if it's temporary)
			if (psWindow->psWindowImage)
			{
				GraphicsSetIntensity(psWindow->psWindowImage,
									 (u8Intensity * psWindow->u8Intensity) >> 8);
			}

			// If this window has a baseline image, draw it
			eStatus = GraphicsDraw(psWindow->psWindowImage,
								   psWindow->s32XPos,
								   psWindow->s32YPos,
								   FALSE,		// Upper left origin
								   0.0);		// Angle is 0 degrees
			ERR_GOTO();

			// Run through all the controls for this window and draw anything that needs it
			psControlChild = psWindow->psWindowControlChildList;
			while (psControlChild)
			{
				eStatus = ControlDraw(psControlChild->eControlHandle,
									  (u8Intensity * psWindow->u8Intensity) >> 8,
									  psWindow->s32XPos,
									  psWindow->s32YPos);
				ERR_GOTO();

				psControlChild = psControlChild->psNextLink;
			}
		}
		else
		{
			// Window isn't visible. Don't draw anything.
		}

		// Next window
		psWindow = psWindow->psNextLink;
	}

errorExit:
	if (bWindowListLocked)
	{
		eStatus = WindowListSetLock(FALSE,
									eStatus);
	}

	if (bFrameStarted)
	{
		// Now we're done with it
		GraphicsEndFrame();
	}

	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Failed - %s\n", GetErrorText(eStatus));
	}
}

// Signals a waiting window message that we're done, here
static EStatus WindowSignalComplete(SWindowMsg *psMsg,
									EStatus eStatusAsync)
{
	EStatus eStatus;

	if (psMsg->peStatusAsync)
	{
		*psMsg->peStatusAsync = eStatusAsync;
	}

	eStatus = OSSemaphorePut(psMsg->sSignal,
							 1);
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Can't signal command %u - %s\n", psMsg->eMsg, GetErrorText(eStatus));
	}

	return(eStatus);
}

static BOOL sg_bLoaded = FALSE;

// This will destroy a window and its children, and its children's children, etc...
static EStatus WindowDestroyChain(EWindowHandle *peWindowHandle,
								  BOOL *pbUpdate)
{
	EStatus eStatus;
	BOOL bWindowLocked;
	SWindow *psWindow = NULL;
	SWindowList *psChild = NULL;

	eStatus = WindowSetLock(*peWindowHandle,
							&psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	bWindowLocked = TRUE;

	psChild = psWindow->psWindowChildList;

	// Destroy this window's children, and their children's children
	while (psWindow->psWindowChildList)
	{
		eStatus = WindowDestroyChain(&psChild->eWindowHandle,
									 pbUpdate);
		ERR_GOTO();

		psChild = psWindow->psWindowChildList;
		psWindow->psWindowChildList = psWindow->psWindowChildList->psNextLink;
		SafeMemFree(psChild);
	}

errorExit:
	// If the window is locked, unlock it
	if (bWindowLocked)
	{
		eStatus = WindowSetLock(*peWindowHandle,
								NULL,
								FALSE,
								eStatus);
	}

	if (ESTATUS_OK == eStatus)
	{
		// Release the handle
		eStatus = HandleDeallocate(sg_psWindowHandlePool,
								   (EHandleGeneric *) peWindowHandle,
								   eStatus);

		if ((ESTATUS_OK == eStatus) &&
			(pbUpdate))
		{
			*pbUpdate = TRUE;
		}
	}

	return(eStatus);
}

// This will destroy a parent window and all of its children
static EStatus WindowDestroyTree(SWindowMsg *psMsg)
{
	EStatus eStatus = ESTATUS_OK;
	SGraphicsImage *psGraphicsImage = NULL;
	SWindow *psWindow = NULL;
	BOOL bWindowListLocked = FALSE;
	BOOL bUpdate = FALSE;

	// Lock the list
	eStatus = WindowListSetLock(TRUE,
								ESTATUS_OK);
	ERR_GOTO();

	bWindowListLocked = TRUE;

	// Go destroy the tree
	eStatus = WindowDestroyChain(&psMsg->eHandle1,
								 &bUpdate);

	// If one of the windows were deleted, then do a frame update
	if (bUpdate)
	{
		WindowUpdated();
	}

errorExit:
	if (bWindowListLocked)
	{
		eStatus = WindowListSetLock(FALSE,
									eStatus);
	}

	return(eStatus);
}

// Rate of timer events
#define		EVENT_TIMER_HZ		100

// Display the event in a human readable method
static void WindowDisplayEvent(SDL_Event *psEvent)
{
	switch (psEvent->window.event) {
	case SDL_WINDOWEVENT_SHOWN:
		DebugOutFunc("Window %d shown\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_HIDDEN:
		DebugOutFunc("Window %d hidden\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_EXPOSED:
		DebugOutFunc("Window %d exposed\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_MOVED:
		DebugOutFunc("Window %d moved to %d,%d\n",
				psEvent->window.windowID, psEvent->window.data1,
				psEvent->window.data2);
		break;
	case SDL_WINDOWEVENT_RESIZED:
		DebugOutFunc("Window %d resized to %dx%d\n",
				psEvent->window.windowID, psEvent->window.data1,
				psEvent->window.data2);
		break;
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		DebugOutFunc("Window %d size changed to %dx%d\n",
				psEvent->window.windowID, psEvent->window.data1,
				psEvent->window.data2);
		break;
	case SDL_WINDOWEVENT_MINIMIZED:
		DebugOutFunc("Window %d minimized\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_MAXIMIZED:
		DebugOutFunc("Window %d maximized\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_RESTORED:
		DebugOutFunc("Window %d restored\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_ENTER:
		DebugOutFunc("Mouse entered window %d\n",
				psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_LEAVE:
		DebugOutFunc("Mouse left window %d\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		DebugOutFunc("Window %d gained keyboard focus\n",
				psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_FOCUS_LOST:
		DebugOutFunc("Window %d lost keyboard focus\n",
				psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_CLOSE:
		DebugOutFunc("Window %d closed\n", psEvent->window.windowID);
		break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
	case SDL_WINDOWEVENT_TAKE_FOCUS:
		DebugOutFunc("Window %d is offered a focus\n", psEvent->window.windowID);
		break;
	case SDL_WINDOWEVENT_HIT_TEST:
		DebugOutFunc("Window %d has a special hit test\n", psEvent->window.windowID);
		break;
#endif
	default:
		DebugOutFunc("Window %d got unknown event %d\n",
				psEvent->window.windowID, psEvent->window.event);
		break;
	}
}

static EStatus WindowQuit(void)
{
	EStatus eStatus;
	SWindowMsg sMsg;

	CLEAR_MSG_STRUCT(sMsg);

	// Submit a quit message
	sMsg.eMsg = EWMSG_QUIT;
	eStatus = OSQueuePut(sg_sWindowQueue,
						 (void *) &sMsg,
						 OS_WAIT_INDEFINITE);

	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Failed to deposit EWMSG_QUIT - %s\n", GetErrorText(eStatus));
	}

	return(eStatus);
}

static void MouseCoordinatesPhysicalToLogical(Sint32 s32SDLX,
											  Sint32 s32SDLY,
											  UINT32 *pu32XPos,
											  UINT32 *pu32YPos)
{
	UINT32 u32XDisplaySize;
	UINT32 u32YDisplaySize;
	UINT32 u32XVirtualSize;
	UINT32 u32YVirtualSize;
	float fXScale;
	float fYScale;

	// Scale the mouse coordinates to the physical surface
	GraphicsGetPhysicalSurfaceSize(&u32XDisplaySize,
									&u32YDisplaySize);

	GraphicsGetVirtualSurfaceSize(&u32XVirtualSize,
									&u32YVirtualSize);

	fXScale = (float) u32XVirtualSize / (float) u32XDisplaySize;
	fYScale = (float) u32YVirtualSize / (float) u32YDisplaySize;

	if (pu32XPos)
	{
		*pu32XPos = (UINT32) (s32SDLX * fXScale);
	}

	if (pu32YPos)
	{
		*pu32YPos = (UINT32) (s32SDLY * fYScale);
	}
}

// Called when a window gains or loses focus
static void WindowSetFocusMessage(SWindowMsg *psMsg)
{
	char *peDir = "lost";

	if (psMsg->bGenericBoolean)
	{
		peDir = "gained";
	}

	DebugOutFunc("Window %.8x - %s focus\n", (UINT32) psMsg->eHandle1, peDir);
}

// Figures out which window is present at the incoming coordinates
static EWindowHandle WindowGetHandleByCoordinates(INT32 s32XPos,
												  INT32 s32YPos)
{
	SWindow *psWindow;
	EWindowHandle eWindowHandle;

	HandleSetInvalid((EHandleGeneric *) &eWindowHandle);

	psWindow = sg_psWindowNear;

	while (psWindow)
	{
		// Skip this window if it's disabled or not visible
		if ((FALSE == psWindow->bVisible) ||
			(psWindow->bDisabled))
		{
			// Do nothing - move to the next window
		}
		else
		if ((s32XPos < psWindow->s32XPos) || (s32XPos >= (psWindow->s32XPos + (INT32) psWindow->u32XSize)))
		{
			// Outside horizontally
		}
		else
		if ((s32YPos < psWindow->s32YPos) || (s32YPos >= (psWindow->s32YPos + (INT32) psWindow->u32YSize)))
		{
			// Outside vertically
		}
		else
		{
			eWindowHandle = psWindow->eWindowHandle;
			break;
		}

		psWindow = psWindow->psPriorLink;
	}

	return(eWindowHandle);
}

// Set TRUE if the mouse is in our window/area
static BOOL sg_bMouseInWindow = FALSE;

// Keyboard focus
static BOOL sg_bKeyboardFocus = FALSE;

// Current mouse button state
static UINT8 sg_u8MouseButtonState;

// Current state of every key scancode
static BOOL sg_bScancodePressed[SDL_NUM_SCANCODES];

BOOL WindowScancodeGetState(SDL_Scancode eScancode)
{
	BASSERT(eScancode < SDL_NUM_SCANCODES);
	return(sg_bScancodePressed[eScancode]);
}

// Process input events from SDL
static void WindowProcessInputEvents(void)
{
	int s32EventStatus = 1;
	SDL_Event sEvent;
	EStatus eStatus;

	// Loop through any events SDL has received
	do
	{
		s32EventStatus = SDL_PollEvent(&sEvent);

		// 1==Something in the event queue, 0=not
		if (1 == s32EventStatus)
		{
			switch (sEvent.type)
			{
				case SDL_WINDOWEVENT:
				{
					if (SDL_WINDOWEVENT_CLOSE == sEvent.window.event)
					{
						// Window close/shutdown - also generates an SQL_QUIT
					}
					else
					if (SDL_WINDOWEVENT_LEAVE == sEvent.window.event)
					{
						// Mouse has left the window
						sg_bMouseInWindow = FALSE;
					}
					else
					if (SDL_WINDOWEVENT_ENTER == sEvent.window.event)
					{
						// Mouse has entered the window
						sg_bMouseInWindow = TRUE;
					}
					else
					if (SDL_WINDOWEVENT_FOCUS_GAINED == sEvent.window.event)
					{
						sg_bKeyboardFocus = TRUE;
						sg_u32FrameBurst = 7*60;

						WindowUpdated();
					}
					else
					if (SDL_WINDOWEVENT_FOCUS_LOST == sEvent.window.event)
					{
						sg_bKeyboardFocus = FALSE;
					}
					else
					if (SDL_WINDOWEVENT_MOVED == sEvent.window.event)
					{
						// Windowed mode and it got moved to:
						// x = psEvent->window.window.data1
						// y = psEvent->window.window.data2
					}
					else
					{
						// Don't know what it is - we don't care about the rest of this
						// WindowDisplayEvent(&sEvent);
					}
					break;
				}

				case SDL_QUIT:
				{
					(void) WindowQuit();
					break;
				}

				case SDL_MOUSEMOTION:
				{
					// Handle mouse motion - only eat the message if it's in the window and the
					// keyboard/app is in focus
					if ((sg_bMouseInWindow) && (sg_bKeyboardFocus))
					{
						SWindowMsg sMsg;
						UINT32 u32MouseX;
						UINT32 u32MouseY;

						MouseCoordinatesPhysicalToLogical(sEvent.motion.x,
														  sEvent.motion.y,
														  &u32MouseX,
														  &u32MouseY);

						CLEAR_MSG_STRUCT(sMsg);
						sMsg.u64Data1 = ((UINT64) (u32MouseY) << 32) |
										((UINT32) (u32MouseX));
						sMsg.eMsg = EWMSG_MOUSE_MOVEMENT;

						// Deposit the mouse movement in the queue
						eStatus = OSQueuePut(sg_sWindowQueue,
											 (void *) &sMsg,
											 OS_WAIT_INDEFINITE);
						if (eStatus != ESTATUS_OK)
						{
							SyslogFunc("Failed to deposit mouse movement queue message - %s\n", GetErrorText(eStatus));
						}
					}
					break;
				}

				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
				{
					// Button down
					if ((sg_bMouseInWindow) && (sg_bKeyboardFocus))
					{
						SWindowMsg sMsg;
						UINT32 u32MouseX;
						UINT32 u32MouseY;

						MouseCoordinatesPhysicalToLogical(sEvent.button.x,
														  sEvent.button.y,
														  &u32MouseX,
														  &u32MouseY);

						CLEAR_MSG_STRUCT(sMsg);
						sMsg.u64Data1 = ((UINT64) (u32MouseY) << 32) |
										((UINT32) (u32MouseX));

						CLEAR_MSG_STRUCT(sMsg);

						sMsg.eMsg = EWMSG_MOUSE_BUTTON;

						// If we're hitting the button, set bGenericButton to TRUE
						if (SDL_MOUSEBUTTONDOWN == sEvent.type)
						{
							sMsg.bGenericBoolean = TRUE;
						}

						// u64Data1 is as follows:
						// Bits 0-7 - Mouse button. One of the following:
						//
						// WINDOW_MOUSEBUTTON_LEFT		0x01
						// WINDOW_MOUSEBUTTON_MIDDLE	0x02
						// WINDOW_MOUSEBUTTON_RIGHT		0x04
						// WINDOW_MOUSEBUTTON_X1		0x08
						// WINDOW_MOUSEBUTTON_X2		0x10
						//
						// Bits 8-39  - X Position
						// Bits 40-70 - Y Position
						//

						if (SDL_BUTTON_LEFT == sEvent.button.button)
						{
							sMsg.u64Data1 = WINDOW_MOUSEBUTTON_LEFT;
						}
						else
						if (SDL_BUTTON_MIDDLE == sEvent.button.button)
						{
							sMsg.u64Data1 = WINDOW_MOUSEBUTTON_MIDDLE;
						}
						else
						if (SDL_BUTTON_RIGHT == sEvent.button.button)
						{
							sMsg.u64Data1 = WINDOW_MOUSEBUTTON_RIGHT;
						}
						else
						if (SDL_BUTTON_X1 == sEvent.button.button)
						{
							sMsg.u64Data1 = WINDOW_MOUSEBUTTON_X1;
						}
						else
						if (SDL_BUTTON_X2 == sEvent.button.button)
						{
							sMsg.u64Data1 = WINDOW_MOUSEBUTTON_X2;
						}
						else
						{
							SyslogFunc("Unknown mouse button - Button=%u, state=%u, clicks=%u\n", sEvent.button.button, sEvent.button.state, sEvent.button.clicks);
						}

						if (sMsg.u64Data1)
						{
							// Add in the mouse X and Y position
							sMsg.u64Data1 |= (((UINT64) u32MouseX) << 8);
							sMsg.u64Data1 |= (((UINT64) u32MouseY) << 40);

							// We've got something. Deposit a message.
							eStatus = OSQueuePut(sg_sWindowQueue,
												 (void *) &sMsg,
												 OS_WAIT_INDEFINITE);
							if (eStatus != ESTATUS_OK)
							{
								SyslogFunc("Failed to deposit mouse button queue message - %s\n", GetErrorText(eStatus));
							}
						}
					}

					break;
				}

				case SDL_MOUSEWHEEL:
				{
					// Mouse wheel
					if ((sg_bMouseInWindow) && (sg_bKeyboardFocus))
					{
						SWindowMsg sMsg;

						// We only care about the Y axis. Bottom 32 bits.
						sMsg.eMsg = EWMSG_MOUSE_WHEEL;
						sMsg.u64Data1 = ((INT32) sEvent.wheel.y);

						eStatus = OSQueuePut(sg_sWindowQueue,
												(void *) &sMsg,
												OS_WAIT_INDEFINITE);
						if (eStatus != ESTATUS_OK)
						{
							SyslogFunc("Failed to deposit mouse wheel queue message - %s\n", GetErrorText(eStatus));
						}
					}

					break;
				}

				case SDL_TEXTINPUT:
				{
					// UTF-8 Input of whatever was typed
					if ((sg_bMouseInWindow) && (sg_bKeyboardFocus))
					{
						SWindowMsg sMsg;
						UINT32 u32Len;

						CLEAR_MSG_STRUCT(sMsg);

						// Key scan state
						sMsg.eMsg = EWMSG_UTF8_INPUT;

						u32Len = UTF8strlen(sEvent.text.text);
						if (u32Len)
						{
							// Allocate a small buffer for this UTF-8 character
							sMsg.peString = (char *) MemAlloc(u32Len + 1);
							if (NULL == sMsg.peString)
							{
								SyslogFunc("Out of memory while attempting to allocate space for a key messsage\n");
							}
							else
							{
								memcpy((void *) sMsg.peString, sEvent.text.text, (size_t) u32Len);
								eStatus = OSQueuePut(sg_sWindowQueue,
														(void *) &sMsg,
														OS_WAIT_INDEFINITE);
								if (eStatus != ESTATUS_OK)
								{
									SyslogFunc("Failed to deposit text input message - %s\n", GetErrorText(eStatus));
									SafeMemFree(sMsg.peString);
								}
							}
						}
					}


					break;
				}

				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					if ((sg_bMouseInWindow) && (sg_bKeyboardFocus))
					{
						SWindowMsg sMsg;

						CLEAR_MSG_STRUCT(sMsg);

						// Key scan state
						sMsg.eMsg = EWMSG_KEYSCAN;

						// If it'e key down, then set the boolean to TRUE
						if (SDL_KEYDOWN == sEvent.type)
						{
							sMsg.bGenericBoolean = TRUE;
						}

						// Scancode in u64Data1
						sMsg.u64Data1 = (UINT64) sEvent.key.keysym.scancode;

						eStatus = OSQueuePut(sg_sWindowQueue,
												(void *) &sMsg,
												OS_WAIT_INDEFINITE);
						if (eStatus != ESTATUS_OK)
						{
							SyslogFunc("Failed to deposit mouse wheel queue message - %s\n", GetErrorText(eStatus));
						}
					}

					break;
				}
				
				case SDL_FINGERDOWN:
				case SDL_FINGERUP:
				case SDL_FINGERMOTION:
				{
					// Ignore finger events - not needed - mouse events handle it
					break;
				}

				default:
				{
					DebugOutFunc("SDL Unknown event type: %u - %.4x\n", sEvent.type, sEvent.type);
				}
			}
		}
	}
	while (1 == s32EventStatus);
}

static void WindowProcessInputEventsTimer(STimer *psTimer,
										  void *pvParameter)
{
	EStatus eStatus;
	SWindowMsg sMsg;

	CLEAR_MSG_STRUCT(sMsg);

	// Submit an event message to poll for events
	sMsg.eMsg = EWMSG_EVENTS;
	eStatus = OSQueuePut(sg_sWindowQueue,
						 (void *) &sMsg,
						 OS_WAIT_INDEFINITE);
}

static EStatus WindowCreateControlInternal(SWindowMsg *psMsg)
{
	EStatus eStatus;
	SControl *psControl = (SControl *) psMsg->pvData1;
	SWindow *psWindow = NULL;

	// Lock this window
	eStatus = WindowSetLock(psControl->eWindowHandle,
						    &psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	// Go create the control
	eStatus = ControlCreateCallback((SControl *) psMsg->pvData1,
									psMsg->pvData2);

	if (ESTATUS_OK == eStatus)
	{
		// All good - add it to the list
		eStatus = WindowControlChildAdd(&psWindow->psWindowControlChildList,
										psControl->eControlHandle);
	}

	// If this control has a periodic callback, then add it to the list for this window
	if ((ESTATUS_OK == eStatus) &&
		(psControl->psMethods->Periodic))
	{
		SControlList *psControlPeriodic = NULL;

		MEMALLOC(psControlPeriodic, sizeof(*psControlPeriodic));
		psControlPeriodic->eControlHandle = psControl->eControlHandle;
		psControlPeriodic->psNextLink = psWindow->psWindowControlPeriodicList;
		psWindow->psWindowControlPeriodicList = psControlPeriodic;
	}

	// Unlock this handle
	eStatus = WindowSetLock(psControl->eWindowHandle,
						    NULL,
							FALSE,
							eStatus);

errorExit:
	return(eStatus);
}

EStatus WindowRemoveControl(EWindowHandle eWindowHandle,
							EHandleGeneric eControlHandle)
{
	EStatus eStatus;

	SWindow *psWindow = NULL;

	// Lock this window
	eStatus = HandleSetLock(sg_psWindowHandlePool,
							NULL,
							(EHandleGeneric) eWindowHandle,
							EHANDLE_NONE,
							(void **) &psWindow,
							TRUE,
							ESTATUS_OK);
	ERR_GOTO();

	WindowControlChildRemove(&psWindow->psWindowControlChildList,
							 (EControlHandle) eControlHandle);

errorExit:
	return(eStatus);
}

static EStatus WindowDestroyControlInternal(SWindowMsg *psMsg)
{
	EStatus eStatus;

	// Go create the control
	eStatus = ControlDestroyCallback(psMsg->eHandle1);
	return(eStatus);
}

static EStatus WindowConfigureControlInternal(SWindowMsg *psMsg)
{
	EStatus eStatus;

	// Go configure the control
	eStatus = ControlConfigureCallback(psMsg->eHandle1,
									   psMsg->pvData1);

	return(eStatus);
}

// Handles queued mouse button presses/releases
static EStatus WindowButtonPressProcess(EWindowHandle eWindowHandle,
										UINT8 u8ButtonMask,
										BOOL bPressed,
										INT32 s32MouseX,
										INT32 s32MouseY)
{
	EStatus eStatus;
	SWindow *psWindow = NULL;
	SWindowControlChild *psControl = NULL;
	SWindowMsg sMsg;
	INT32 s32MouseXOriginal;
	INT32 s32MouseYOriginal;

	CLEAR_MSG_STRUCT(sMsg);

	// Get the window pointer
	eStatus = WindowGetStructByHandle(eWindowHandle,
									  &psWindow);
	if (ESTATUS_HANDLE_NOT_ALLOCATED == eStatus)
	{
		// Means we clicked on something outside of all windows. Just use the currently
		// active window as a reference if there is one
		eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
										  &psWindow);
		if (ESTATUS_HANDLE_NOT_ALLOCATED == eStatus)
		{
			// This means there isnt' a window in focus or it went away. Just return without
			// spewing an error on the console
			eStatus = ESTATUS_OK;
			goto errorExit;
		}

		// Any other error, bail out
		ERR_GOTO();

		// Coordinates we give to the control will be outside the currently active window, but
		// that's OK since someone can drag and release off the active window
	}

	// Set invalid handles to start with
	HandleSetInvalid((EHandleGeneric *) &sMsg.eHandle1);
	HandleSetInvalid((EHandleGeneric *) &sMsg.eHandle2);

	// Got our window! Let's subtract off the window x/y coordinates to make it window relative
	// coordinates
	s32MouseX -= psWindow->s32XPos;
	s32MouseY -= psWindow->s32YPos;

	// Store mouse click position (window relative) and the button mask for this event
	sMsg.u64Data1 = u8ButtonMask;
	sMsg.u64Data1 |= (((UINT64) s32MouseX) << 8);
	sMsg.u64Data2 = (((UINT64) s32MouseY));
	sMsg.bGenericBoolean = bPressed;

	// Store the original window relative mouse position
	s32MouseXOriginal = s32MouseX;
	s32MouseYOriginal = s32MouseY;

	if (bPressed)
	{
		BOOL bSendFocus = TRUE;

		// Find the end of all controls and search it front to back
		psControl = psWindow->psWindowControlChildList;
		while (psControl && psControl->psNextLink)
		{
			psControl = psControl->psNextLink;
		}

		// We're now potentially selecting a new control - search things backwards
		while (psControl)
		{
			// Restore the original window relative mouse position since it might get rotated
			// when converted to control relative coordinates.
			s32MouseX = s32MouseXOriginal;
			s32MouseY = s32MouseYOriginal;

			// Go see if this x/y offset is hitting the control
			eStatus = ControlHitTest(eWindowHandle,
									 psControl->eControlHandle,
									 u8ButtonMask,
									 &s32MouseX,
									 &s32MouseY,
									 FALSE);
			if (ESTATUS_OK == eStatus)
			{
				// We've gotten a hit. s32MouseX and s32MouseY are now control
				// relative UNROTATED coordinates, so we need to update the
				// positions.
				sMsg.u64Data1 = u8ButtonMask;
				sMsg.u64Data1 |= (((UINT64) s32MouseX) << 8);
				sMsg.u64Data2 = ((UINT64) s32MouseY);
				break;
			}
			else
			if (ESTATUS_UI_CONTROL_NOT_HIT == eStatus)
			{
				// It didn't hit this control for whatever reason... Set to OK, since it
				// isn't an actual error that we want to return.
				eStatus = ESTATUS_OK;
			}
			else
			{
				// Some other error
				goto errorExit;
			}

			psControl = psControl->psPriorLink;
		}
		
		// Send a control focus change message
		sMsg.eMsg = EWMSG_CONTROL_FOCUS;

		// Our current control
		sMsg.eHandle1 = psWindow->eControlHandleFocus;

		// If we have a new control, then we need to record it
		if (psControl)
		{
			if (psControl->eControlHandle != psWindow->eControlHandleFocus)
			{
				// Changed focus
				sMsg.eHandle2 = psControl->eControlHandle;
			}
			else
			{
				// Hasn't changed
				bSendFocus = FALSE;
			}
		}

		// If both control handles are invalid, then don't send anything
		if ((FALSE == ControlHandleIsValid(sMsg.eHandle1)) &&
			(FALSE == ControlHandleIsValid(sMsg.eHandle2)))
		{
			// No more focus
			bSendFocus = FALSE;
		}

		// Only send a focus message if we need one
		if (bSendFocus)
		{
			// Make sure we're not sending conflicting messages to the control
			BASSERT(sMsg.eHandle1 != sMsg.eHandle2);

			// Put a focus control message in the queue
			eStatus = OSQueuePut(sg_sWindowQueue,
									(void *) &sMsg,
									OS_WAIT_INDEFINITE);
			ERR_GOTO();
		}

		// If we have a control and the new control isn't what our current control
		// handle focus is, then let the control know.
		if (psControl)
		{
			// Record the new control whatever it may be
			psWindow->eControlHandleFocus = psControl->eControlHandle;

			// Now desposit a message for the button
			sMsg.eMsg = EWMSG_CONTROL_BUTTON;
			sMsg.eHandle1 = (EHandleGeneric) psControl->eControlHandle;
			eStatus = OSQueuePut(sg_sWindowQueue,
								 (void *) &sMsg,
								 OS_WAIT_INDEFINITE);
			ERR_GOTO();

			psWindow->bControlButtonHeld = TRUE;
		}	
	}
	else
	{
		if (psWindow->eWindowHandle != sg_eWindowInFocus)
		{
			// This means we've released a button on a window that isn't in focus. Change
			// the window in question.
			eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
											  &psWindow);
			if (ESTATUS_HANDLE_NOT_ALLOCATED == eStatus)
			{
				// This means there isnt' a window in focus or it went away. Just return without
				// spewing an error on the console
				eStatus = ESTATUS_OK;
				goto errorExit;
			}

			// Any other error, bail out
			ERR_GOTO();
		}

		// Button release. Let's see if there's a control in focus and the button
		// is currently held.
		if (psWindow->bControlButtonHeld &&
			ControlHandleIsValid(psWindow->eControlHandleFocus))
		{
			EStatus eStatus;
			INT32 s32WindowRelativeXPos;
			INT32 s32WindowRelativeYPos;

			eStatus = ControlGetSizePos(psWindow->eControlHandleFocus,
										&s32WindowRelativeXPos,
										&s32WindowRelativeYPos,
										NULL,
										NULL);
			if (ESTATUS_OK == eStatus)
			{
				// Reencode the release to be focused control relative
				s32MouseX -= s32WindowRelativeXPos;
				s32MouseY -= s32WindowRelativeYPos;
				sMsg.u64Data1 = u8ButtonMask;
				sMsg.u64Data1 |= (((UINT64) s32MouseX) << 8);
				sMsg.u64Data2 = ((UINT64) s32MouseY);
			}

			sMsg.eMsg = EWMSG_CONTROL_BUTTON;
			sMsg.eHandle1 = (EHandleGeneric) psWindow->eControlHandleFocus;
			eStatus = OSQueuePut(sg_sWindowQueue,
								 (void *) &sMsg,
								 OS_WAIT_INDEFINITE);
			ERR_GOTO();

			psWindow->bControlButtonHeld = FALSE;
		}
	}

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Failed - %s\n", GetErrorText(eStatus));
	}
	return(eStatus);
}


// Main windowing thread
static void WindowMainThread(void *pvParameter)
{
	EStatus eStatus;
	UINT32 u32FPS;
	STimer *psEventTimer = NULL;
	BOOL bRunning = TRUE;
	
	SyslogFunc("Started\n");

	// Init the graphics subsystem
	eStatus = GraphicsInit(&u32FPS);
	ERR_GOTO();

	eStatus = OSEventFlagCreate(&sg_psFrameWaitEventFlag,
								0);
	ERR_GOTO();

	// Init font engine
	eStatus = FontInit();

	SyslogFunc("Graphics engine says %u FPS\n", u32FPS);
	eStatus = TimerCreate(&sg_psFrameTimer);
	ERR_GOTO();

	// Calculate the milliseconds per frame
	sg_fMsecPerFrame = (float) 1000.0 / (float) u32FPS;

	eStatus = TimerSet(sg_psFrameTimer,
					   TRUE,
					   0,
					   1000 / u32FPS,
					   WindowFrameTimerCallback,
					   (void *) (1000 / u32FPS));
	ERR_GOTO();
	
	// Create window messaging queue
	eStatus = OSQueueCreate(&sg_sWindowQueue,
							"Windowing queue",
							sizeof(SWindowMsg),
							WINDOW_QUEUE_COUNT);
	ERR_GOTO();

	// Create a handle pool
	eStatus = HandlePoolCreate(&sg_psWindowHandlePool,
							   "Window handle pool",
							   MAX_WINDOWS,
							   sizeof(SWindow),
							   offsetof(SWindow, sCriticalSection),
							   WindowHandleAllocate,
							   WindowHandleDeallocate);
	ERR_GOTO();

	eStatus = TimerStart(sg_psFrameTimer);
	ERR_GOTO();

	// Start the event timer
	eStatus = TimerCreate(&psEventTimer);
	ERR_GOTO();

	eStatus = TimerSet(psEventTimer,
					   TRUE,
					   1000 / EVENT_TIMER_HZ,
					   1000 / EVENT_TIMER_HZ,
					   WindowProcessInputEventsTimer,
					   NULL);
	ERR_GOTO();

	eStatus = TimerStart(psEventTimer);
	ERR_GOTO();

	// Now the control code
	eStatus = ControlInit();
	ERR_GOTO();

	// Signal everything's OK
	sg_eWindowStartupEStatus = eStatus;

	// Signal the foreground code
	eStatus = OSSemaphorePut(sg_sWindowStartupShutdownSemaphore,
							 1);
	BASSERT(ESTATUS_OK == eStatus);

	SyslogFunc("Windowing subsystem started\n");

	// Do a frame burst of 7 seconds of frames so the NVidia "press + z for in game overlay"
	// message goes away on its own.
	sg_u32FrameBurst = u32FPS * 7;

	// This will cause the display to be cleared to an initial state
	WindowUpdated();

	// Start processing the queue
	while (bRunning)
	{
		SWindowMsg sMsg;

		// Get a windowing message
		eStatus = OSQueueGet(sg_sWindowQueue,
							 (void *) &sMsg,
							 OS_WAIT_INDEFINITE);
		ERR_GOTO();

		switch (sMsg.eMsg)
		{
			case EWMSG_EVENTS:
			{
				SWindow *psWindow = NULL;

				// Message pump timer
				WindowProcessInputEvents();

				// Now provide periodic timers to whatever controls are part of this window
				// that need periodic timers
				eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
												  &psWindow);
				if (ESTATUS_OK == eStatus)
				{
					SControlList *psPeriodic;

					psPeriodic = psWindow->psWindowControlPeriodicList;

					while (psPeriodic)
					{
						eStatus = ControlPeriodicTimer(psPeriodic->eControlHandle,
													   1000 / EVENT_TIMER_HZ);

						psPeriodic = psPeriodic->psNextLink;
					}
				}

				break;
			}

			case EWMSG_FRAME:
			{
				// Kick off a frame render
				WindowFrame();
				break;
			}

			case EWMSG_WINDOW_CREATE_GRAPHIC:
			{
				// Load up a graphical image for a window
				eStatus = GraphicsCreateImage(sMsg.ppsGraphicsImage,
											  TRUE,
											  (UINT32) (sMsg.u64Data1),				// X Size
											  (UINT32) (sMsg.u64Data1 >> 32));		// Y Size
				(void) WindowSignalComplete(&sMsg,
											eStatus);
				break;
			}

			case EWMSG_WINDOW_DESTROY:
			{
				// Destroy a window
				eStatus = WindowDestroyTree(&sMsg);
				(void) WindowSignalComplete(&sMsg,
											eStatus);
				break;
			}

			case EWMSG_QUIT:
			{
				// User initiated quit message callback

				// Call a user defined function if
				if (sg_psShutdownCallback)
				{
					sg_psShutdownCallback();
				}

				SyslogFunc("UI Initiated quit received\n");
				break;
			}

			case EWMSG_SHUTDOWN:
			{
				SyslogFunc("Shutting down\n");
				bRunning = FALSE;
				break;
			}

			case EWMSG_MOUSE_MOVEMENT:
			{
				SWindow *psWindow = NULL;
				UINT32 u32MouseX;
				UINT32 u32MouseY;

				u32MouseX = (UINT32) sMsg.u64Data1;
				u32MouseY = (UINT32) (sMsg.u64Data1 >> 32);

				// If we have both a window in focus and a control in focus, then pass
				// any mouse movement - relative to the control - to the control module
				eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
												  &psWindow);
				if (ESTATUS_OK == eStatus)
				{
					// Got a window that's in focus. Pass the relative control position to
					// the control code for potential processing.
					(void) ControlProcessDragMouseover(psWindow->eControlHandleFocus,
													   sg_u8MouseButtonState,
													   ((INT32) u32MouseX) - psWindow->s32XPos,
													   ((INT32) u32MouseY) - psWindow->s32YPos);
				}

				break;
			}

			case EWMSG_MOUSE_BUTTON:
			{
				EWindowHandle eWindowHandle;
				INT32 s32MouseX;
				INT32 s32MouseY;
				BOOL bFocusModal = FALSE;
				EHandleGeneric eInvalidHandle;

				HandleSetInvalid(&eInvalidHandle);

				// Get modality of current window
				bFocusModal = WindowIsModal(sg_eWindowInFocus);

				// Bits 0-7 - Mouse buttons
				// Bits 8-39 - X Coordinate of press/release
				// Bits 40-71 - Y Coordinates of press/release
				s32MouseX = (INT32) (sMsg.u64Data1 >> 8);
				s32MouseY = (INT32) (sMsg.u64Data1 >> 40);

				// Get clicked-on handle
				eWindowHandle = WindowGetHandleByCoordinates(s32MouseX,
																s32MouseY);

				if (sMsg.bGenericBoolean)
				{
					// Pressed
					sg_u8MouseButtonState = (sg_u8MouseButtonState | ((UINT8) sMsg.u64Data1));
				}
				else
				{
					// Released
					sg_u8MouseButtonState = (sg_u8MouseButtonState & ((UINT8) ~sMsg.u64Data1));
				}

				if ((bFocusModal) &&
					(eWindowHandle != sg_eWindowInFocus))
				{
					// Disallow the focus change
				}
				else
				{
					if (sMsg.bGenericBoolean)
					{
						// Pressed

						// Since we have a press, look up to see which window this may
						// have intersected with and change it
						// Now check it against the current window
						eStatus = WindowFocusChangeCheck(eWindowHandle);
						if (eStatus != ESTATUS_OK)
						{
							SyslogFunc("Failed to set focus - %s\n", GetErrorText(eStatus));
						}

						// Only process this click if it's not an invalid window
						if (eWindowHandle != ((EHandleGeneric) eInvalidHandle))
						{
							WindowButtonPressProcess(eWindowHandle,				// Which window got this press?
														(UINT8) sMsg.u64Data1,	// Mouse button mask
														sMsg.bGenericBoolean,	// TRUE=Pressed, FALSE=Released
														s32MouseX,				// X/Y SCREEN position
														s32MouseY);
						}
					}
					else
					{
						// Released
						eWindowHandle = WindowGetHandleByCoordinates(s32MouseX,
																	 s32MouseY);

						WindowButtonPressProcess(eWindowHandle,					// Which window got this press?
												 (UINT8) sMsg.u64Data1,			// Mouse button mask
												 sMsg.bGenericBoolean,			// TRUE=Pressed, FALSE=Released
												 s32MouseX,						// X/Y SCREEN position
												 s32MouseY);
					}
				}

//				DebugOut("Mouse button: Button=%u, State=%u, Total=%.2x, X=%u, Y=%u\n", (UINT8) sMsg.u64Data1, sMsg.bGenericBoolean, sg_u8MouseButtonState, s32MouseX, s32MouseY);

				break;
			}

			case EWMSG_MOUSE_WHEEL:
			{
				INT32 s32Wheel;
				SWindow *psWindow = NULL;

				s32Wheel = ((INT32) sMsg.u64Data1);

				// See if we have a window in focus
				eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
												  &psWindow);
				if (ESTATUS_OK == eStatus)
				{
					// And if so, send the mouse wheel to the control code for processing
					(void) ControlProcessWheel(psWindow->eControlHandleFocus,
											   s32Wheel);
				}

				break;
			}

			case EWMSG_WINDOW_FOCUS:
			{
				WindowSetFocusMessage(&sMsg);
				break;
			}

			case EWMSG_KEYSCAN:
			{
				if (sMsg.u64Data1 >= (sizeof(sg_bScancodePressed) / sizeof(sg_bScancodePressed[0])))
				{
					SyslogFunc("Scancode beyond array - %u\n", (UINT32) sMsg.u64Data1);
				}
				else
				{
					SWindow *psWindow = NULL;
					sg_bScancodePressed[sMsg.u64Data1] = sMsg.bGenericBoolean;

					// If we have both a window in focus and a control in focus, hand it off to
					// the control module
					eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
													  &psWindow);
					if (ESTATUS_OK == eStatus)
					{
						// Got a window that's in focus. Pass the UTF8 character to the control module
						(void) ControlScancodeInputProcess(psWindow->eControlHandleFocus,
														   (SDL_Scancode) sMsg.u64Data1,
														   sMsg.bGenericBoolean,
														   FALSE);
					}
				}

				break;
			}

			case EWMSG_UTF8_INPUT:
			{
				SWindow *psWindow = NULL;

				// If we have both a window in focus and a control in focus, hand it off to
				// the control module
				eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
												  &psWindow);
				if (ESTATUS_OK == eStatus)
				{
					// Got a window that's in focus. Pass the UTF8 character to the control module
					(void) ControlUTF8InputProcess(psWindow->eControlHandleFocus,
												   sMsg.peString,
												   sMsg.bGenericBoolean);
				}

				// Clear the string
				SafeMemFree(sMsg.peString);
				break;
			}

			case EWMSG_CONTROL_CREATE:
			{
				// Create a control
				eStatus = WindowCreateControlInternal(&sMsg);
				(void) WindowSignalComplete(&sMsg,
											eStatus);
				break;
			}

			case EWMSG_CONTROL_DESTROY:
			{
				// Destroy a control
				eStatus = WindowDestroyControlInternal(&sMsg);
				(void) WindowSignalComplete(&sMsg,
											eStatus);
				break;
			}

			case EWMSG_CONTROL_CONFIGURE:
			{
				// Configure a control
				eStatus = WindowConfigureControlInternal(&sMsg);
				(void) WindowSignalComplete(&sMsg,
											eStatus);
				break;
			}

			case EWMSG_CONTROL_BUTTON:
			{
				eStatus = ControlButtonProcess((EControlHandle) sMsg.eHandle1,
											   (UINT8) sMsg.u64Data1,
											   (INT32) (sMsg.u64Data1 >> 8),
											   (INT32) (((INT64) sMsg.u64Data2)),
											   sMsg.bGenericBoolean);
				break;
			}

			case EWMSG_CONTROL_FOCUS:
			{
				char eSrcName[100];
				char eDestName[100];
				char *peSrcName = NULL;
				char *peDestName = NULL;

				eSrcName[0] = '\0';
				eDestName[0] = '\0';

				eStatus = ControlGetUserName((EControlHandle) sMsg.eHandle1,
											 &peSrcName);
				if (ESTATUS_HANDLE_NOT_ALLOCATED == eStatus)
				{
					strcpy(eSrcName, "No handle");
				}
				else
				{
					if (peSrcName)
					{
						snprintf(eSrcName, sizeof(eSrcName) - 1, "%s - %s", ControlGetName((EControlHandle) sMsg.eHandle1), peSrcName);
					}
					else
					{
						snprintf(eSrcName, sizeof(eSrcName) - 1, "%s - unnamed", ControlGetName((EControlHandle) sMsg.eHandle1));
					}
				}

				eStatus = ControlGetUserName((EControlHandle) sMsg.eHandle2,
											 &peDestName);
				if (ESTATUS_HANDLE_NOT_ALLOCATED == eStatus)
				{
					strcpy(eSrcName, "No handle");
				}
				else
				{
					if (peDestName)
					{
						snprintf(eDestName, sizeof(eDestName) - 1, "%s - %s", ControlGetName((EControlHandle) sMsg.eHandle2), peDestName);
					}
					else
					{
						snprintf(eDestName, sizeof(eDestName) - 1, "%s - unnamed", ControlGetName((EControlHandle) sMsg.eHandle2));
					}
				}

				DebugOut("Control focus changed - %.8x (%s) %.8x (%s)\n", (UINT32) sMsg.eHandle1, eSrcName, (UINT32) sMsg.eHandle2, eDestName);

				if (ControlHandleIsValid((EControlHandle) sMsg.eHandle1) ||
					ControlHandleIsValid((EControlHandle) sMsg.eHandle2))
				{
					if (sMsg.eHandle1 != sMsg.eHandle2)
					{
						// Go tell the control module about it
						eStatus = ControlProcessFocus((EControlHandle) sMsg.eHandle1,
													  (EControlHandle) sMsg.eHandle2,
													  (UINT8) sMsg.u64Data1,
													  (INT32) (sMsg.u64Data1 >> 8),
													  (INT32) (((INT64) sMsg.u64Data2)));

						if ((ESTATUS_OK == eStatus) &&
							(ControlHandleIsValid((EControlHandle) sMsg.eHandle2)))
						{
							SWindow *psWindow = NULL;

							eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
															  &psWindow);
							if (ESTATUS_OK == eStatus)
							{
								psWindow->eControlHandleFocus = sMsg.eHandle2;
							}
						}
					}
					else
					{
						// Same focus - that's OK
					}
				}
				else
				{
					SWindow *psWindow = NULL;

					eStatus = WindowGetStructByHandle(sg_eWindowInFocus,
													  &psWindow);
					if (ESTATUS_OK == eStatus)
					{
						EControlHandle eControlHandleInvalid;
						SWindowControlChild *psControl = NULL;

						HandleSetInvalid((EHandleGeneric *) &eControlHandleInvalid);

						// If this is an invalid handle + focus gained, then they've clicked
						// somewhere other than us. Let all controls know that there has been
						// a click outside of all controls
						psControl = psWindow->psWindowControlChildList;

						// Find the end of all controls
						while (psControl && psControl->psNextLink)
						{
							psControl = psControl->psNextLink;
						}

						// Inform them front to back
						while (psControl)
						{
							(void) ControlProcessFocus((EControlHandle) psControl->eControlHandle,
													   eControlHandleInvalid,
													   0,
													   0, 0);

							psControl = psControl->psPriorLink;
						}

						// We're now potentially selecting a new control - search things backwards
					}
				}

				break;
			}

			default:
			{
				SyslogFunc("Unknown Windowing message %u\n", sMsg.eMsg);
				break;
			}
		}
	}

	// Getting here means we're done. Run through and shut down all windows. This will
	// force deallocation of all resources.
	while (sg_psWindowFar)
	{
		SWindowMsg sMsg;

		CLEAR_MSG_STRUCT(sMsg);
		sMsg.eHandle1 = sg_psWindowFar->eWindowHandle;
		
		WindowDestroyTree(&sMsg);
	}

	// All done
	eStatus = ESTATUS_OK;

errorExit:
	sg_eWindowStartupEStatus = eStatus;

	// Kill off the graphics subsystem
	eStatus = GraphicsShutdown();
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Graphics shutdown failure - %s\n", GetErrorText(eStatus));
	}

	// Signal the foreground code
	eStatus = OSSemaphorePut(sg_sWindowStartupShutdownSemaphore,
							 1);

	// Die
	SyslogFunc("Terminated\n");
}

// Destroys a control
EStatus WindowControlDestroy(EControlHandle *peControlHandle)
{
	EStatus eStatus;
	EStatus eStatusResult;
	SWindowMsg sMsg;

	// Set up a destroy message 
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_CONTROL_DESTROY,
									  (EHandleGeneric *) peControlHandle,
									  NULL,
									  &eStatusResult,
									  NULL,
									  NULL,
									  NULL);
	ERR_GOTO();

	eStatus = eStatusResult;

errorExit:
	return(eStatus);
}

// Creates a control and attaches it to a window
EStatus WindowControlCreate(EWindowHandle eWindowHandle,
							SControl *psControl,
							void *pvConfigurationStructure)
{
	EStatus eStatus;
	EStatus eStatusResult;
	SWindowMsg sMsg;

	// Set up a create message 
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_CONTROL_CREATE,
									  &eWindowHandle,
									  NULL,
									  &eStatusResult,
									  NULL,
									  psControl,
									  pvConfigurationStructure);
	ERR_GOTO();

	eStatus = eStatusResult;

errorExit:
	return(eStatus);
}

// Configures an already-existing control
EStatus WindowControlConfigure(EControlHandle eControlHandle,
							   void *pvConfigurationStructure)
{
	EStatus eStatus;
	EStatus eStatusResult;
	SWindowMsg sMsg;

	// Set up a create message 
	eStatus = WindowSendSignalMessage(&sMsg,
									  EWMSG_CONTROL_CONFIGURE,
									  &eControlHandle,
									  NULL,
									  &eStatusResult,
									  NULL,
									  pvConfigurationStructure,
									  NULL);
	ERR_GOTO();

	eStatus = eStatusResult;

errorExit:
	return(eStatus);
}

EStatus WindowInit(void)
{
	EStatus eStatus;

	HandleSetInvalid((EHandleGeneric *) &sg_eWindowInFocus);

	eStatus = OSSemaphoreCreate(&sg_sWindowStartupShutdownSemaphore,
								0,
								1);
	ERR_GOTO();

	eStatus = OSCriticalSectionCreate(&sg_sWindowListLock);
	ERR_GOTO();

	eStatus = OSCriticalSectionCreate(&sg_sFrameLock);
	ERR_GOTO();

	eStatus = OSThreadCreate("Windowing library",
							 NULL,
							 WindowMainThread,
							 FALSE,
							 NULL,
							 0,
							 EOSPRIORITY_NORMAL);
	ERR_GOTO();

	eStatus = OSSemaphoreGet(sg_sWindowStartupShutdownSemaphore,
							 OS_WAIT_INDEFINITE);
	ERR_GOTO();

	eStatus = sg_eWindowStartupEStatus;

	SyslogFunc("Initialized - %s\n", GetErrorText(eStatus));

errorExit:
	return(eStatus);
}

void WindowSetMinimize(void)
{
	GraphicsSetMinimize();
}

void WindowSetMaximize(void)
{
	GraphicsSetMaximize();
	WindowUpdated();
}

EStatus WindowRenderStretch(UINT32 *pu32RGBACornerImage,
							UINT32 u32CornerXSize,
							UINT32 u32CornerYSize,
							UINT32 *pu32RGBAEdgeImage,
							UINT32 u32EdgeXSize,
							UINT32 u32EdgeYSize,
						    UINT32 u32RGBAFillPixel,
							UINT32 u32WindowStretchXSize,
							UINT32 u32WindowStretchYSize,
							UINT32 **ppu32RGBAWindowStretchImage)
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32Loop;
	UINT32 *pu32RGBADest;
	UINT32 *pu32RGBADest2;
	UINT32 *pu32RGBASrc;

	// See if X size is too small
	if ((u32CornerXSize << 1) > u32WindowStretchXSize)
	{
		eStatus = ESTATUS_UI_TOO_SMALL;
		goto errorExit;
	}

	// Now Y size
	if ((u32CornerYSize << 1) > u32WindowStretchYSize)
	{
		eStatus = ESTATUS_UI_TOO_SMALL;
		goto errorExit;
	}

	// Allocate some space for everything
	MEMALLOC(*ppu32RGBAWindowStretchImage, sizeof(**ppu32RGBAWindowStretchImage) * u32WindowStretchXSize * u32WindowStretchYSize);

	// Fill everything with the fill pixel
	u32Loop = u32WindowStretchXSize * u32WindowStretchYSize;
	pu32RGBADest = *ppu32RGBAWindowStretchImage;
	while (u32Loop)
	{
		*pu32RGBADest = u32RGBAFillPixel;
		++pu32RGBADest;
		u32Loop--;
	}

	// Copy in the upper left hand corner
	pu32RGBADest = *ppu32RGBAWindowStretchImage;
	pu32RGBASrc = pu32RGBACornerImage;
	u32Loop = u32CornerYSize;
	while (u32Loop)
	{
		memcpy((void *) pu32RGBADest, (void *) pu32RGBASrc, u32CornerXSize * sizeof(*pu32RGBASrc));
		pu32RGBASrc += u32CornerXSize;
		pu32RGBADest += u32WindowStretchXSize;
		u32Loop--;
	}

	// Now the lower left hand corner (vertical flip)
	pu32RGBADest = *ppu32RGBAWindowStretchImage + ((u32WindowStretchYSize - 1) * u32WindowStretchXSize);
	pu32RGBASrc = pu32RGBACornerImage;
	u32Loop = u32CornerYSize;
	while (u32Loop)
	{
		memcpy((void *) pu32RGBADest, (void *) pu32RGBASrc, u32CornerXSize * sizeof(*pu32RGBASrc));
		pu32RGBASrc += u32CornerXSize;
		pu32RGBADest -= u32WindowStretchXSize;
		u32Loop--;
	}

	// Now the upper right hand corner (horizontal flip)
	pu32RGBADest = *ppu32RGBAWindowStretchImage + (u32WindowStretchXSize - u32CornerXSize);
	pu32RGBASrc = (pu32RGBACornerImage + u32CornerXSize);
	u32Loop = u32CornerYSize;
	while (u32Loop)
	{
		UINT32 u32Loop2;

		u32Loop2 = u32CornerXSize;
		while (u32Loop2)
		{
			--pu32RGBASrc;
			*pu32RGBADest = *pu32RGBASrc;
			++pu32RGBADest;
			u32Loop2--;
		}

		// Jump to the end of the next line on the corner
		pu32RGBASrc += (u32CornerXSize << 1);
		pu32RGBADest += (u32WindowStretchXSize - u32CornerXSize);
		u32Loop--;
	}

	// Now the lower right hand corner (horizontal and vertical flip)
	pu32RGBADest = *ppu32RGBAWindowStretchImage + (u32WindowStretchXSize * ((u32WindowStretchYSize - u32CornerYSize) + 1)) - u32CornerXSize;
	pu32RGBASrc = (pu32RGBACornerImage + (u32CornerXSize * u32CornerYSize));
	u32Loop = u32CornerYSize;
	while (u32Loop)
	{
		UINT32 u32Loop2;

		u32Loop2 = u32CornerXSize;
		while (u32Loop2)
		{
			--pu32RGBASrc;
			*pu32RGBADest = *pu32RGBASrc;
			++pu32RGBADest;
			u32Loop2--;
		}

		// Jump to the end of the next line on the corner
		pu32RGBADest += (u32WindowStretchXSize - u32CornerXSize);
		u32Loop--;
	}

	// Now we need to do the edges

	// Top and bottom
	pu32RGBADest = *ppu32RGBAWindowStretchImage + u32CornerXSize;
	pu32RGBADest2 = pu32RGBADest + (u32WindowStretchXSize * (u32WindowStretchYSize - 1));
	u32Loop = u32WindowStretchXSize - (u32CornerXSize << 1);
	while (u32Loop)
	{
		UINT32 u32Chunk;
		UINT32 u32Loop2;

		u32Chunk = u32Loop;
		if (u32Chunk > u32EdgeXSize)
		{
			u32Chunk = u32EdgeXSize;
		}

		pu32RGBASrc = pu32RGBAEdgeImage;
		u32Loop2 = u32EdgeYSize;
		while (u32Loop2)
		{
			memcpy((void *) pu32RGBADest, (void *) pu32RGBASrc, sizeof(*pu32RGBADest) * u32Chunk);
			memcpy((void *) pu32RGBADest2, (void *) pu32RGBASrc, sizeof(*pu32RGBADest) * u32Chunk);
			pu32RGBADest += u32WindowStretchXSize;
			pu32RGBADest2 -= u32WindowStretchXSize;
			pu32RGBASrc += u32EdgeXSize;
			u32Loop2--;
		}

		pu32RGBADest -= (u32WindowStretchXSize * u32EdgeYSize);
		pu32RGBADest += u32Chunk;
		pu32RGBADest2 += (u32WindowStretchXSize * u32EdgeYSize);
		pu32RGBADest2 += u32Chunk;
		u32Loop -= u32Chunk;
	}

	// Left and right
	pu32RGBADest = *ppu32RGBAWindowStretchImage + (u32WindowStretchXSize * u32CornerYSize);
	pu32RGBADest2 = *ppu32RGBAWindowStretchImage + (u32WindowStretchXSize * u32CornerYSize) + (u32WindowStretchXSize - 1);
	u32Loop = u32WindowStretchYSize - (u32CornerYSize << 1);
	while (u32Loop)
	{
		UINT32 u32Chunk;
		UINT32 u32Loop2;
		UINT32 u32Loop3;

		u32Chunk = u32Loop;
		if (u32Chunk > u32EdgeXSize)
		{
			u32Chunk = u32EdgeXSize;
		}

		// u32 Chunk is the # of window relative horizontal lines we need to draw
		pu32RGBASrc = pu32RGBAEdgeImage;

		u32Loop2 = u32Chunk;
		while (u32Loop2)
		{
			u32Loop3 = u32EdgeYSize;
			while (u32Loop3)
			{
				*pu32RGBADest = *pu32RGBASrc;
				++pu32RGBADest;
				*pu32RGBADest2 = *pu32RGBASrc;
				--pu32RGBADest2;
				pu32RGBASrc += u32EdgeXSize;
				u32Loop3--;
			}

			pu32RGBADest -= u32EdgeYSize;
			pu32RGBADest += u32WindowStretchXSize;
			pu32RGBADest2 += u32EdgeYSize;
			pu32RGBADest2 += u32WindowStretchXSize;
			pu32RGBASrc-= (u32EdgeXSize * u32EdgeYSize);
			++pu32RGBASrc;

			u32Loop2--;
		}

		u32Loop -= u32Chunk;
	}


errorExit:
	return(eStatus);
}

EStatus WindowShutdown(void)
{
	EStatus eStatus = ESTATUS_OK;
	SWindowMsg sMsg;

	if( ESTATUS_OK == sg_eWindowStartupEStatus )
	{
		CLEAR_MSG_STRUCT(sMsg);
		sMsg.eMsg = EWMSG_SHUTDOWN;
		eStatus = OSQueuePut(sg_sWindowQueue,
								(void *) &sMsg,
								OS_WAIT_INDEFINITE);
		ERR_GOTO();

		eStatus = OSSemaphoreGet(sg_sWindowStartupShutdownSemaphore,
								 OS_WAIT_INDEFINITE);
		ERR_GOTO();
	}

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Failed - %s\n", GetErrorText(eStatus));
	}

	return(eStatus);
}