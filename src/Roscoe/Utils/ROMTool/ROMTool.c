#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "lex.h"
#include "types.h"

UINT8 *pbImage = NULL;
UINT32 dwSize = 0;

enum 
{
	LEX_SIZE = FIRST_USER_TOKEN,
	LEX_FILL,
	LEX_OFFSET,
	LEX_WITH,
	LEX_LOAD,
	LEX_CRC16,
	LEX_SAVE,
	LEX_TO,
	LEX_LIMIT,
	LEX_PATCH,
	LEX_COMPSIZE,
	LEX_CHECKCODE,
	LEX_DATESTAMP,
	LEX_SAVE_EFI,
	LEX_CLOSE_EFI,
	LEX_RESET_EFI,
	LEX_RESET_NORMAL,
	LEX_RESET_ROLAND,
	LEX_RESET_FTM,
	LEX_RESET_AUXBOOT,
	LEX_RESET_EUROPA,
	LEX_DELAYEFI,
	LEX_SERIAL,
	LEX_PROMPT
};

struct sReservedWords sRomtool[] = 
{
	{"include",				INCLUDE},

	{"size",		LEX_SIZE},
	{"fill",		LEX_FILL},
	{"offset",	LEX_OFFSET},
	{"with",		LEX_WITH},
	{"load",		LEX_LOAD},
	{"crc16",	LEX_CRC16},
	{"save",		LEX_SAVE},
	{"patch",	LEX_PATCH},
	{"to",		LEX_TO},
	{"limit",	LEX_LIMIT},
	{"compsize",LEX_COMPSIZE},
	{"checkcode", LEX_CHECKCODE},
	{"datestamp", LEX_DATESTAMP},
	{"saveefi", LEX_SAVE_EFI},
	{"closeefi", LEX_CLOSE_EFI},
	{"resetefi", LEX_RESET_EFI},
	{"serial", LEX_SERIAL},
	{"prompt", LEX_PROMPT},
	{"normal",	LEX_RESET_NORMAL},
	{"roland", 	LEX_RESET_ROLAND},
	{"ftm",		LEX_RESET_FTM},
	{"auxboot", LEX_RESET_AUXBOOT}, 
	{"europa",	LEX_RESET_EUROPA},
	{"delayefi",	LEX_DELAYEFI},

	{NULL, 0}
};

static UINT8 u8ParityBitTable[256] =
{
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};

static unsigned char PI_SUBST[256] = 
{
  41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
  19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
  76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
  138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
  245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
  148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
  39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
  181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
  150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
  112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
  96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
  85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
  234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
  129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
  8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
  203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
  166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
  31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

static UINT8 u8EFIImageName[200];
static UINT8 u8SerialNumber[6];
UINT8 u8OnBit = 0;
UINT8 u8BitAccumulator = 0;

static UINT16 wCRCTable[256] = 
{
	0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
	0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
	0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
	0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
	0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
	0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
	0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
	0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
	0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
	0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
	0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
	0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
	0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
	0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
	0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
	0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
	0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};

void ParseSize()
{
	UINT32 dwToken;

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Constant value expected after size");
		exit(1);
	}
	else
	{
		dwSize = lexval.integer;
		if(0 == dwSize)
		{
			yyerror("0 Length image illegal");
			exit(1);
		}

		pbImage = calloc(dwSize, 1);
		if (NULL == pbImage)
		{
			yyerror("Can't allocate %ld bytes for image", dwSize);
		}

		printf("Allocated %ld bytes for image\n", dwSize);	

		dwToken = yylex();

		if (dwToken != ';')
		{
			yyerror("Semicolon expected at end of statement");
			exit(1);
		}
	}
}

void ParseFill()
{
	UINT32 dwToken;
	UINT32 dwFrom = 0;
	UINT32 dwTo = 0;
	UINT32 dwValue = 0;

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Source address expected");
		exit(1);
	}

	dwFrom = lexval.integer;

	dwToken = yylex();
	if (LEX_TO != dwToken)
	{
		yyerror("'TO' expected");
		exit(1);
	}

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Destination address expected");
		exit(1);
	}

	dwTo = lexval.integer;

	dwToken = yylex();
	if (LEX_WITH != dwToken)
	{
		yyerror("WITH Expected");
		exit(1);
	}

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Fill value expected");
		exit(1);
	}

	dwValue = lexval.integer;

	if (dwValue > 0xff)
	{
		yyerror("Can't fill with values larger than 1 byte");
		exit(1);
	}

	dwToken = yylex();
	if (dwToken != ';')
	{
		yyerror("Semicolon expected");
		exit(1);
	}

	if (dwFrom > dwTo)
	{
		dwToken = dwFrom;
		dwFrom = dwTo;	
		dwTo = dwToken;
	}

	if (dwFrom >= dwSize || dwTo >= dwSize)
	{
		yyerror("Address beyond size of image");
		exit(1);
	}

	printf("Setting %.8lxh - %.8lxh with %.2xh\n", dwFrom, dwTo, (UINT8) dwValue);
	memset(pbImage + dwFrom, dwValue, (dwTo - dwFrom) + 1);
}

