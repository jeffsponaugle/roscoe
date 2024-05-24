#include "Shared/Shared.h"
#include "Shared/HandlePool.h"
#include "Shared/Graphics/Font.h"
#include "Shared/freetype2/include/ft2build.h"
#include "Shared/freetype2/include/freetype/ftglyph.h"
#include "Shared/Graphics/Graphics.h"

// A "font", in this context, is defined as both a typeface AND a size.

// Freetype fixed point math (see Freertype docs)
#define	FT_FP_SHIFT		6							// 6 Bits of decimal 26.6
#define	FT_LINEAR_SHIFT	16							// Linear shift 16.16
#define	FT_FP_MASK		((1 << FT_FP_SHIFT) - 1)	// Fixed point mask (fractional part)

// # Of total fonts 
#define	FONT_POOL_COUNT		1024

// Pool of font handles
static SHandlePool *sg_psFontPool;

// Freefont library global
static FT_Library sg_sFreefontLibrary;

// There is a linked list of font files - loaded in memory and cached once and forever
// until they are destroyed.
//
// Each font file has a linked list of font sizes. As with font files, they are cached
// until they are destroyed.
//
// Each font size has a linked list of encountered characters. They are rendered as they
// are encountered. Prerendering all fonts would be a memory hog, hence why they are
// done individually.

// Font character structure. Terminology is taken directly from this page:
//
// https://freetype.org/freetype2/docs/tutorial/step2.html

typedef struct SFontChar
{
	EUnicode eUnicodeChar;			// Unicode equivalent for this character

	// Malloc'd pixel font char data
	uint8_t *pu8FontPixelData;

	// Overall character X/Y size
	uint32_t u32PixelXSize;
	uint32_t u32PixelYSize;

	// Character advance (in Freetype fixed point format)
	uint32_t u32AdvanceX;
	uint32_t u32AdvanceY;

	// Horizontal X/Y bearing
	FT_Pos eHorizontalBearingX;
	FT_Pos eHorizontalBearingY;

	struct SFontChar *psNextLink;
} SFontChar;

// Font file structure
typedef struct SFontFile
{
	char *peFilename;				// Name of font file
	FT_Face sTypeface;				// FT_Face structure

	// The font file itself
	uint8_t *pu8FontFile;				// Font file contents itself
	uint64_t u64FontFileSize;			// How big is the font file?

	// Next pointer
	struct SFontFile *psNextLink;
} SFontFile;

// Font file size structure
typedef struct SFontFileSize
{
	SFontFile *psFontFile;			// Pointer to font file
	SFontChar *psFontChars;			// Characters we've rendered so far

	// Maximum Y size for all characters rendered so far
	uint32_t u32PixelYSizeMax;

	struct SFontFileSize *psNextLink;
} SFontFileSize;

// Font size structure
typedef struct SFontSize
{
	uint16_t u16FontSize;				// Size of this font	

	SFontFileSize *psFontFileSize;	// Pointer to the list of font files for this size

	struct SFontSize *psNextLink;
} SFontSize;


// Font structure
typedef struct SFont
{
	EFontHandle eFontHandle;		// This font's handle

	// Font files
	SFontFile *psFontFiles;

	// Font sizes
	SFontSize *psFontSizes;			// Linked list of font sizes

	SOSCriticalSection sFontLock;	// Critical section lock for this font
	struct SFont *psNextLink;
} SFont;

// Font list
static SFont *sg_psFontListHead = NULL;
static SOSCriticalSection sg_sFontListLock;

// Translation of FreeType errors to EStatus equivalents
typedef struct SFreeTypeToEStatus
{
	FT_Error eFreetypeError;
	EStatus eStatus;
} SFreeTypeToEStatus;

