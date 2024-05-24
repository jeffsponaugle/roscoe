#ifndef _FONT_H_
#define _FONT_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EFontHandle;

extern EStatus FontInit(void);
extern EStatus FontLoad(EFontHandle eFontHandle,
						char *peFilename);
extern EStatus FontCreate(EFontHandle *peFontHandle);
extern EStatus FontDestroy(EFontHandle *peFontHandle);
extern EStatus FontCacheRange(EFontHandle eFontHandle,
							  UINT16 u16FontSize,
							  EUnicode eCharStart,
							  EUnicode eCharEnd);
extern EStatus FontGetOverallStringSize(EFontHandle eFontHandle,
										UINT16 u16FontSize,
										UINT32 *pu32XSizeTotal,
										UINT32 *pu32YSizeTotal,
										char *peString);
extern EStatus FontRenderString(EFontHandle eFontHandle,
								UINT16 u16FontSize,
								UINT32 *pu32RGBA,
								UINT32 u32RGBAPitch,
								char *peString,
								UINT32 u32RGBAColor);
extern EStatus FontRenderStringClip(EFontHandle eFontHandle,
									UINT16 u16FontSize,
									char *peString,
									UINT32 u32RGBAColor,
									UINT32 *pu32RGBABufferBase,
									UINT32 u32RGBABufferPitch,
									INT32 s32XPosBuffer,
									INT32 s32YPosBuffer,
									UINT32 u32XSizeBuffer,
									UINT32 u32YSizeBuffer);
extern EStatus FontRenderCharClip(EFontHandle eFontHandle,
								  UINT16 u16FontSize,
								  EUnicode eUnicodeChar,
								  UINT32 u32RGBAColor,
								  UINT32 *pu32RGBABufferBase,
								  UINT32 u32RGBABufferPitch,
								  INT32 s32XPosBuffer,
								  INT32 s32YPosBuffer,
								  UINT32 u32XSizeBuffer,
								  UINT32 u32YSizeBuffer);
extern EStatus FontGetCharInfo(EFontHandle eFontHandle,
							   UINT16 u16FontSize,
							   EUnicode eUnicodeChar,
							   UINT32 *u32PixelXSize,
							   UINT32 *u32PixelYSize,
							   INT32 *ps32XAdvance,
							   UINT32 *pu32YAdvance);
extern EStatus FontHandleIsValid(EFontHandle eFontHandle);

#endif