void ParsePatch(void)
{
	UINT32 dwAddr = 0;
	UINT32 dwToken = 0;

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Address of patch expected");
		exit(1);
	}

	dwAddr = lexval.integer;

	dwToken = yylex();

	if (dwToken != '=')
	{
		yyerror("'=' Expected");
		exit(1);
	}

	dwToken = yylex();

	while (dwToken != INVALID && dwToken != ';')
	{
		if (dwToken != CONSTANT)
		{
			yyerror("Constant value expected");
			exit(1);
		}

		if (lexval.integer > 0xff)
		{
			yyerror("Constant must be in 0-ffh range");
			exit(1);
		}

		if (dwAddr >= dwSize)
		{
			yyerror("Patch beyond end of image size");
			exit(1);
		}

		pbImage[dwAddr++] = lexval.integer;
		dwToken = yylex();
	}
}

void ParseLoad(void)
{
	UINT32 dwToken = 0;
	UINT32 dwAddr = 0;
	UINT32 dwLimit = 0xffffffff;
	UINT32 dwOffset = 0;
	UINT8 *pbAddr = NULL;
	UINT8 bByte;
	FILE *fp = NULL;

	dwToken = yylex();
	
	if (STRING != dwToken)
	{
		yyerror("Filename (in quotes) expected");
		exit(1);
	}
	
	fp = fopen(lexval.string, "rb");

	if (NULL == fp)
	{
		yyerror("File '%s' not found", lexval.string);
		exit(1);
	}

	printf("Loading file '%s'\n", lexval.string);

	dwToken = yylex();

	if ('@' != dwToken)
	{
		yyerror("'@' Expected");
		exit(1);
	}

	dwToken = yylex();

	if (CONSTANT != dwToken)
	{
		yyerror("Address expected");
		exit(1);
	}

	dwAddr = lexval.integer;
	if (dwAddr >= dwSize)
	{
		yyerror("Can't load file out of defined size");
		exit(1);
	}

	// Otherwise, we go in a loop and look for other goodies

	while (dwToken != ';' && dwToken != INVALID)
	{
		dwToken = yylex();

		if (LEX_OFFSET == dwToken)
		{
			dwToken = yylex();
			if (CONSTANT != dwToken)
			{
				yyerror("Offset value expected");
				exit(1);
			}

			dwOffset = lexval.integer;
		}
		else
		if (LEX_LIMIT == dwToken)
		{
			dwToken = yylex();
			if (CONSTANT != dwToken)
			{
				yyerror("Limit value expected");
				exit(1);
			}

			dwLimit = lexval.integer;
		}
	}

	// Let's attempt a seek

	if (fseek(fp, dwOffset, SEEK_SET))
	{
		yyerror("Seek error - offset beyond end of file?");
		exit(1);
	}

	// Load the file in place!

	while ((feof(fp) == 0) && dwLimit)
	{
		bByte = fgetc(fp);
		if (feof(fp) == 0)
		{
			if (dwAddr >= dwSize)
			{
				yyerror("Attempted to load file past end of image");
				exit(1);
			}	
			pbImage[dwAddr] = bByte;
			dwAddr++;
		}

		--dwLimit;
	}

	fclose(fp);
}

void ParseCRC16(void)
{
	UINT32 dwToken = 0;
	UINT32 dwCrcAddr = 0;
	UINT16 wCrc16 = 0;
	UINT32 dwFrom = 0;
	UINT32 dwTo = 0;
	UINT32 dwLoop = 0;

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Source address expected");
		exit(1);
	}

	dwFrom = lexval.integer;

	dwToken = yylex();
	if (LEX_TO != dwToken)
	{
		yyerror("'TO' expected");
		exit(1);
	}

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("Destination address expected");
		exit(1);
	}

	dwTo = lexval.integer;

	dwToken = yylex();

	if ('@' != dwToken)
	{
		yyerror("@ Expected");
		exit(1);
	}

	dwToken = yylex();
	if (CONSTANT != dwToken)
	{
		yyerror("CRC Placement address expected");
		exit(1);
	}

	dwCrcAddr = lexval.integer;

	dwToken = yylex();
	if (';' != dwToken)
	{
		yyerror("Semicolon expected");
		exit(1);
	}

	// Now compute the CRC

	if (dwTo < dwFrom)
	{
		dwToken = dwTo;
		dwTo = dwFrom;
		dwFrom = dwToken;
	}

	if (dwTo >= dwSize)
	{
		yyerror("Range beyond end of image size");
		exit(1);
	}

	if ((dwCrcAddr + 1) >= dwSize)
	{
		yyerror("CRC Placement beyond end of image");
		exit(1);
	}

	wCrc16 = 0;

	for (dwLoop = dwFrom; dwLoop <= dwTo; dwLoop++)
	{
		wCrc16 = wCRCTable[(wCrc16 ^ pbImage[dwLoop]) & 0xff] ^ (wCrc16 >> 8);
	}

	printf("CRC16=%.4xh\n", wCrc16);

	pbImage[dwCrcAddr] = wCrc16 & 0xff;
	pbImage[dwCrcAddr + 1] = wCrc16 >> 8;
}