static const SFreeTypeToEStatus sg_sFreeTypeToEStatus[] =
{
	{FT_Err_Ok,								ESTATUS_OK},
	{FT_Err_Invalid_Argument,				ESTATUS_FREETYPE_INVALID_ARGUMENT},
	{FT_Err_Cannot_Open_Resource,			ESTATUS_FREETYPE_CANT_OPEN_RESOURCE},
	{FT_Err_Unknown_File_Format,			ESTATUS_FREETYPE_UNKNOWN_FILE_FORMAT},
	{FT_Err_Invalid_File_Format,			ESTATUS_FREETYPE_INVALID_FILE_FORMAT},
	{FT_Err_Invalid_Version,				ESTATUS_FREETYPE_INVALID_VERSION},
	{FT_Err_Lower_Module_Version,			ESTATUS_FREETYPE_LOWER_MODULE_VERSION},
	{FT_Err_Invalid_Argument,				ESTATUS_FREETYPE_INVALID_ARGUMENT},
	{FT_Err_Unimplemented_Feature,			ESTATUS_FREETYPE_UNIMPLEMENTED_FEATURE},
	{FT_Err_Invalid_Table,					ESTATUS_FREETYPE_INVALID_TABLE},
	{FT_Err_Invalid_Offset,					ESTATUS_FREETYPE_INVALID_OFFSET},
	{FT_Err_Array_Too_Large,				ESTATUS_FREETYPE_ARRAY_TOO_LARGE},

	{FT_Err_Invalid_Glyph_Index,			ESTATUS_FREETYPE_INVALID_GLYPH_INDEX},
	{FT_Err_Invalid_Character_Code,			ESTATUS_FREETYPE_INVALID_CHARACTER_CODE},
	{FT_Err_Invalid_Glyph_Format,			ESTATUS_FREETYPE_INVALID_GLYPH_FORMAT},
	{FT_Err_Cannot_Render_Glyph,			ESTATUS_FREETYPE_CANNOT_RENDER_GLYPH},
	{FT_Err_Invalid_Outline,				ESTATUS_FREETYPE_INVALID_OUTLINE},
	{FT_Err_Invalid_Composite,				ESTATUS_FREETYPE_INVALID_COMPOSITE},
	{FT_Err_Too_Many_Hints,					ESTATUS_FREETYPE_TOO_MANY_HINTS},
	{FT_Err_Invalid_Pixel_Size,				ESTATUS_FREETYPE_INVALID_PIXEL_SIZE},

	{FT_Err_Invalid_Handle,					ESTATUS_FREETYPE_INVALID_HANDLE},
	{FT_Err_Invalid_Library_Handle,			ESTATUS_FREETYPE_INVALID_LIBRARY_HANDLE},
	{FT_Err_Invalid_Driver_Handle,			ESTATUS_FREETYPE_INVALID_DRIVER_HANDLE},
	{FT_Err_Invalid_Face_Handle,			ESTATUS_FREETYPE_INVALID_FACE_HANDLE},
	{FT_Err_Invalid_Size_Handle,			ESTATUS_FREETYPE_INVALID_SIZE_HANDLE},
	{FT_Err_Invalid_Slot_Handle,			ESTATUS_FREETYPE_INVALID_SLOT_HANDLE},
	{FT_Err_Invalid_CharMap_Handle,			ESTATUS_FREETYPE_INVALID_CHARMAP_HANDLE},
	{FT_Err_Invalid_Cache_Handle,			ESTATUS_FREETYPE_INVALID_CACHE_HANDLE},
	{FT_Err_Invalid_Stream_Handle,			ESTATUS_FREETYPE_INVALID_STREAM_HANDLE},

	{FT_Err_Too_Many_Drivers,				ESTATUS_FREETYPE_TOO_MANY_DRIVERS},
	{FT_Err_Too_Many_Extensions,			ESTATUS_FREETYPE_TOO_MANY_EXTENSIONS},

	{FT_Err_Out_Of_Memory,					ESTATUS_OUT_OF_MEMORY},
	{FT_Err_Unlisted_Object,				ESTATUS_FREETYPE_UNLISTED_OBJECT},

	{FT_Err_Cannot_Open_Stream,				ESTATUS_FREETYPE_CANNOT_OPEN_STREAM},
	{FT_Err_Invalid_Stream_Seek,			ESTATUS_FREETYPE_INVALID_STREAM_SEEK},
	{FT_Err_Invalid_Stream_Skip,			ESTATUS_FREETYPE_INVALID_STREAM_SKIP},
	{FT_Err_Invalid_Stream_Read,			ESTATUS_FREETYPE_INVALID_STREAM_READ},
	{FT_Err_Invalid_Stream_Operation,		ESTATUS_FREETYPE_INVALID_STREAM_OPERATION},
	{FT_Err_Invalid_Frame_Operation,		ESTATUS_FREETYPE_INVALID_FRAME_OPERATION},
	{FT_Err_Nested_Frame_Access,			ESTATUS_FREETYPE_NESTED_FRAME_ACCESS},
	{FT_Err_Invalid_Frame_Read,				ESTATUS_FREETYPE_INVALID_FRAME_READ},

	{FT_Err_Raster_Uninitialized,			ESTATUS_FREETYPE_RASTER_UNINITIALIZED},
	{FT_Err_Raster_Corrupted,				ESTATUS_FREETYPE_RASTER_CORRUPTED},
	{FT_Err_Raster_Overflow,				ESTATUS_FREETYPE_RASTER_OVERFLOW},
	{FT_Err_Raster_Negative_Height,			ESTATUS_FREETYPE_RASTER_NEGATIVE_HEIGHT},

	{FT_Err_Too_Many_Caches,				ESTATUS_FREETYPE_TOO_MANY_CACHES},

	{FT_Err_Invalid_Opcode,					ESTATUS_FREETYPE_INVALID_OPCODE},
	{FT_Err_Too_Few_Arguments,				ESTATUS_FREETYPE_TOO_FEW_ARGUMENTS},
	{FT_Err_Stack_Overflow,					ESTATUS_FREETYPE_STACK_OVERFLOW},
	{FT_Err_Code_Overflow,					ESTATUS_FREETYPE_CODE_OVERFLOW},
	{FT_Err_Bad_Argument,					ESTATUS_FREETYPE_BAD_ARGUMENT},
	{FT_Err_Divide_By_Zero,					ESTATUS_FREETYPE_DIVIDE_BY_ZERO},
	{FT_Err_Invalid_Reference,				ESTATUS_FREETYPE_INVALID_REFERENCE},
	{FT_Err_Debug_OpCode,					ESTATUS_FREETYPE_DEBUG_OPCODE},
	{FT_Err_ENDF_In_Exec_Stream,			ESTATUS_FREETYPE_ENDF_IN_EXEC_STREAM},
	{FT_Err_Nested_DEFS,					ESTATUS_FREETYPE_NESTED_DEFS},
	{FT_Err_Invalid_CodeRange,				ESTATUS_FREETYPE_INVALID_CODERANGE},
	{FT_Err_Execution_Too_Long,				ESTATUS_FREETYPE_EXECUTION_TOO_LONG},
	{FT_Err_Too_Many_Function_Defs,			ESTATUS_FREETYPE_TOO_MANY_FUNCTION_DEFS},
	{FT_Err_Too_Many_Instruction_Defs,		ESTATUS_FREETYPE_TOO_MANY_INSTRUCTION_DEFS},
	{FT_Err_Table_Missing,					ESTATUS_FREETYPE_TABLE_MISSING},
	{FT_Err_Horiz_Header_Missing,			ESTATUS_FREETYPE_HORIZ_HEADER_MISSING},
	{FT_Err_Locations_Missing,				ESTATUS_FREETYPE_LOCATIONS_MISSING},
	{FT_Err_Name_Table_Missing,				ESTATUS_FREETYPE_NAME_TABLE_MISSING},
	{FT_Err_CMap_Table_Missing,				ESTATUS_FREETYPE_CMAP_TABLE_MISSING},
	{FT_Err_Hmtx_Table_Missing,				ESTATUS_FREETYPE_HMTX_TABLE_MISSING},
	{FT_Err_Post_Table_Missing,				ESTATUS_FREETYPE_POST_TABLE_MISSING},
	{FT_Err_Invalid_Horiz_Metrics,			ESTATUS_FREETYPE_INVALID_HORIZ_METRICS},
	{FT_Err_Invalid_CharMap_Format,			ESTATUS_FREETYPE_INVALID_CHARMAP_FORMAT},
	{FT_Err_Invalid_PPem,					ESTATUS_FREETYPE_INVALID_PPEM},
	{FT_Err_Invalid_Vert_Metrics,			ESTATUS_FREETYPE_INVALID_VER_METRICS},
	{FT_Err_Could_Not_Find_Context,			ESTATUS_FREETYPE_COULD_NOT_FOUND_CONTEXT},
	{FT_Err_Invalid_Post_Table_Format,		ESTATUS_FREETYPE_INVALID_POST_TABLE_FORMAT},
	{FT_Err_Invalid_Post_Table,				ESTATUS_FREETYPE_INVALID_POST_TABLE},

	{FT_Err_Syntax_Error,					ESTATUS_FREETYPE_SYNTAX_ERROR},
	{FT_Err_Stack_Underflow,				ESTATUS_FREETYPE_STACK_UNDERFLOW},
	{FT_Err_Ignore,							ESTATUS_FREETYPE_IGNORE},

	{FT_Err_Missing_Startfont_Field,		ESTATUS_FREETYPE_MISSING_STARTFONT_FIELD},
	{FT_Err_Missing_Font_Field,				ESTATUS_FREETYPE_MISSING_FONT_FIELD},
	{FT_Err_Missing_Size_Field,				ESTATUS_FREETYPE_MISSING_SIZE_FIELD},
	{FT_Err_Missing_Chars_Field,			ESTATUS_FREETYPE_MISSING_CHARS_FIELD},
	{FT_Err_Missing_Startchar_Field,		ESTATUS_FREETYPE_MISSING_STARTCHAR_FIELD},
	{FT_Err_Missing_Encoding_Field,			ESTATUS_FREETYPE_MISSING_ENCODING_FIELD},
	{FT_Err_Missing_Bbx_Field,				ESTATUS_FREETYPE_MISSING_BBX_FIELD},
	{FT_Err_Bbx_Too_Big,					ESTATUS_FREETYPE_BBX_TOO_BIG},
	{FT_Err_Corrupted_Font_Header,			ESTATUS_FREETYPE_CORRUPTED_FONT_HEADER},
	{FT_Err_Corrupted_Font_Glyphs,			ESTATUS_FREETYPE_CORRUPTED_FONT_GLYPHS},
};

