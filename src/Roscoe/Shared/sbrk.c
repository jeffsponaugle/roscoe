#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <assert.h>
#include <string.h>
#include "Shared/LinkerDefines.h"
#include "Hardware/Roscoe.h"
#include "Shared/sbrk.h"

// Our current top-of-heap pointer
static uint8_t *sg_pu8HeapEnd = NULL;

// Allocation alignment size (in bytes, must be powers of 2)
#define	ALLOC_ALIGNMENT_SIZE	4
#define	ALLOC_ALIGNMENT_MASK	(ALLOC_ALIGNMENT_SIZE - 1)

// Fill value of heap
#define	HEAP_FILL_VALUE			0xffffffff

caddr_t sbrk(size_t u32AllocSize) 
{
	uint8_t *pu8PreviousHeapEnd;
	uint32_t *pu32HeapPointer;

	// Round up to the next allocated size
	u32AllocSize = (u32AllocSize + ALLOC_ALIGNMENT_MASK) & ~ALLOC_ALIGNMENT_MASK;

	if (NULL == sg_pu8HeapEnd)
	{
		sg_pu8HeapEnd = (uint8_t *) &_end;
	}

	// If incoming size is 0, just return the existing pointer
	if (0 == u32AllocSize)
	{
		return((caddr_t) sg_pu8HeapEnd);
	}

	pu8PreviousHeapEnd = sg_pu8HeapEnd;

	// If we're greater than or equal to our top of stack, we're out of memory
	if ((((uint32_t) sg_pu8HeapEnd) + u32AllocSize) >= *((uint32_t *) VECTOR_STACK_POINTER))
	{
		goto outOfMemory;
	}

	// If we're now greater than the current frame pointer, we're out of memory
	if ((((uint32_t) sg_pu8HeapEnd) + u32AllocSize) >= StackPointerGet())
	{
		goto outOfMemory;
	}

	// Looking good. Let's check the new block and see if it's filled with
	// HEAP_FILL_VALUE. If not, then we've probably slammed in to stack
	// even if the stack pointer isn't currently there.
	pu32HeapPointer = (uint32_t *) pu8PreviousHeapEnd;
	sg_pu8HeapEnd += u32AllocSize;
	while (pu32HeapPointer < ((uint32_t *) sg_pu8HeapEnd))
	{
		if (*pu32HeapPointer != HEAP_FILL_VALUE)
		{
			// This indicates we're growing in to the stack
			goto outOfMemory;
		}

		++pu32HeapPointer;
	}

	// New heap pointer is OK
	return((caddr_t) pu8PreviousHeapEnd);

	// We're out of memory
outOfMemory:
	errno = ENOMEM;
	return((caddr_t) - 1);
}

void SBRKHeapReset(void)
{
	// Full up memory to the stack pointer
	if (sg_pu8HeapEnd)
	{
		uint32_t u32EndAddr = StackPointerGet();

		u32EndAddr -= (sizeof(uint32_t) << 4);
		u32EndAddr -= ((uint32_t) &_end);
//		memset((void *) &_end, 0xff, u32EndAddr);
	}

	sg_pu8HeapEnd = NULL;
}

void SBRKGetInfo(uint32_t *pu32HeapEnd,
				 uint32_t *pu32LowestStack,
				 uint32_t *pu32StackPointer)
{
	if (pu32HeapEnd)
	{
		*pu32HeapEnd = (uint32_t) sg_pu8HeapEnd;
	}

	if (pu32StackPointer)
	{
		*pu32StackPointer = StackPointerGet();
	}

	if (pu32LowestStack)
	{
		uint32_t *pu32HeapPointer;
		uint32_t u32StackPointer;

		u32StackPointer = StackPointerGet();

		// Search up through the stack pointer 
		pu32HeapPointer = (uint32_t *) sg_pu8HeapEnd;
		while (((uint32_t) pu32HeapPointer) < u32StackPointer)
		{
			if (*pu32HeapPointer != HEAP_FILL_VALUE)
			{
				*pu32LowestStack = ((int32_t) pu32HeapPointer);
				break;
			}

			++pu32HeapPointer;
		}
	}
}