void ParseSave()
{
	UINT32 dwToken = 0;
	FILE *fp;

	dwToken = yylex();

	if (dwToken != STRING)
	{
		yyerror("Filename expected");
		exit(1);
	}

	fp = fopen(lexval.string, "wb");

	if (NULL == fp)
	{
		yyerror("Can't open %s for writing", lexval.string);
		exit(1);
	}

	printf("Saved to '%s' %ld bytes\n", lexval.string, dwSize);

	fwrite(pbImage, 1, dwSize, fp);
	fclose(fp);

	dwToken = yylex();

	if (dwToken != ';')
	{
		yyerror("Semicolon expected at end of statement");
		exit(1);
	}
}

void ParseCompsize()
{
	UINT32 u32Low = 0;
	UINT32 u32High = 0;
	UINT32 u32Lockdown = 0;
	UINT32 u32Minimum = 0;
	UINT32 dwToken;

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("First operand: Constant expected");
		exit(1);
	}

	u32Low = lexval.integer;

	dwToken = yylex();

	if (dwToken != LEX_TO)
	{
		yyerror("Operator 'to' expected");
		exit(1);
	}

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("Constant expected");
		exit(1);
	}

	u32High = lexval.integer;

	if (u32High < u32Low)
	{
		yyerror("Low address must be lower than high address");
		exit(1);
	}

	dwToken = yylex();

	if (dwToken != '@')
	{
		yyerror("'@' expected");
		exit(1);
	}

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("Constant expected");
		exit(1);
	}

	u32Lockdown = lexval.integer;

	dwToken = yylex();

	if (dwToken != ';')
	{
		if (dwToken != LEX_LIMIT)
		{
			yyerror("Semicolon or limit operator expected at the end of statement\n");
			exit(1);
		}

		dwToken = yylex();

		if (dwToken != CONSTANT)
		{
			yyerror("Constant expected");
			exit(1);
		}

		u32Minimum = lexval.integer;
		dwToken = yylex();
	}

	if (dwToken != ';')
	{
		yyerror("Semicolon expected at end of statement");
		exit(1);
	}

	// Let's figure out where it is

	while ((pbImage[u32High] == 0xff) && (u32High >= u32Low) && (u32High > u32Minimum))
	{
		--u32High;
	}

	if (u32High < u32Low)
	{
		yyerror("Can't figure size - no data bytes!");
		exit(1);
	}

	u32High -= u32Low;
	u32High++;
	pbImage[u32Lockdown] = u32High & 0xff;
	pbImage[u32Lockdown + 1] = u32High >> 8;
}

void ParseDatestamp()
{
	UINT32 dwToken;
	UINT32 u32Addr;
	time_t u32Time;
	struct tm *pTime;

	dwToken = yylex();

	if (dwToken != '@')
	{
		yyerror("'@' Expected");
		exit(1);
	}

	// Get where we put it

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("Constant expected");
		exit(1);
	}

	u32Addr = lexval.integer;

	dwToken = yylex();

	if (dwToken != ';')
	{
		yyerror("Semicolon expected at end of statement");
		exit(1);
	}
	
	// Compute time stuff

	u32Time = time(NULL);
	pTime = localtime(&u32Time);

	pTime ->tm_year += 1900;

	pbImage[u32Addr++] = pTime->tm_mon + 1;
	pbImage[u32Addr++] = pTime->tm_mday;
	pbImage[u32Addr++] = pTime->tm_year >> 7;
	pbImage[u32Addr] = pTime->tm_year & 0x7f;
}