// Locks the font list
static EStatus FontListSetLock(bool bLock,
							   EStatus eStatusIncoming)
{
	EStatus eStatus;

	if (bLock)
	{
		eStatus = OSCriticalSectionEnter(sg_sFontListLock);
	}
	else
	{
		eStatus = OSCriticalSectionLeave(sg_sFontListLock);
	}

	if (eStatusIncoming != ESTATUS_OK)
	{
		eStatus = eStatusIncoming;
	}

	return(eStatus);
}

// Controls font locking
static EStatus FontSetLock(EFontHandle eFontHandle,
						   SFont **ppsFont,
						   bool bLock,
						   EStatus eStatusIncoming)
{
	bool bValidateAllocated = false;
	EHandleLock eLock = EHANDLE_LOCK;

	if (false == bLock)
	{
		eLock = EHANDLE_UNLOCK;
	}

	return(HandleSetLock(sg_psFontPool,
						 NULL,
						 (EHandleGeneric) eFontHandle,
						 eLock,
						 (void **) ppsFont,
						 true,
						 eStatusIncoming));
}

// Translate a Freetype error to an EStatus
static EStatus FeetypeToEStatus(FT_Error eFTErr)
{
	uint32_t u32Loop;

	for (u32Loop = 0; u32Loop < (sizeof(sg_sFreeTypeToEStatus) / sizeof(sg_sFreeTypeToEStatus[0])); u32Loop++)
	{
		if (sg_sFreeTypeToEStatus[u32Loop].eFreetypeError == eFTErr)
		{
			return(sg_sFreeTypeToEStatus[u32Loop].eStatus);
		}
	}

	return(ESTATUS_FREETYPE_UNKNOWN_ERROR);
}

// Destroy all rendered characters
static void FontCharsDestroy(SFontChar **ppsFontCharHead)
{
	while (*ppsFontCharHead)
	{
		SFontChar *psFontChar;

		psFontChar = *ppsFontCharHead;
		SafeMemFree(psFontChar->pu8FontPixelData);
		*ppsFontCharHead = (*ppsFontCharHead)->psNextLink;
		SafeMemFree(psFontChar);
	}
}

// Destroy all font file sizes
static void FontFileSizeDestroy(SFontFileSize **ppsFontFileSizeHead)
{
	while (*ppsFontFileSizeHead)
	{
		SFontFileSize *psFontFileSize;

		FontCharsDestroy(&(*ppsFontFileSizeHead)->psFontChars);
		psFontFileSize = *ppsFontFileSizeHead;
		*ppsFontFileSizeHead = (*ppsFontFileSizeHead)->psNextLink;
		SafeMemFree(psFontFileSize);
	}
}

// Destroy all font sizes and each rendered character
static void FontSizeDestroy(SFontSize **ppsFontSizeHead)
{
	while (*ppsFontSizeHead)
	{
		SFontSize *psFontSize;

		psFontSize = (*ppsFontSizeHead)->psNextLink;
		*ppsFontSizeHead = (*ppsFontSizeHead)->psNextLink;
		FontFileSizeDestroy(&psFontSize->psFontFileSize);
		SafeMemFree(psFontSize);
	}
}

// Called every time a font handle is allocated. Handle is not locked, but the
// handle list is, effectively locking everything.
static EStatus FontHandleAllocate(EHandleGeneric eFontHandle,
								  void *pvHandleStructBase)
{
	SFont *psFont = (SFont *) pvHandleStructBase;

	// And make a copy of the font handle
	psFont->eFontHandle = (EFontHandle) eFontHandle;

	return(ESTATUS_OK);
}

