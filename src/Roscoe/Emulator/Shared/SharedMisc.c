#include <ctype.h>
#include "Shared/Shared.h"
#include "Arch/Arch.h"
#include "Shared/SharedMisc.h"
#include "Platform/Platform.h"
#include "Shared/CmdLine.h"

// Search for UTF8 multi-byte characters in a string
static bool IsStringUTF8(char *peString)
{
	uint8_t* pu8Char;
	bool bFoundMB = false;

	for( pu8Char = (uint8_t*)peString; 
		 *pu8Char; 
		 pu8Char++ )
	{
		if( (*pu8Char) >> 7 )
		{
			bFoundMB = true;
			break;
		}
	}

	return( bFoundMB );
}

// This takes an incoming non-UTF8 string, strips it of white space,
// then capitalizes every word
void SharedCapitalizeAllWords(char *peString)
{
	bool bCapital = true;

	// If we have a NULL string or it's UTF-8 don't do anything
	if ((NULL == peString) || (IsStringUTF8(peString)))
	{
		return;
	}

	// Eliminate whitespace
	StripWhitespace(peString);

	// Loop through all characters of this string. The first character of a word
	// is capitalized. Subsequent aren't.
	while (*peString)
	{
		if (islower(*peString))
		{
			// Force the character to be upper case
			if (bCapital)
			{
				*peString = toupper(*peString);
			}

			bCapital = false;
		}
		else
		if (false == isupper(*peString))
		{
			// Some other character
			if ('\'' == *peString)
			{
				// Exclude apostrophes from capitalization
			}
			else
			{
				bCapital = true;
			}
		}
		else
		{
			bCapital = false;
		}

		++peString;
	}
}



EStatus SharedMiscInit(void)
{
	EStatus eStatus = ESTATUS_OK;


errorExit:
	return(eStatus);
}

EStatus SharedMiscShutdown(void)
{
	EStatus eStatus = ESTATUS_OK;

	return(eStatus);
}