void ParseCheckcode()
{
	UINT32 u32HeaderAddr = 0;
	UINT32 u32Lockdown = 0;
	UINT32 dwToken = 0;
	UINT32 u32Loop;
	UINT32 u32Base;
	UINT32 u32Size;
	UINT16 checkVal;
	UINT8 string[150];
	UINT8 bVal;
	UINT8 R4, R3, R2, R5, ACC, B;

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("First operand: Constant expected");
		exit(1);
	}

	u32HeaderAddr = lexval.integer;

	dwToken = yylex();

	if (dwToken != STRING)
	{
		yyerror("Header tag expected");
		exit(1);
	}

	strcpy(string, lexval.string);

	dwToken = yylex();

	if (dwToken != '@')
	{
		yyerror("'@' expected");
		exit(1);
	}

	dwToken = yylex();

	if (dwToken != CONSTANT)
	{
		yyerror("Constant expected");
		exit(1);
	}

	u32Lockdown = lexval.integer;

	dwToken = yylex();

	if (dwToken != ';')
	{
		yyerror("Semicolon expected at end of statement");
		exit(1);
	}

	// First figure out if our tag is here

	for (u32Loop = 0; u32Loop < strlen(string); u32Loop++)
	{
		if (pbImage[u32HeaderAddr] != string[u32Loop])
		{
			yywarning("WARNING: Tag '%s' not found at %.4lx - no action taken\n", string, u32HeaderAddr);
			return;
		}

		++u32HeaderAddr;
	}

	// We found the tag. Let's compute the check code

	u32Base = u32HeaderAddr + 2;
	u32Size = pbImage[u32Base] | (pbImage[u32Base + 1] << 8);
	u32Base += 4;	// Skip past version #s and size

	// Let's do it! Check code time!

	R2 = u32Size & 0xff;
	R3 = u32Size >> 8;
	R4 = 0;
	R5 = 0;

	while (1)
	{
top:
		ACC = pbImage[u32Base];
		u32Base++;
		B = ACC;

		R5 <<= 1;
		if (R4 & 0x80)
		{
			R5 |= 0x01;
		}

		R4 <<= 1;

		ACC = R4;
		ACC ^= R2;
		ACC ^= B;
		R4 = ACC;

		ACC = R5;
		ACC ^= R3;
		ACC ^= 0;
		R5 = ACC;

		R2--;
		ACC = R2;
		if (R2)
		{
			goto top;
		}

		ACC |= R3;
		if (0 == ACC)
		{
			break;
		}

		R3--;
		goto top;
	}

	pbImage[u32HeaderAddr] = R4 & 0xff;
	pbImage[u32HeaderAddr + 1] = R5;
}

void OpenEFI(void)
{
	FILE *fp;
	UINT32 dwToken = 0;

	dwToken = yylex();

	if (dwToken != STRING)
	{
		yyerror("Filename expected");
		exit(1);
	}

	strcpy(u8EFIImageName, lexval.string);

	fp = fopen(u8EFIImageName, "wb+");

	if (NULL == fp)
	{
		yyerror("Can't open filename");
		exit(1);
	}	

	fputc(0x0, fp);	// Rev 1
	fputc(0x0, fp);	// LSB Of CRC (filled in on close)
	fputc(0x0, fp);	// MSB Of CRC (filled in on close)

	fclose(fp);

	printf("Opened/cleared EFI file %s\n", u8EFIImageName);

	dwToken = yylex();

	if (dwToken != ';')
	{
		yyerror("Semicolon expected");
		exit(1);
	}
}

void ParseFile()
{
	UINT32 dwToken = 0;

	while (dwToken != INVALID)
	{
		dwToken = yylex();

		if (LEX_SIZE == dwToken)
		{
			ParseSize();
		}
		else
		if (LEX_FILL == dwToken)
		{
			ParseFill();
		}
		else
		if (LEX_LOAD == dwToken)
		{
			ParseLoad();
		}
		else
		if (LEX_CRC16 == dwToken)
		{
			ParseCRC16();
		}
		else
		if (LEX_SAVE == dwToken)
		{
			ParseSave();
		}
		else
		if (LEX_PATCH == dwToken)
		{
			ParsePatch();
		}
		else
		if (LEX_COMPSIZE == dwToken)
		{
			ParseCompsize();
		}
		else
		if (LEX_CHECKCODE == dwToken)
		{
			ParseCheckcode();
		}
		else
		if (LEX_DATESTAMP == dwToken)
		{
			ParseDatestamp();
		}
		else
		if (INVALID != dwToken)
		{
			yyerror("Syntax error (%.8lx). Command expected", dwToken);
			exit(1);
		}
	}
}

int main(int argc, char **argv)
{
	UINT32 u32Loop;

	if (argc < 2)
	{
		printf("Parse filename required\n");
		exit(1);
	}

	// Set up the reserved word list

	SetReservedWordlist(sRomtool);

	// Now let's open ourselves up

	if (yyopen(argv[1], LEX_FILE) == 0)
	{
		printf("Can't open file '%s'\n", argv[1]);
		exit(1);
	}
	
	// Let's rock

	ParseFile();

	// And close things up

	yykill();
	return(0);
}