// Called every time a font handle is deallocated. Handle is locked.
static EStatus FontHandleDeallocate(EHandleGeneric eFontHandle,
									void *pvHandleStructBase)
{
	EStatus eStatus;
	SFont *psFont = (SFont *) pvHandleStructBase;
	SFontFile *psFontFile;

	while (psFont->psFontFiles)
	{
		psFontFile = psFont->psFontFiles;
		SafeMemFree(psFontFile->peFilename);
		SafeMemFree(psFontFile->pu8FontFile);
		// Get rid of the typeface
		eStatus = FeetypeToEStatus(FT_Done_Face(psFontFile->sTypeface));
		psFontFile->sTypeface = NULL;
		psFontFile->u64FontFileSize = 0;
		psFont->psFontFiles = psFont->psFontFiles->psNextLink;
		SafeMemFree(psFontFile);
	}

	// Go destroy all the sizes and chars
	FontSizeDestroy(&psFont->psFontSizes);


	// Set the handle invalid
	HandleSetInvalid(&psFont->eFontHandle);

	return(eStatus);
}


// Will destroy a font - doesn't do anything with the linked list of fonts. Note the font
// itself is locked
EStatus FontDestroy(EFontHandle *peFontHandle)
{
	return(HandleDeallocate(sg_psFontPool,
							(EHandleGeneric *) peFontHandle,
							ESTATUS_OK));
}

// This routine will load a font character from a given font and create an SFontChar
// structure and return it. Information on how to do it was obtained here:
//
// https://freetype.org/freetype2/docs/tutorial/step1.html
// https://freetype.org/freetype2/docs/tutorial/step2.html

static EStatus FontLoadChar(SFont *psFont,
							EUnicode eCharacter,
							uint16_t u16FontSize,
							SFontSize *psFontSize,
							SFontFileSize **ppsFontFileSize,
							SFontChar **ppsFontChar)
{
	FT_UInt eCharIndex;
	EStatus eStatus = ESTATUS_OK;
	SFontFile *psFontFile;
	SFontFileSize *psFontFileSize;

	psFontFileSize = psFontSize->psFontFileSize;

	while (psFontFileSize)
	{
		// Figure out what this Unicode character's index is
		eCharIndex = FT_Get_Char_Index(psFontFileSize->psFontFile->sTypeface,
									   (FT_ULong) eCharacter);
		if (0 == eCharIndex)
		{
			// This means it couldn't find the character in this font file - move to the next
			psFontFileSize = psFontFileSize->psNextLink;
		}
		else
		{
			break;
		}
	}

	// If we didn't find a valid character in any of our font files, then return an invalid
	// character code
	if (NULL == psFontFileSize)
	{
		eStatus = ESTATUS_FREETYPE_INVALID_CHARACTER_CODE;
		goto errorExit;
	}

	// If we want to know what file size structure has this character, return it
	if (ppsFontFileSize)
	{
		*ppsFontFileSize = psFontFileSize;
	}

	psFontFile = psFontFileSize->psFontFile;

	// Set the font size - width is variable, height is constant
	eStatus = FeetypeToEStatus(FT_Set_Pixel_Sizes(psFontFile->sTypeface,
												  0,	
												  (FT_UInt) u16FontSize));
	ERR_GOTO();

	// First charmap (Unicode) gets selected
	eStatus = FeetypeToEStatus(FT_Select_Charmap(psFontFile->sTypeface,
												 psFontFile->sTypeface->charmaps[0]->encoding));
	ERR_GOTO();

	// Now load up the glyph.
	eStatus = FeetypeToEStatus(FT_Load_Glyph(psFontFile->sTypeface,
											 eCharIndex,
											 FT_LOAD_DEFAULT));
	ERR_GOTO();

	// And render the glyph
	eStatus = FeetypeToEStatus(FT_Render_Glyph(psFontFile->sTypeface->glyph,
											   FT_RENDER_MODE_NORMAL));
	ERR_GOTO();

	// Now create a place to put it
	MEMALLOC(*ppsFontChar, sizeof(**ppsFontChar));

	(*ppsFontChar)->eUnicodeChar = eCharacter;

	// Get the pixel X/Y size of the overall character
	BASSERT(0 == (psFontFile->sTypeface->glyph->metrics.width & FT_FP_MASK));
	(*ppsFontChar)->u32PixelXSize = (psFontFile->sTypeface->glyph->metrics.width >> FT_FP_SHIFT);
	BASSERT(0 == (psFontFile->sTypeface->glyph->metrics.height & FT_FP_MASK));
	(*ppsFontChar)->u32PixelYSize = (psFontFile->sTypeface->glyph->metrics.height >> FT_FP_SHIFT);

	// Figure out the X/Y advance (in Freetype fixed point format)
	(*ppsFontChar)->u32AdvanceX = (uint32_t) (psFontFile->sTypeface->glyph->metrics.horiAdvance);
	(*ppsFontChar)->u32AdvanceY = (uint32_t) (psFontFile->sTypeface->glyph->metrics.vertAdvance);

	// See if this has a horizontal advance - everything is horizontal advance for us
	if (FT_HAS_HORIZONTAL(psFontFile->sTypeface))
	{
		(*ppsFontChar)->eHorizontalBearingX = psFontFile->sTypeface->glyph->metrics.horiBearingX;
		(*ppsFontChar)->eHorizontalBearingY = (psFontFile->sTypeface->glyph->linearVertAdvance >> (FT_LINEAR_SHIFT - FT_FP_SHIFT)) - (psFontFile->sTypeface->glyph->metrics.horiBearingY+(1<<FT_FP_SHIFT));
	}

	// If we have some actual bitmap data, then we need to copy it. It's just bytes (grey scaling)
	if ((*ppsFontChar)->u32PixelXSize &&
		(*ppsFontChar)->u32PixelYSize)
	{
		MEMALLOC_NO_CLEAR((*ppsFontChar)->pu8FontPixelData,
						  (*ppsFontChar)->u32PixelXSize * (*ppsFontChar)->u32PixelYSize);

		// Copy the data in
		memcpy((void *) (*ppsFontChar)->pu8FontPixelData,
			   (void *) psFontFile->sTypeface->glyph->bitmap.buffer,
			   (*ppsFontChar)->u32PixelXSize * (*ppsFontChar)->u32PixelYSize);

		// All good!
	}

errorExit:
	return(eStatus);
}

