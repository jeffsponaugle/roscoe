#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/HandlePool.h"

// Maximum # of pools we support
#define	MAX_POOLS	50

// Critical section for all handle pools
static SOSCriticalSection sg_sHandlePoolLock;

// Handle pool IDs
static uint8_t sg_u8HandlePoolIDMax;

// Handle mask (the handle part)
#define	HANDLE_MASK	((1 << 24) - 1)

// Handles
static SHandlePool sg_sHandlePools[MAX_POOLS];
static SHandlePool *sg_psHandlePoolHead;

// Handle flags
#define	HFLAG_ALLOCATED				0x00000001

// Internal housekeeping
typedef struct SHandleHeader
{
	uint32_t u32HandleFlags;
} SHandleHeader;

// Set a handle to invalid
void HandleSetInvalid(EHandleGeneric *peHandle)
{
	if (peHandle)
	{
		*peHandle = (EHandleGeneric) 0;
	}
}

static EStatus HandleGetPool(SHandlePool *psHandlePoolHead,
							 SHandlePool **ppsHandlePoolAdjusted,
							 EHandleGeneric eHandle)
{
	EStatus eStatus;

	// If they've passed us in a NULL handle pool head, then let's go figure
	// out which pool they're talking about and look it up
	if (NULL == psHandlePoolHead)
	{
		psHandlePoolHead = sg_psHandlePoolHead;

		while (psHandlePoolHead)
		{
			if ((uint8_t) (eHandle >> 24) == psHandlePoolHead->u8PoolID)
			{
				// Found it!
				break;
			}

			psHandlePoolHead = psHandlePoolHead->psNextLink;
		}

		// If this is NULL, then we don't know what this pool is
		if (NULL == psHandlePoolHead)
		{
			eStatus = ESTATUS_HANDLE_POOL_INVALID;
			goto errorExit;
		}
	}

	// Let's see if we have a mismatch
	if ((uint8_t) (eHandle >> 24) != psHandlePoolHead->u8PoolID)
	{
		eStatus = ESTATUS_HANDLE_POOL_MISMATCH;
		goto errorExit;
	}

	if (ppsHandlePoolAdjusted)
	{
		*ppsHandlePoolAdjusted = psHandlePoolHead;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Set's a handle specific lock and/or obtain its base address
EStatus HandleSetLock(SHandlePool *psHandlePool,
					  SHandlePool **ppsHandlePoolAdjusted,
					  EHandleGeneric eHandle,
					  EHandleLock eLock,
					  void **ppvDataPointer,
					  bool bValidateAllocated,
					  EStatus eStatusIncoming)
{
	EStatus eStatus;
	SHandleHeader *psHandleHeader = NULL;
	void *pvDataPtr;

	if (0 == (eHandle & HANDLE_MASK))
	{
		// Invalid handle
		eStatus = ESTATUS_HANDLE_NOT_ALLOCATED;
		goto errorExit;
	}

	eStatus = HandleGetPool(psHandlePool,
							&psHandlePool,
							eHandle);
	ERR_GOTO();

	// Return the adjusted handle pool
	if (ppsHandlePoolAdjusted)
	{
		*ppsHandlePoolAdjusted = psHandlePool;
	}
							
	// We have a correct pool. Now let's see if the handle part is valid
	if ((eHandle & HANDLE_MASK) > psHandlePool->u64HandleCount)
	{
		// Invalid handle
		eStatus = ESTATUS_HANDLE_NOT_ALLOCATED;
		goto errorExit;
	}

	pvDataPtr = (void *) ((((uint8_t *) psHandlePool->pvHandleDataHead) + (((eHandle - 1) & HANDLE_MASK) * (psHandlePool->u64HandleDataSize + sizeof(SHandleHeader)))) + sizeof(SHandleHeader));
	psHandleHeader = (SHandleHeader *) (((uint8_t *) pvDataPtr) - sizeof(SHandleHeader));

	// See if we validate that the handle is allocated
	if (bValidateAllocated)
	{
		if (0 == (psHandleHeader->u32HandleFlags & HFLAG_ALLOCATED))
		{
			eStatus = ESTATUS_HANDLE_NOT_ALLOCATED;
			goto errorExit;
		}
	}

	if ((psHandlePool->s64CriticalSectionOffset < 0) &&
		(eLock != EHANDLE_NONE))
	{
		eStatus = ESTATUS_HANDLE_POOL_NO_CRITICAL_SECTION;
		goto errorExit;
	}

	if (EHANDLE_LOCK == eLock)
	{
//		DebugOut("Locking handle %.8x\n", (uint32_t) eHandle);
		eStatus = OSCriticalSectionEnter((SOSCriticalSection) *((SOSCriticalSection *) (((uint8_t *) pvDataPtr) + psHandlePool->s64CriticalSectionOffset)));
	}
	else
	if (EHANDLE_UNLOCK == eLock)
	{
//		DebugOut("Unlocking handle %.8x\n", (uint32_t) eHandle);
		eStatus = OSCriticalSectionLeave((SOSCriticalSection) *((SOSCriticalSection *) (((uint8_t *) pvDataPtr) + psHandlePool->s64CriticalSectionOffset)));
	}
	else
	if (EHANDLE_NONE == eLock)
	{
		// Don't do anything
	}
	else
	{
		// Invalid parameter
		BASSERT(0);
	}

	if (ppvDataPointer)
	{
		*ppvDataPointer = pvDataPtr;
	}

errorExit:
	if (eStatusIncoming != ESTATUS_OK)
	{
		eStatus = eStatusIncoming;
	}

	return(eStatus);
}


// Deallocates a handle
EStatus HandleDeallocate(SHandlePool *psHandlePoolHead,
						 EHandleGeneric *peHandle,
						 EStatus eStatusIncoming)
{
	EStatus eStatus;
	SHandleHeader *psHandleHeader = NULL;
	bool bPoolLocked = false;
	SHandlePool *psHandlePool;

	// Get the header and the pool (if we adjusted it)
	eStatus = HandleSetLock(psHandlePoolHead,
							&psHandlePool,
							*peHandle,
							EHANDLE_LOCK,
							(void **) &psHandleHeader,
							true,
							eStatusIncoming);
	ERR_GOTO();

	// Lock it the pool
	eStatus = OSCriticalSectionEnter(psHandlePool->sPoolLock);
	ERR_GOTO();
	bPoolLocked = true;

	// Back up one handle header to our actual header rather than the user data
	--psHandleHeader;

	BASSERT(psHandleHeader->u32HandleFlags & HFLAG_ALLOCATED);

	// Call the deallocation callback
	if (psHandlePool->Deallocate)
	{
		eStatus = psHandlePool->Deallocate(*peHandle,
										   (void *) (psHandleHeader + 1));
	}

	// Unlock the handle
	eStatus = HandleSetLock(psHandlePoolHead,
							&psHandlePool,
							*peHandle,
							EHANDLE_UNLOCK,
							NULL,
							false,
							eStatus);

	HandleSetInvalid(peHandle);

errorExit:
	if (bPoolLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(psHandlePool->sPoolLock);
		}
		else
		{
			(void) OSCriticalSectionLeave(psHandlePool->sPoolLock);
		}
	}

	return(eStatus);
}

// Allocate a handle
EStatus HandleAllocateInternal(SHandlePool *psHandlePool,
							   EHandleGeneric *peHandle,
							   void **ppvDataPointer,
							   char *peModule,
							   uint32_t u32LineNumber)
{
	EStatus eStatus;
	bool bPoolLocked = false;
	uint64_t u64SlotsTried = 0;
	uint64_t u64Index = 0;
	SHandleHeader *psHandleHeader = NULL;
	uint8_t *pu8Base;

	eStatus = OSCriticalSectionEnter(psHandlePool->sPoolLock);
	ERR_GOTO();
	bPoolLocked = true;

	u64Index = psHandlePool->u64NextIndex;
	pu8Base = (uint8_t *) psHandlePool->pvHandleDataHead;
	pu8Base += (u64Index * (sizeof(*psHandleHeader) + psHandlePool->u64HandleDataSize));

	while (u64SlotsTried < psHandlePool->u64HandleCount)
	{
		// See if this is allocated (or not)
		psHandleHeader = (SHandleHeader *) pu8Base;
		if (psHandleHeader->u32HandleFlags & HFLAG_ALLOCATED)
		{
			// Slot is allocated - move to the next one
			u64Index++;
			if (u64Index >= psHandlePool->u64HandleCount)
			{
				pu8Base = (uint8_t *) psHandlePool->pvHandleDataHead;
				u64Index = 0;
			}
			else
			{
				pu8Base += (sizeof(*psHandleHeader) + psHandlePool->u64HandleDataSize);
			}

			++u64SlotsTried;
		}
		else
		{
			// Slot is not allocated. Allocate it!
			*peHandle = (EHandleGeneric) ((((uint32_t) psHandlePool->u8PoolID) << 24) | (u64Index + 1));

			// Mark the handle as allocated
			psHandleHeader->u32HandleFlags |= HFLAG_ALLOCATED;
			break;
		}
	}

	// If the # of slots tried is equal to the handle count, error out - we have no
	// room.
	if (u64SlotsTried == psHandlePool->u64HandleCount)
	{
		eStatus = ESTATUS_NO_MORE_HANDLES;
		goto errorExit;
	}

	// All good! Advance to the next index, whatever it is
	psHandlePool->u64NextIndex = u64Index + 1;
	if (psHandlePool->u64NextIndex >= psHandlePool->u64HandleCount)
	{
		psHandlePool->u64NextIndex = 0;
	}

	// Lock the new allocation 
	eStatus = OSCriticalSectionEnter((SOSCriticalSection) *((SOSCriticalSection *) ((pu8Base + sizeof(*psHandleHeader)) + psHandlePool->s64CriticalSectionOffset)));

	// Pass back the structure's base address if they want it
	if (ppvDataPointer)
	{
		*ppvDataPointer = (void *) (pu8Base + sizeof(*psHandleHeader));
	}

	// Call the allocation callback
	if (psHandlePool->Allocate)
	{
		eStatus = psHandlePool->Allocate(*peHandle,
										 (void *) (pu8Base + sizeof(*psHandleHeader)));
	}

errorExit:
	if (bPoolLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(psHandlePool->sPoolLock);
		}
		else
		{
			(void) OSCriticalSectionLeave(psHandlePool->sPoolLock);
		}
	}

	return(eStatus);
}

