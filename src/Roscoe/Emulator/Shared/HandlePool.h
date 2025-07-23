#ifndef _HANDLEPOOL_H_
#define _HANDLEPOOL_H_

// Generic handle we use everywhere for this handle pool management system
typedef uint64_t EHandleGeneric;

// Pool handle 
typedef struct SHandlePool
{
	char *peHandlePoolName;			// Name of pool
	uint64_t u64HandleCount;			// # Of handles in this pool
	uint64_t u64HandleDataSize;		// Size of each handle's database
	uint8_t u8PoolID;					// Single byte pool ID
	void *pvHandleDataHead;			// Head of handle data
	int64_t s64CriticalSectionOffset; // If >=0, this is the offset in the u64HandleDataSize where the critical section is
	uint64_t u64NextIndex;			// Next index we care about
	SOSCriticalSection sPoolLock;	// Lock the pool when allocating/deallocating or otherwise messing with this structure

	// Function called during allocation
	EStatus (*Allocate)(EHandleGeneric eHandleGeneric,
						void *pvHandleStructBase);
	EStatus (*Deallocate)(EHandleGeneric eHandleGeneric,
						  void *pvHandleStructBase);

	struct SHandlePool *psNextLink;
} SHandlePool;

// Orders for HandleSetLock
typedef enum
{
	EHANDLE_NONE,
	EHANDLE_LOCK,
	EHANDLE_UNLOCK
} EHandleLock;

#define	HandleAllocate(x, y, z)	HandleAllocateInternal(x, y, (void **) z, (char *) __FILE__, (uint32_t) __LINE__)
extern EStatus HandleDeallocate(SHandlePool *psHandlePool,
								EHandleGeneric *peHandle,
								EStatus eStatusIncoming);
extern EStatus HandleAllocateInternal(SHandlePool *psHandlePool,
									  EHandleGeneric *peHandle,
									  void **ppvDataPointer,
									  char *peModule,
									  uint32_t u32LineNumber);
extern EStatus HandleSetLock(SHandlePool *psHandlePool,
							 SHandlePool **ppsHandlePoolAdjusted,
							 EHandleGeneric eHandle,
							 EHandleLock eLock,
							 void **ppvDataPointer,
							 bool bValidateAllocated,
							 EStatus eStatusIncoming);
#define	HandleIsValid(pool, handle)	HandleSetLock(pool, NULL, (EHandleGeneric) handle, EHANDLE_NONE, NULL, true, ESTATUS_OK)
extern void HandleSetInvalid(EHandleGeneric *peHandle);
extern EStatus HandlePoolCreate(SHandlePool **ppsHandlePool,
								char *peHandlePoolName,
								uint64_t u64HandleCount,
								uint64_t u64HandleDataSize,
								int64_t s64CriticalSectionOffset,
								EStatus (*Allocate)(EHandleGeneric eHandleGeneric,
													void *pvHandleStructBase),
								EStatus (*Deallocate)(EHandleGeneric eHandleGeneric,
													  void *pvHandleStructBase));
extern EStatus HandlePoolInit(void);

#endif