// Finds a font character and allocates a size if necessary. It assumes the font
// handle is locked.
static EStatus FontFindChar(SFont *psFont,
							uint16_t u16FontSize,
							EUnicode eCharacter,
							SFontChar **ppsFontChar,
							uint32_t *pu32MaxYTotal)
{
	EStatus eStatus;
	SFontSize *psFontSize = NULL;
	SFontChar *psFontChar = NULL;
	SFontChar *psFontCharPrevious = NULL;
	SFontFileSize *psFontFileSize = NULL;

	// Find the size
	psFontSize = psFont->psFontSizes;
	while (psFontSize)
	{
		if (u16FontSize == psFontSize->u16FontSize)
		{
			break;
		}

		psFontSize = psFontSize->psNextLink;
	}

	// If we haven't found the size, create a size structure and link it to this font
	if (NULL == psFontSize)
	{
		SFontFile *psFontFile = NULL;
		SFontFileSize *psFontFileSizePrior = NULL;
		FT_Long u32Index = 0;

		MEMALLOC(psFontSize, sizeof(*psFontSize));

		psFontSize->psNextLink = psFont->psFontSizes;
		psFont->psFontSizes = psFontSize;
		psFontSize->u16FontSize = u16FontSize;

		psFontFile = psFont->psFontFiles;

		while (psFontFile)
		{
			// Create a set of font file size structures
			if (NULL == psFontFileSizePrior)
			{
				MEMALLOC(psFontFileSize, sizeof(*psFontFileSize));
				psFontSize->psFontFileSize = psFontFileSize;
			}
			else
			{
				MEMALLOC(psFontFileSizePrior->psNextLink, sizeof(*psFontFileSizePrior->psNextLink));
				psFontFileSize = psFontFileSizePrior->psNextLink;
			}

			psFontFileSize->psFontFile = psFontFile;

			// Set the font size - width is variable, height is constant
			eStatus = FeetypeToEStatus(FT_Set_Pixel_Sizes(psFontFile->sTypeface,
															0,	
															(FT_UInt) u16FontSize));
			ERR_GOTO();

			// First charmap (Unicode) gets selected
			eStatus = FeetypeToEStatus(FT_Select_Charmap(psFontFile->sTypeface,
														 FT_ENCODING_UNICODE));
			ERR_GOTO();

			// Set the maximum pixel Y size for this font
			psFontFileSize->u32PixelYSizeMax = ((psFontFile->sTypeface->size->metrics.height + ((1 << FT_FP_SHIFT) - 1)) >> FT_FP_SHIFT);
			psFontFileSizePrior = psFontFileSize;

			psFontFile = psFontFile->psNextLink;
		}
	}

	// Got the size. Find the character.
	psFontFileSize = psFontSize->psFontFileSize;

	while (psFontFileSize)
	{
		psFontChar = psFontFileSize->psFontChars;
		while (psFontChar)
		{
			if (psFontChar->eUnicodeChar == eCharacter)
			{
				break;
			}

			psFontCharPrevious = psFontChar;
			psFontChar = psFontChar->psNextLink;
		}

		// If we've found the character in the file sizes, break out
		if (psFontChar)
		{
			break;
		}
		else
		{
			// Mvoe on to the next font file in this font handle
			psFontFileSize = psFontFileSize->psNextLink;
		}
	}

	// If this wasn't found, then we need to load it from the font file
	if (NULL == psFontChar)
	{
		eStatus = FontLoadChar(psFont,
							   eCharacter,
							   u16FontSize,
							   psFontSize,
							   &psFontFileSize,
							   &psFontChar);
		ERR_GOTO();

		if (NULL == psFontCharPrevious)
		{
			// Link it in to the head of the list for fonts of this size
			psFontChar->psNextLink = psFontFileSize->psFontChars;
			psFontFileSize->psFontChars = psFontChar;
		}
		else
		{
			// Add it to the end
			BASSERT(NULL == psFontCharPrevious->psNextLink);
			psFontCharPrevious->psNextLink = psFontChar;
		}
	}

	// psFontChar now points to the SFontChar structure
	if (ppsFontChar)
	{
		*ppsFontChar = psFontChar;
	}

	if (pu32MaxYTotal)
	{
		*pu32MaxYTotal = psFontFileSize->u32PixelYSizeMax;
	}

	// All good!
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// This routine will load up/render/cache 
EStatus FontCacheRange(EFontHandle eFontHandle,
					   uint16_t u16FontSize,
					   EUnicode eCharStart,
					   EUnicode eCharEnd)
{
	EStatus eStatus = ESTATUS_OK;
	SFont *psFont = NULL;
	bool bLocked = false;

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	// Run through all characters we want to load up/precache
	while ((eCharStart <= eCharEnd) &&
		   (ESTATUS_OK == eStatus))
	{
		eStatus = FontFindChar(psFont,
							   u16FontSize,
							   eCharStart,
							   NULL,
							   NULL);

		// If we get an invalid character code back, then just allow it, as
		// there may be gaps in the characters, which is OK.
		if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
		{
			eStatus = ESTATUS_OK;
		}

		++eCharStart;
	}


errorExit:
	if (bLocked)
	{
		// Unlock the font
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}
	return(eStatus);
}

// Returns the overall X/Y size of this string
EStatus FontGetOverallStringSize(EFontHandle eFontHandle,
								 uint16_t u16FontSize,
								 uint32_t *pu32XSizeTotal,
								 uint32_t *pu32YSizeTotal,
								 char *peString)
{
	EStatus eStatus = ESTATUS_OK;
	EUnicode eUnicodeChar;
	uint32_t u32MaxXTotal = 0;
	SFont *psFont = NULL;
	bool bLocked = false;
	SFontFile *psFontFile = NULL;

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	psFontFile = psFont->psFontFiles;
	while (psFontFile)
	{
		eStatus = FeetypeToEStatus(FT_Set_Pixel_Sizes(psFontFile->sTypeface,
													  0,	
													  (FT_UInt) u16FontSize));
		ERR_GOTO();

		psFontFile = psFontFile->psNextLink;
	}

	// Loop through all characters
	while (*peString)
	{
		SFontChar *psFontChar = NULL;

		// Get the Unicode equivalent
		eUnicodeChar = UTF8toUnicode(peString);

		// Next character
		peString += UTF8charlen(peString);

		eStatus = FontFindChar(psFont,
							   u16FontSize,
							   eUnicodeChar,
							   &psFontChar,
							   pu32YSizeTotal);

		// We can skip over invalid character codes
		if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
		{
			// Let's look up a ?
			eStatus = FontFindChar(psFont,
								   u16FontSize,
								   (EUnicode) '?',
								   &psFontChar,
								   pu32YSizeTotal);

			// Question mark should always be available
			BASSERT(ESTATUS_OK == eStatus);
		}

		ERR_GOTO();

		if (*peString)
		{
			// We have more characters - add in the advance
			u32MaxXTotal += (psFontChar->u32AdvanceX >> FT_FP_SHIFT);
		}
		else
		{
			// We don't have more characters. Add in the width of the character and horizontal bearing
			u32MaxXTotal += (psFontChar->eHorizontalBearingX >> FT_FP_SHIFT) + psFontChar->u32PixelXSize;
		}
	}

	if (pu32XSizeTotal)
	{
		*pu32XSizeTotal = u32MaxXTotal;
	}

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}

	return(eStatus);
}

