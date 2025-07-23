#include "Shared/Shared.h"
#include "Shared/SharedMisc.h"
#include "Shared/Windows/UIMisc.h"
#include "UIError.h"
#include "UIImage.h"

void UIImageSet565Bitmap(HWND eDialog,
						 uint32_t u32ControlID,
						 uint32_t u32XSizeSource,
						 uint32_t u32YSize,
						 uint16_t *pu16Image)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32XSize;
	uint16_t *pu16ImageDataAllocated = NULL;
	BITMAPINFO *psBitmapInfo = NULL;
	uint32_t *pu32Bitmask;
	int s32Result;
	HDC eDeviceContext = NULL;
	HBITMAP eBitmapHandle = NULL;

	// In order to make Windows happy, all images must be 4 byte aligned, so time to clip it
	u32XSize = u32XSizeSource & ~3;

	// Allocate a bitmap info structure. Must be this way, since the pixels and colors go
	// past the end of the structure
	MEMALLOC(psBitmapInfo, sizeof(*psBitmapInfo) + (3 * sizeof(*pu32Bitmask)));

	psBitmapInfo->bmiHeader.biSize = sizeof(psBitmapInfo->bmiHeader);
	psBitmapInfo->bmiHeader.biBitCount = 16;
	psBitmapInfo->bmiHeader.biHeight = 0 - u32YSize;
	psBitmapInfo->bmiHeader.biWidth = u32XSize;
	psBitmapInfo->bmiHeader.biPlanes = 1;
	psBitmapInfo->bmiHeader.biCompression = BI_BITFIELDS;

	// Set up 565 RGB
	pu32Bitmask = (uint32_t*) &psBitmapInfo->bmiColors;
	*(pu32Bitmask + 0) = 0xf800;	// Red
	*(pu32Bitmask + 1) = 0x07e0;	// Green
	*(pu32Bitmask + 2) = 0x001f;	// Blue

	// If the image size got adjusted, then let's clip it
	if (u32XSize != u32XSizeSource)
	{
		uint32_t u32Loop;
		uint16_t *pu16Src;
		uint16_t *pu16Dest;

		MEMALLOC(pu16ImageDataAllocated, sizeof(*pu16ImageDataAllocated) * u32XSize * u32YSize);

		// This clips everything
		pu16Src = pu16Image;
		pu16Dest = pu16ImageDataAllocated;
		u32Loop = u32YSize;
		while (u32Loop--)
		{
			memcpy((void *) pu16Dest, (void *) pu16Src, u32XSize * (sizeof(*pu16Dest)));
			pu16Dest += u32XSize;
			pu16Src += u32XSizeSource;
		}

		pu16Image = pu16ImageDataAllocated;
	}

	eDeviceContext = GetDC(eDialog);
	BASSERT(eDeviceContext);
	
	// Create a DC that works for 565
	eBitmapHandle = CreateCompatibleBitmap(eDeviceContext, 
										   u32XSize, 
										   u32YSize);
	BASSERT(eBitmapHandle);

	// Now send it to the window
	s32Result = SetDIBits(eDeviceContext, 
						  eBitmapHandle, 
						  0, 
						  u32YSize, 
						  pu16Image, 
						  psBitmapInfo, 
						  DIB_RGB_COLORS);
	BASSERT(s32Result);

	SendDlgItemMessage(eDialog, 
					   u32ControlID, 
					   STM_SETIMAGE, 
					   IMAGE_BITMAP, 
					   (LPARAM) eBitmapHandle);

errorExit:
	SafeMemFree(psBitmapInfo);
	SafeMemFree(pu16ImageDataAllocated);
}
