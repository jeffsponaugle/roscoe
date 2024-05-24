#include "Shared/Shared.h"
#include "Shared/zlib/zutil.h"

voidpf zcalloc(voidpf pvInternalData, unsigned s64Items, unsigned s64Size)
{
	return(MemAlloc(s64Items * s64Size));
}

void zcfree(voidpf pvInternalData, voidpf pvData)
{
	MemFree(pvData);
}