// Create a greyscale->RGBA translation table
static void FontGeneratePixelTable(uint32_t u32RGBAColor,
								   uint32_t *pu32PixelTranslate,
								   uint32_t u32TranslateCount)
{
	uint32_t u32Loop;

	for (u32Loop = 0; u32Loop < u32TranslateCount; u32Loop++)
	{
		pu32PixelTranslate[u32Loop] = (((u32RGBAColor & 0xff) * u32Loop) >> 8) |
									  (((uint8_t) ((((u32RGBAColor >> 8) & 0xff)  * u32Loop) >> 8)) << 8) |
									  (((uint8_t) ((((u32RGBAColor >> 16) & 0xff) * u32Loop) >> 8)) << 16) |
									  (((uint8_t) (u32Loop)) << 24);
	}
}

EStatus FontGetCharInfo(EFontHandle eFontHandle,
						uint16_t u16FontSize,
						EUnicode eUnicodeChar,
						uint32_t *u32PixelXSize,
						uint32_t *u32PixelYSize,
						int32_t *ps32XAdvance,
						uint32_t *pu32YAdvance)
{
	EStatus eStatus = ESTATUS_OK;
	bool bLocked = false;
	SFont *psFont = NULL;
	SFontChar *psFontChar = NULL;

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	eStatus = FontFindChar(psFont,
						   u16FontSize,
						   eUnicodeChar,
						   &psFontChar,
						   pu32YAdvance);
	ERR_GOTO();

	if (u32PixelXSize)
	{
		*u32PixelXSize = psFontChar->u32PixelXSize;
	}

	if (u32PixelYSize)
	{
		*u32PixelYSize = psFontChar->u32PixelYSize;
	}

	if (ps32XAdvance)
	{
		*ps32XAdvance = ((psFontChar->u32AdvanceX + FT_FP_MASK) >> FT_FP_SHIFT);
	}

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}
	return(eStatus);
}

static void FontRenderCharClipInternal(uint32_t *pu32RGBABufferBase,
									   uint32_t u32RGBABufferPitch,
									   int32_t s32XPosBuffer,
									   int32_t s32YPosBuffer,
									   uint32_t u32XSizeBuffer,
									   uint32_t u32YSizeBuffer,
									   SFontChar *psFontChar,
									   uint32_t *pu32PixelTranslate)
{
	int32_t s32XPosAdjusted = s32XPosBuffer + (int32_t) (psFontChar->eHorizontalBearingX >> FT_FP_SHIFT);
	int32_t s32YPosAdjusted = s32YPosBuffer + (int32_t) (psFontChar->eHorizontalBearingY >> FT_FP_SHIFT);
	int32_t s32XSizeAdjusted = (int32_t) psFontChar->u32PixelXSize;
	int32_t s32YSizeAdjusted = (int32_t) psFontChar->u32PixelYSize;

	// s32X/YPosAdjusted is the adjusted X/Y coordinates on the target buffer where the
	// pixels of the font actually start.

	// Clip if off the left side
	if (s32XPosAdjusted < 0)
	{
		s32XSizeAdjusted += s32XPosAdjusted;
		s32XPosAdjusted = 0;
	}

	// And clip to the top side
	if (s32YPosAdjusted < 0)
	{
		s32YSizeAdjusted += s32YPosAdjusted;
		s32YPosAdjusted = 0;
	}

	// Clip to the right side
	if ((s32XPosAdjusted + s32XSizeAdjusted) > (int32_t) u32XSizeBuffer)
	{
		s32XSizeAdjusted = u32XSizeBuffer - s32XPosAdjusted;
	}

	// And bottom
	if ((s32YPosAdjusted + s32YSizeAdjusted) > (int32_t) u32YSizeBuffer)
	{
		s32YSizeAdjusted = u32YSizeBuffer - s32YPosAdjusted;
	}

	// Now only draw it if there's something to draw
	if ((s32XSizeAdjusted > 0) &&
		(s32YSizeAdjusted > 0))
	{
		uint8_t *pu8SrcData = psFontChar->pu8FontPixelData;
		uint32_t *pu32RGBAPtr = pu32RGBABufferBase + s32XPosAdjusted + (s32YPosAdjusted * u32RGBABufferPitch);

		if (pu8SrcData)
		{
			while (s32YSizeAdjusted)
			{
				uint32_t u32XSize;

				u32XSize = (uint32_t) s32XSizeAdjusted;
				while (u32XSize)
				{
					uint8_t u8AlphaDest;

					u8AlphaDest = (uint8_t) ((pu32PixelTranslate[*pu8SrcData] & 0xFF000000) >> 24);

					if (PIXEL_TRANSPARENT == u8AlphaDest)
					{
						// Don't do anything - completely transparent
					}
					else
					if (PIXEL_OPAQUE == u8AlphaDest)
					{
						// Completely solid
						*pu32RGBAPtr = pu32PixelTranslate[*pu8SrcData];
					}
					else
					{
						// Gotta combine
						uint32_t u32RB1;
						uint32_t u32RB2;
						uint32_t u32G1;
						uint32_t u32G2;
						uint16_t u16AlphaNew;
						uint8_t u8AlphaSrc;
						uint32_t u32InvAlpha;

						u8AlphaSrc = (uint8_t) ((*pu32RGBAPtr & 0xFF000000) >> 24);
						u32InvAlpha = (0x100 - u8AlphaDest);
						u32RB1 = (u32InvAlpha * (*pu32RGBAPtr & 0xFF00FF)) >> 8;
						u32RB2 = (u8AlphaDest * (pu32PixelTranslate[*pu8SrcData] & 0xFF00FF)) >> 8;
						u32G1 = (u32InvAlpha * (*pu32RGBAPtr & 0x00FF00)) >> 8;
						u32G2 = (u8AlphaDest * (pu32PixelTranslate[*pu8SrcData] & 0x00FF00)) >> 8;
						u16AlphaNew = u8AlphaSrc + u8AlphaDest;

						if (u16AlphaNew > 255)
						{
							u16AlphaNew = 255;
						}

						*pu32RGBAPtr = ((u32RB1 + u32RB2) & 0xFF00FF) + ((u32G1 + u32G2) & 0x00FF00) + (u16AlphaNew << 24);
					}

					++pu8SrcData;
					++pu32RGBAPtr;
					u32XSize--;
				}

				// Adjust target pointer
				pu32RGBAPtr += (u32RGBABufferPitch - s32XSizeAdjusted);

				// Adjust source pixel pointer
				pu8SrcData += (psFontChar->u32PixelXSize - s32XSizeAdjusted);
				s32YSizeAdjusted--;
			}
		}
	}
}

