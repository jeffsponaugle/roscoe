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
							  uint16_t u16FontSize,
							  EUnicode eCharStart,
							  EUnicode eCharEnd);
extern EStatus FontGetOverallStringSize(EFontHandle eFontHandle,
										uint16_t u16FontSize,
										uint32_t *pu32XSizeTotal,
										uint32_t *pu32YSizeTotal,
										char *peString);
extern EStatus FontRenderString(EFontHandle eFontHandle,
								uint16_t u16FontSize,
								uint32_t *pu32RGBA,
								uint32_t u32RGBAPitch,
								char *peString,
								uint32_t u32RGBAColor);
extern EStatus FontRenderStringClip(EFontHandle eFontHandle,
									uint16_t u16FontSize,
									char *peString,
									uint32_t u32RGBAColor,
									uint32_t *pu32RGBABufferBase,
									uint32_t u32RGBABufferPitch,
									int32_t s32XPosBuffer,
									int32_t s32YPosBuffer,
									uint32_t u32XSizeBuffer,
									uint32_t u32YSizeBuffer);
extern EStatus FontRenderCharClip(EFontHandle eFontHandle,
								  uint16_t u16FontSize,
								  EUnicode eUnicodeChar,
								  uint32_t u32RGBAColor,
								  uint32_t *pu32RGBABufferBase,
								  uint32_t u32RGBABufferPitch,
								  int32_t s32XPosBuffer,
								  int32_t s32YPosBuffer,
								  uint32_t u32XSizeBuffer,
								  uint32_t u32YSizeBuffer);
extern EStatus FontGetCharInfo(EFontHandle eFontHandle,
							   uint16_t u16FontSize,
							   EUnicode eUnicodeChar,
							   uint32_t *u32PixelXSize,
							   uint32_t *u32PixelYSize,
							   int32_t *ps32XAdvance,
							   uint32_t *pu32YAdvance);
extern EStatus FontHandleIsValid(EFontHandle eFontHandle);

#endif