// Creates a handle pool
EStatus HandlePoolCreate(SHandlePool **ppsHandlePool,
						 char *peHandlePoolName,
						 uint64_t u64HandleCount,
						 uint64_t u64HandleDataSize,
						 int64_t s64CriticalSectionOffset,
						 EStatus (*Allocate)(EHandleGeneric eHandleGeneric,
											 void *pvHandleStructBase),
						 EStatus (*Deallocate)(EHandleGeneric eHandleGeneric,
											   void *pvHandleStructBase))
{
	EStatus eStatus;
	uint32_t u32Loop;

	// 4 Byte align the data block
	u64HandleDataSize = (u64HandleDataSize + 3) & ~3;

	// Lock the handle pool
	eStatus = OSCriticalSectionEnter(sg_sHandlePoolLock);
	if (eStatus != ESTATUS_OK)
	{
		return(eStatus);
	}

	// Find an empty pool handle
	for (u32Loop = 0; u32Loop < MAX_POOLS; u32Loop++)
	{
		if (NULL == sg_sHandlePools[u32Loop].peHandlePoolName)
		{
			break;
		}
	}

	if (MAX_POOLS == u32Loop)
	{
		// Out of handle types
		eStatus = ESTATUS_NO_MORE_HANDLE_TYPES;
		goto errorExit;
	}

	// Our new handle pool!
	*ppsHandlePool = &sg_sHandlePools[u32Loop];

	// Now the data
	MEMALLOC((*ppsHandlePool)->pvHandleDataHead, u64HandleCount * u64HandleDataSize * sizeof(SHandleHeader));

	// Fill in the goodies
	(*ppsHandlePool)->peHandlePoolName = strdupHeap(peHandlePoolName);
	if (NULL == (*ppsHandlePool)->peHandlePoolName)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	(*ppsHandlePool)->u8PoolID = sg_u8HandlePoolIDMax;
	++sg_u8HandlePoolIDMax;

	(*ppsHandlePool)->u64HandleCount = u64HandleCount;
	(*ppsHandlePool)->u64HandleDataSize = u64HandleDataSize;
	(*ppsHandlePool)->s64CriticalSectionOffset = s64CriticalSectionOffset;

	// Connection up the allocator and deallocators
	(*ppsHandlePool)->Allocate = Allocate;
	(*ppsHandlePool)->Deallocate = Deallocate;

	// If this is >=0, then we need to init a critical section for this item
	if (s64CriticalSectionOffset >= 0)
	{
		uint64_t u64Loop;
		uint8_t *pu8Base = ((uint8_t *) (*ppsHandlePool)->pvHandleDataHead) + s64CriticalSectionOffset + sizeof(SHandleHeader);

		// Init a crapload of critical sections
		for (u64Loop = 0; u64Loop < u64HandleCount; u64Loop++)
		{
			eStatus = OSCriticalSectionCreate((SOSCriticalSection *) pu8Base);
			ERR_GOTO();

			// Next structure
			pu8Base += u64HandleDataSize + sizeof(SHandleHeader);
		}
	}

	// And the pool lock
	eStatus = OSCriticalSectionCreate(&(*ppsHandlePool)->sPoolLock);
	ERR_GOTO();

	// Add it to our linked list
	(*ppsHandlePool)->psNextLink = sg_psHandlePoolHead;
	sg_psHandlePoolHead = (*ppsHandlePool);

	// All good!
	eStatus = ESTATUS_OK;

	DebugOut("%s: Handle pool '%s' successfully created\n", __FUNCTION__, peHandlePoolName);

errorExit:
	if (ESTATUS_OK == eStatus)
	{
		eStatus = OSCriticalSectionLeave(sg_sHandlePoolLock);
	}
	else
	{
		// Get rid of the pool if it didn't create properly
		if (*ppsHandlePool)
		{
			SafeMemFree((*ppsHandlePool)->pvHandleDataHead);
			SafeMemFree(*ppsHandlePool);
		}

		(void) OSCriticalSectionLeave(sg_sHandlePoolLock);
	}

	return(eStatus);
}

// Init the handle pool
EStatus HandlePoolInit(void)
{
	EStatus eStatus;

	// Create handle pool critical section
	eStatus = OSCriticalSectionCreate(&sg_sHandlePoolLock);
	ERR_GOTO();

	// Handle pool IDs should start at 1
	sg_u8HandlePoolIDMax = 1;

	// All good
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}