EStatus FontRenderString(EFontHandle eFontHandle,
						 uint16_t u16FontSize,
						 uint32_t *pu32RGBA,
						 uint32_t u32RGBAPitch,
						 char *peString,
						 uint32_t u32RGBAColor)
{
	EStatus eStatus = ESTATUS_OK;
	EUnicode eUnicodeChar;
	uint32_t u32MaxXTotal = 0;
	uint32_t u32MaxYTotal = 0;
	SFont *psFont = NULL;
	bool bLocked = false;
	uint32_t u32PixelTranslate[0x100];

	if (NULL == peString)
	{
		goto errorExit;
	}

	FontGeneratePixelTable(u32RGBAColor,
						   u32PixelTranslate,
						   sizeof(u32PixelTranslate) / sizeof(u32PixelTranslate[0]));

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	// Loop through all characters
	while (*peString)
	{
		SFontChar *psFontChar = NULL;

		// Get the Unicode equivalent
		eUnicodeChar = UTF8toUnicode(peString);

		eStatus = FontFindChar(psFont,
							   u16FontSize,
							   eUnicodeChar,
							   &psFontChar,
							   NULL);

		// Replace any invalid cahracters with ?
		if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
		{
			// Let's look up a ?
			eStatus = FontFindChar(psFont,
								   u16FontSize,
								   (EUnicode) '?',
								   &psFontChar,
								   NULL);

			// Question mark should always be available
			BASSERT(ESTATUS_OK == eStatus);
		}

		// We can skip over invalid character codes
		if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
		{
			// Fall through
		}
		else
		if (ESTATUS_OK == eStatus)
		{
			uint32_t u32YSize = psFontChar->u32PixelYSize;
			uint8_t *pu8SrcData = psFontChar->pu8FontPixelData;
			uint32_t *pu32RGBAPtr = pu32RGBA;

			if (pu8SrcData)
			{
				pu32RGBAPtr += (u32RGBAPitch * (psFontChar->eHorizontalBearingY >> FT_FP_SHIFT)) +
								(psFontChar->eHorizontalBearingX >> FT_FP_SHIFT);
				while (u32YSize)
				{
					uint32_t u32XSize;
				
					u32XSize =  psFontChar->u32PixelXSize;
					while (u32XSize)
					{
						*pu32RGBAPtr = u32PixelTranslate[*pu8SrcData];
						++pu32RGBAPtr;
						++pu8SrcData;

						u32XSize--;
					}

					// Next row of pixels
					pu32RGBAPtr += (u32RGBAPitch - psFontChar->u32PixelXSize);
					u32YSize--;
				}
			}

			// Move on to the next character of pixels
			pu32RGBA += ((psFontChar->u32AdvanceX + FT_FP_MASK) >> FT_FP_SHIFT);
		}
		else
		{
			ERR_GOTO();
		}

		// Next character
		peString += UTF8charlen(peString);
	}

	eStatus = ESTATUS_OK;

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}

	return(eStatus);
}

// Render text to a buffer (with clipping)
EStatus FontRenderStringClip(EFontHandle eFontHandle,
							 uint16_t u16FontSize,
							 char *peString,
							 uint32_t u32RGBAColor,
							 uint32_t *pu32RGBABufferBase,
							 uint32_t u32RGBABufferPitch,
							 int32_t s32XPosBuffer,
							 int32_t s32YPosBuffer,
							 uint32_t u32XSizeBuffer,
							 uint32_t u32YSizeBuffer)
{
	EStatus eStatus = ESTATUS_OK;
	EUnicode eUnicodeChar;
	uint32_t u32MaxXTotal = 0;
	uint32_t u32MaxYTotal = 0;
	SFont *psFont = NULL;
	bool bLocked = false;
	uint32_t u32PixelTranslate[0x100];

	// Just return if we have nothing to render
	if (NULL == peString)
	{
		goto errorExit;
	}

	FontGeneratePixelTable(u32RGBAColor,
						   u32PixelTranslate,
						   sizeof(u32PixelTranslate) / sizeof(u32PixelTranslate[0]));

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	// Loop through all characters
	while (*peString)
	{
		SFontChar *psFontChar = NULL;

		// Get the Unicode equivalent
		eUnicodeChar = UTF8toUnicode(peString);

		eStatus = FontFindChar(psFont,
							   u16FontSize,
							   eUnicodeChar,
							   &psFontChar,
							   NULL);

		// We can skip over invalid character codes
		if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
		{
			// Fall through
		}
		else
		if (ESTATUS_OK == eStatus)
		{
			FontRenderCharClipInternal(pu32RGBABufferBase,
									   u32RGBABufferPitch,
									   s32XPosBuffer,
									   s32YPosBuffer,
									   u32XSizeBuffer,
									   u32YSizeBuffer,
									   psFontChar,
									   u32PixelTranslate);

			// Move to the next character
			s32XPosBuffer += ((psFontChar->u32AdvanceX + FT_FP_MASK) >> FT_FP_SHIFT);
		}
		else
		{
			ERR_GOTO();
		}

		// Next character
		peString += UTF8charlen(peString);
	}

	eStatus = ESTATUS_OK;

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}

	return(eStatus);
}

EStatus FontRenderCharClip(EFontHandle eFontHandle,
						   uint16_t u16FontSize,
						   EUnicode eUnicodeChar,
						   uint32_t u32RGBAColor,
						   uint32_t *pu32RGBABufferBase,
						   uint32_t u32RGBABufferPitch,
						   int32_t s32XPosBuffer,
						   int32_t s32YPosBuffer,
						   uint32_t u32XSizeBuffer,
						   uint32_t u32YSizeBuffer)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32MaxXTotal = 0;
	uint32_t u32MaxYTotal = 0;
	SFont *psFont = NULL;
	bool bLocked = false;
	uint32_t u32PixelTranslate[0x100];
	SFontChar *psFontChar = NULL;

	FontGeneratePixelTable(u32RGBAColor,
						   u32PixelTranslate,
						   sizeof(u32PixelTranslate) / sizeof(u32PixelTranslate[0]));

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	eStatus = FontFindChar(psFont,
							u16FontSize,
							eUnicodeChar,
							&psFontChar,
							NULL);

	// We can skip over invalid character codes
	if (ESTATUS_FREETYPE_INVALID_CHARACTER_CODE == eStatus)
	{
		// Fall through
	}
	else
	if (ESTATUS_OK == eStatus)
	{
		FontRenderCharClipInternal(pu32RGBABufferBase,
								   u32RGBABufferPitch,
								   s32XPosBuffer,
								   s32YPosBuffer,
								   u32XSizeBuffer,
								   u32YSizeBuffer,
								   psFontChar,
								   u32PixelTranslate);
	}

	eStatus = ESTATUS_OK;

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}

	return(eStatus);
}

// Load a font into this font file handle
EStatus FontLoad(EFontHandle eFontHandle,
				 char *peFilename)
{
	EStatus eStatus;
	uint8_t *pu8FontFile = NULL;
	uint64_t u64FontFileSize;
	SFontFile *psFontFile = NULL;
	SFontFile *psFontFilePtr = NULL;
	FT_Error eFTError;
	SFont *psFont = NULL;
	bool bLocked = false;

	// Load up the font file
	eStatus = FileLoad(peFilename,
					   &pu8FontFile,
					   &u64FontFileSize,
					   0);
	ERR_GOTO();

	// It loaded!
	MEMALLOC(psFontFile, sizeof(*psFontFile));

	// Now duplicate the filename
	psFontFile->peFilename = strdupHeap(peFilename);
	if (NULL == psFontFile->peFilename)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	psFontFile->pu8FontFile = pu8FontFile;
	psFontFile->u64FontFileSize = u64FontFileSize;

	// Clear these out so they don't get freed later
	pu8FontFile = NULL;
	u64FontFileSize = 0;

	// Font file loaded - init a memory based typeface
	eFTError = FT_New_Memory_Face(sg_sFreefontLibrary,
								  (FT_Byte *) psFontFile->pu8FontFile,
								  (FT_Long) psFontFile->u64FontFileSize,
								  0,
								  &psFontFile->sTypeface);
	eStatus = FeetypeToEStatus(eFTError);
	ERR_GOTO();

	// Lock this font while we mess with it
	eStatus = FontSetLock(eFontHandle,
						  &psFont,
						  true,
						  ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	// Link this file into the end of the list
	psFontFilePtr = psFont->psFontFiles;
	while (psFontFilePtr && psFontFilePtr->psNextLink)
	{
		psFontFilePtr = psFontFilePtr->psNextLink;
	}

	if (NULL == psFontFilePtr)
	{
		// Head of list of font files
		psFont->psFontFiles = psFontFile;
	}
	else
	{
		psFontFilePtr->psNextLink = psFontFile;
	}

errorExit:
	if (bLocked)
	{
		eStatus = FontSetLock(eFontHandle,
							  NULL,
							  false,
							  eStatus);
	}

	return(eStatus);
}

// Create a font handle
EStatus FontCreate(EFontHandle *peFontHandle)
{
	EStatus eStatus;
	SFont *psFont = NULL;

	// Allocate a handle for this font
	eStatus = HandleAllocate(sg_psFontPool,
							 peFontHandle,
							 &psFont);
	ERR_GOTO();

	// Cache the font handle
	psFont->eFontHandle = *peFontHandle;

	// Lock the list
	eStatus = FontListSetLock(true,
							  eStatus);
	ERR_GOTO();

	// Link this font file into the master list of fonts
	psFont->psNextLink = sg_psFontListHead;
	sg_psFontListHead = psFont;

	// Unlock the list and we're good to go!
	eStatus = FontListSetLock(false,
							  eStatus);

	// Unlock the newly allocated handle
	eStatus = FontSetLock(*peFontHandle,
						  NULL,
						  false,
						  eStatus);

	// All done!

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		(void) FontDestroy(peFontHandle);
	}

	return(eStatus);
}

EStatus FontHandleIsValid(EFontHandle eFontHandle)
{
	EStatus eStatus;

	eStatus = HandleSetLock(sg_psFontPool,
							NULL,
							(EHandleGeneric) eFontHandle,
							EHANDLE_NONE,
							NULL,
							true,
							ESTATUS_OK);

	return(eStatus);
}

EStatus FontInit(void)
{
	EStatus eStatus;

	// Init freetype
	eStatus = FeetypeToEStatus(FT_Init_FreeType(&sg_sFreefontLibrary));
	ERR_GOTO();

	eStatus = HandlePoolCreate(&sg_psFontPool,
							   "Font handle pool",
								FONT_POOL_COUNT,
								sizeof(SFont),
								offsetof(SFont, sFontLock),
								FontHandleAllocate,
								FontHandleDeallocate);
	ERR_GOTO();

	eStatus = OSCriticalSectionCreate(&sg_sFontListLock);
	ERR_GOTO();

errorExit:
	return(eStatus);
}