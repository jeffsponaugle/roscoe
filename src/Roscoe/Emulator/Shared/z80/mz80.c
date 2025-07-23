#include <stdio.h>
#include "Shared/Shared.h"
#include "Shared/types.h"
#include "Shared/Z80/mz80.h"

// Name of file generated when there's a trace going on
#define MZ80_TRACE_FILE		"mz80trace.txt"

#ifdef _WIN32
static FILE *sg_psTraceFile = NULL;
#endif

#ifdef _WIN32
static BOOL sg_bTracing = FALSE;

void MZ80StartTrace(void)
{
	FILE *fp;

	// Zero out the trace file (if there is one)
	fp = fopen(MZ80_TRACE_FILE, "w");
	fclose(fp);

	sg_bTracing = TRUE;
}
#endif

// Fetch the next word at the program counter and increment it
#define		PC_WORD_FETCH(x)		x = *pu8PC + (*(pu8PC + 1) << 8); pu8PC += 2;
#define		PC_BYTE_FETCH(x)		x = *pu8PC; pu8PC++;

// Used for JR xx/DJNZ relative PC dispatching
#define	PC_REL_IMM()	pu8PC = psCPU->pu8CodeBase + ((UINT32) (pu8PC + 1) - (UINT32) psCPU->pu8CodeBase) + *((INT8 *) pu8PC)
#define PC_ABS_JMP()	pu8PC = psCPU->pu8CodeBase + ((*pu8PC) | (*(pu8PC + 1) << 8))

// Pushing/popping
#define PUSHWORD(x)		MZ80WriteByte(psCPU, --psCPU->z80sp, (UINT8) (x >> 8)); MZ80WriteByte(psCPU, --psCPU->z80sp, (UINT8) x)
#define POPWORD(x)		x = MZ80ReadByte(psCPU, psCPU->z80sp++); x |= (MZ80ReadByte(psCPU, psCPU->z80sp++) << 8)

//#define	PUSHWORD(x)		psCPU->pu8Base[psCPU->z80sp - 1] = (UINT8) (x >> 8); psCPU->pu8Base[psCPU->z80sp - 2] = (UINT8) (x); psCPU->z80sp -= 2;
//#define POPWORD(x)		x = (psCPU->pu8Base[psCPU->z80sp] | (psCPU->pu8Base[psCPU->z80sp + 1] << 8)); psCPU->z80sp += 2;

// Post increment flags
static const UINT8 sg_u8PreIncFlags[0x100] = 
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,

	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x94,

	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,

	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x50
};

// Post decrement flags
static const UINT8 sg_u8PreDecFlags[0x100] = 
{
	0x92,0x42,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,

	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,

	0x16,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,

	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82
};

// Sign, zero, and parity
static const UINT8 sg_u8SZP[] =
{
	0x44,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,

	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,

	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,

	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84
};

// Sign and zero only
static const UINT8 sg_u8SZ[0x100] = 
{
	0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,

	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80
};

// DAA Flag/data table
static const UINT16 sg_u16DAATable[0x800] = 
{
	0x5400,0x1001,0x1002,0x1403,0x1004,0x1405,0x1406,0x1007,
	0x1008,0x1409,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,0x1016,0x1417,
	0x1418,0x1019,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,0x1026,0x1427,
	0x1428,0x1029,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,0x1436,0x1037,
	0x1038,0x1439,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,0x1046,0x1447,
	0x1448,0x1049,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,0x1456,0x1057,
	0x1058,0x1459,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,0x1466,0x1067,
	0x1068,0x1469,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,0x1076,0x1477,
	0x1478,0x1079,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,0x9086,0x9487,
	0x9488,0x9089,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,0x9496,0x9097,
	0x9098,0x9499,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,
	0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,
	0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,
	0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,
	0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,
	0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,
	0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,0x1566,0x1167,
	0x1168,0x1569,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,0x1176,0x1577,
	0x1578,0x1179,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,0x9186,0x9587,
	0x9588,0x9189,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,0x9596,0x9197,
	0x9198,0x9599,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,0x95a6,0x91a7,
	0x91a8,0x95a9,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,0x91b6,0x95b7,
	0x95b8,0x91b9,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,0x95c6,0x91c7,
	0x91c8,0x95c9,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,0x91d6,0x95d7,
	0x95d8,0x91d9,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,0x91e6,0x95e7,
	0x95e8,0x91e9,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,0x95f6,0x91f7,
	0x91f8,0x95f9,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,
	0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,
	0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,
	0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,
	0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,
	0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,
	0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1406,0x1007,0x1008,0x1409,0x140a,0x100b,0x140c,0x100d,
	0x100e,0x140f,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1016,0x1417,0x1418,0x1019,0x101a,0x141b,0x101c,0x141d,
	0x141e,0x101f,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1026,0x1427,0x1428,0x1029,0x102a,0x142b,0x102c,0x142d,
	0x142e,0x102f,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1436,0x1037,0x1038,0x1439,0x143a,0x103b,0x143c,0x103d,
	0x103e,0x143f,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1046,0x1447,0x1448,0x1049,0x104a,0x144b,0x104c,0x144d,
	0x144e,0x104f,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1456,0x1057,0x1058,0x1459,0x145a,0x105b,0x145c,0x105d,
	0x105e,0x145f,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1466,0x1067,0x1068,0x1469,0x146a,0x106b,0x146c,0x106d,
	0x106e,0x146f,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1076,0x1477,0x1478,0x1079,0x107a,0x147b,0x107c,0x147d,
	0x147e,0x107f,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9086,0x9487,0x9488,0x9089,0x908a,0x948b,0x908c,0x948d,
	0x948e,0x908f,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9496,0x9097,0x9098,0x9499,0x949a,0x909b,0x949c,0x909d,
	0x909e,0x949f,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,
	0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,
	0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,
	0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,
	0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,
	0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,
	0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1566,0x1167,0x1168,0x1569,0x156a,0x116b,0x156c,0x116d,
	0x116e,0x156f,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1176,0x1577,0x1578,0x1179,0x117a,0x157b,0x117c,0x157d,
	0x157e,0x117f,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9186,0x9587,0x9588,0x9189,0x918a,0x958b,0x918c,0x958d,
	0x958e,0x918f,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9596,0x9197,0x9198,0x9599,0x959a,0x919b,0x959c,0x919d,
	0x919e,0x959f,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a6,0x91a7,0x91a8,0x95a9,0x95aa,0x91ab,0x95ac,0x91ad,
	0x91ae,0x95af,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b6,0x95b7,0x95b8,0x91b9,0x91ba,0x95bb,0x91bc,0x95bd,
	0x95be,0x91bf,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c6,0x91c7,0x91c8,0x95c9,0x95ca,0x91cb,0x95cc,0x91cd,
	0x91ce,0x95cf,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d6,0x95d7,0x95d8,0x91d9,0x91da,0x95db,0x91dc,0x95dd,
	0x95de,0x91df,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e6,0x95e7,0x95e8,0x91e9,0x91ea,0x95eb,0x91ec,0x95ed,
	0x95ee,0x91ef,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f6,0x91f7,0x91f8,0x95f9,0x95fa,0x91fb,0x95fc,0x91fd,
	0x91fe,0x95ff,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,
	0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,
	0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,
	0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,
	0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,
	0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,
	0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x5600,0x1201,0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,
	0x1208,0x1609,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x1210,0x1611,0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,
	0x1618,0x1219,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x1220,0x1621,0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,
	0x1628,0x1229,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x1630,0x1231,0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,
	0x1238,0x1639,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x1240,0x1641,0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,
	0x1648,0x1249,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x1650,0x1251,0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,
	0x1258,0x1659,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x1660,0x1261,0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,
	0x1268,0x1669,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x1270,0x1671,0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,
	0x1678,0x1279,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x9280,0x9681,0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,
	0x9688,0x9289,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x9690,0x9291,0x9292,0x9693,0x9294,0x9695,0x9696,0x9297,
	0x9298,0x9699,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,
	0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,
	0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,
	0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,
	0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,
	0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,
	0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97a0,0x93a1,0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,
	0x93a8,0x97a9,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x93b0,0x97b1,0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,
	0x97b8,0x93b9,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x97c0,0x93c1,0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,
	0x93c8,0x97c9,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x93d0,0x97d1,0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,
	0x97d8,0x93d9,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93e0,0x97e1,0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,
	0x97e8,0x93e9,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x97f0,0x93f1,0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,
	0x93f8,0x97f9,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x5700,0x1301,0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,
	0x1308,0x1709,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x1310,0x1711,0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,
	0x1718,0x1319,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x1320,0x1721,0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,
	0x1728,0x1329,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x1730,0x1331,0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,
	0x1338,0x1739,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,
	0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,
	0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,
	0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,
	0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,
	0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,
	0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5600,0x1201,
	0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x160a,0x120b,0x160c,0x120d,0x120e,0x160f,0x1210,0x1611,
	0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x121a,0x161b,0x121c,0x161d,0x161e,0x121f,0x1220,0x1621,
	0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x122a,0x162b,0x122c,0x162d,0x162e,0x122f,0x1630,0x1231,
	0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x163a,0x123b,0x163c,0x123d,0x123e,0x163f,0x1240,0x1641,
	0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x124a,0x164b,0x124c,0x164d,0x164e,0x124f,0x1650,0x1251,
	0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x165a,0x125b,0x165c,0x125d,0x125e,0x165f,0x1660,0x1261,
	0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x166a,0x126b,0x166c,0x126d,0x126e,0x166f,0x1270,0x1671,
	0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x127a,0x167b,0x127c,0x167d,0x167e,0x127f,0x9280,0x9681,
	0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x928a,0x968b,0x928c,0x968d,0x968e,0x928f,0x9690,0x9291,
	0x9292,0x9693,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,
	0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,
	0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,
	0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,
	0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,
	0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,
	0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x979a,0x939b,0x979c,0x939d,0x939e,0x979f,0x97a0,0x93a1,
	0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x97aa,0x93ab,0x97ac,0x93ad,0x93ae,0x97af,0x93b0,0x97b1,
	0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x93ba,0x97bb,0x93bc,0x97bd,0x97be,0x93bf,0x97c0,0x93c1,
	0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x97ca,0x93cb,0x97cc,0x93cd,0x93ce,0x97cf,0x93d0,0x97d1,
	0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93da,0x97db,0x93dc,0x97dd,0x97de,0x93df,0x93e0,0x97e1,
	0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x93ea,0x97eb,0x93ec,0x97ed,0x97ee,0x93ef,0x97f0,0x93f1,
	0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5700,0x1301,
	0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x170a,0x130b,0x170c,0x130d,0x130e,0x170f,0x1310,0x1711,
	0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x131a,0x171b,0x131c,0x171d,0x171e,0x131f,0x1320,0x1721,
	0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x132a,0x172b,0x132c,0x172d,0x172e,0x132f,0x1730,0x1331,
	0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,
	0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,
	0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,
	0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,
	0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,
	0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,
	0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799 
};

// Timing tables for each basic instruction. Note that the instructions that
// have different timing depending upon branching, this table contains the
// lowest value.
static const UINT8 sg_u8Z80MainCycleCounts[0x100] =
{
	0x04, 0x0a, 0x07, 0x06, 0x04, 0x04, 0x07, 0x04, 0x04, 0x0b, 0x07, 0x06, 0x04, 0x04, 0x07, 0x04,
	0x08, 0x0a, 0x07, 0x06, 0x04, 0x04, 0x07, 0x04, 0x0c, 0x0b, 0x07, 0x06, 0x04, 0x04, 0x07, 0x04,
	0x07, 0x0a, 0x10, 0x06, 0x04, 0x04, 0x07, 0x04, 0x07, 0x0b, 0x10, 0x06, 0x04, 0x04, 0x07, 0x04,
	0x07, 0x0a, 0x0d, 0x06, 0x0b, 0x0b, 0x0a, 0x04, 0x07, 0x0b, 0x0d, 0x06, 0x04, 0x04, 0x07, 0x04,

	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,

	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x04,

	0x05, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x07, 0x0b, 0x05, 0x0a, 0x0a, 0x00, 0x0a, 0x11, 0x07, 0x0b,
	0x05, 0x0a, 0x0a, 0x0b, 0x0a, 0x0b, 0x07, 0x0b, 0x05, 0x04, 0x0a, 0x0b, 0x0a, 0x00, 0x07, 0x0b,
	0x05, 0x0a, 0x0a, 0x13, 0x0a, 0x0b, 0x07, 0x0b, 0x05, 0x04, 0x0a, 0x04, 0x0a, 0x00, 0x07, 0x0b,
	0x05, 0x0a, 0x0a, 0x04, 0x0a, 0x0b, 0x07, 0x0b, 0x05, 0x06, 0x0a, 0x04, 0x0a, 0x00, 0x07, 0x0b
};

static const UINT8 sg_u8Z80EDCycleCounts[0x100] = 
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 

	0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x0e, 0x08, 0x09, 0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x0e, 0x08, 0x09,
	0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x09, 0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x09,
	0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x12, 0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x12,
	0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x00, 0x0c, 0x0c, 0x0f, 0x14, 0x08, 0x08, 0x08, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
};

static const UINT8 sg_u8Z80DDFDCycleCounts[0x100] = 
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x0e, 0x14, 0x0a, 0x09, 0x09, 0x09, 0x00, 0x00, 0x0f, 0x14, 0x0a, 0x09, 0x09, 0x09, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x17, 0x17, 0x13, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 

	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x13, 0x09,	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x13, 0x09,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 

	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x13, 0x00, 

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x0e, 0x00, 0x17, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const UINT8 sg_u8Z80CBCycleCounts[0x100] = 
{
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 

	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0c, 0x08,

	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 

	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0f, 0x08
};

static const UINT8 sg_u8Z80DDFDCBCycleCounts[0x100] = 
{
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 

	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 

	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 

	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17
};

static UINT8 MZ80ReadByte(CONTEXTMZ80 *psCPU,
						  UINT16 u16Address)
{
	MemoryRead8 *psMem = psCPU->mz80MemoryRead;

	while (psMem->memoryCall)
	{
		if ((u16Address >= psMem->lowAddr) && (u16Address <= psMem->highAddr))
		{
			return(psMem->memoryCall(u16Address,
									 psMem));
		}
		++psMem;
	}

	return(psCPU->pu8Base[u16Address]);
}

static void MZ80WriteByte(CONTEXTMZ80 *psCPU,
						  UINT16 u16Address,
						  UINT8 u8Data)
{
	MemoryWrite8 *psMem = psCPU->mz80MemoryWrite;

	while (psMem->memoryCall)
	{
		if ((u16Address >= psMem->lowAddr) && (u16Address <= psMem->highAddr))
		{
			psMem->memoryCall(u16Address,
							  u8Data,
							  psMem);
			return;
		}
		++psMem;
	}

	psCPU->pu8Base[u16Address] = u8Data;
}

static UINT8 MZ80InPort(CONTEXTMZ80 *psCPU,
						UINT16 u16Address)
{
	IORead8 *psIO = psCPU->mz80IORead;

	while (psIO->IOCall)
	{
		if ((u16Address >= psIO->lowAddr) && (u16Address <= psIO->highAddr))
		{
			return(psIO->IOCall(u16Address));
		}
		psIO++;
	}

	// Unhandled
	return(0xff);
}

static void MZ80OutPort(CONTEXTMZ80 *psCPU,
						UINT16 u16Address,
						UINT8 u8Data)
{
	IOWrite8 *psIO = psCPU->mz80IOWrite;

	while (psIO->IOCall)
	{
		if ((u16Address >= psIO->lowAddr) && (u16Address <= psIO->highAddr))
		{
			psIO->IOCall(u16Address,
						 u8Data);
			return;
		}
		psIO++;
	}

	// Unhandled - drop it on the ground
}

// This resets the Z80 context passed in on input
void MZ80Reset(CONTEXTMZ80 *psCPU)
{
	psCPU->z80AF = 0xffff;
	psCPU->z80BC = 0;
	psCPU->z80DE = 0;
	psCPU->z80HL = 0;
	psCPU->z80afprime = 0;
	psCPU->z80bcprime = 0;
	psCPU->z80deprime = 0;
	psCPU->z80hlprime = 0;
	psCPU->z80i = 0;
	psCPU->z80iff = 0;
	psCPU->z80r = 0xff;
	psCPU->z80rBit8 = 0x80;
	psCPU->z80IX = 0xffff;
	psCPU->z80IY = 0xffff;
	psCPU->z80sp = 0xffff;
	psCPU->z80pc = 0;
	psCPU->z80InterruptMode = 0;
	psCPU->z80intAddr = 0x38;
	psCPU->bHalted = FALSE;
}

static UINT8 MZ80RLC(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value >> 7);
	u8Value = (u8Value << 1) | (u8Value >> 7);

	// Set flags appropriately
	psCPU->z80F |= sg_u8SZP[u8Value];

	// Return the shifted value
	return(u8Value);
}

static UINT8 MZ80RRC(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value & 0x01);
	u8Value = (u8Value >> 1) | (u8Value << 7);

	// Set flags appropriately
	psCPU->z80F |= sg_u8SZP[u8Value];

	// Return the shifted value
	return(u8Value);
}

static UINT8 MZ80RL(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT8 u8Temp = psCPU->z80F & Z80F_C;

	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value >> 7);
	u8Value = (u8Value << 1) | u8Temp;

	// Set flags appropriately
	psCPU->z80F |= sg_u8SZP[u8Value];

	// Return the shifted value
	return(u8Value);
}

static UINT8 MZ80RR(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT8 u8Temp = (psCPU->z80F & Z80F_C) << 7;

	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value & 0x01);
	u8Value = (u8Value >> 1) | u8Temp;

	// Set flags appropriately
	psCPU->z80F |= sg_u8SZP[u8Value];

	// Return the shifted value
	return(u8Value);
}

static UINT8 MZ80SLA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value >> 7);
	u8Value <<= 1;
	psCPU->z80F |= sg_u8SZP[u8Value];
	return(u8Value);
}

static UINT8 MZ80SRA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value & 0x01);
	u8Value = (u8Value >> 1) | (u8Value & 0x80);
	psCPU->z80F |= sg_u8SZP[u8Value];
	return(u8Value);
}

static UINT8 MZ80SLL(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Temporarily until it's validated
	BASSERT(0);

	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value >> 7);
	u8Value = (u8Value << 1) | 0x01;
	psCPU->z80F |= sg_u8SZP[u8Value];
	return(u8Value);
}

static UINT8 MZ80SRL(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Clear all flags but the XF/YF (undocumented) flags
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
	psCPU->z80F |= (u8Value & 0x01);
	u8Value >>= 1;
	psCPU->z80F |= sg_u8SZP[u8Value];
	return(u8Value);
}

// Sets the overflow flag based on operators
#define SET_OVERFLOW8(a, b, c)	psCPU->z80F |= (((a ^ b ^ c ^ (c >> 1)) & 0x80) >> 5)
#define SET_OVERFLOW16(a, b, c)	psCPU->z80F |= (((a ^ b ^ c ^ (c >> 1)) & 0x8000) >> 13)

// Increment the R register and preserve bit 7
#define	INC_R(x) x->z80r++

static void MZ80ADDA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT16 u16Temp;

	// 16 Bit quantity needed for overflow checkout
	u16Temp = u8Value + psCPU->z80A;
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);

	// See if there's a carry generated
	psCPU->z80F |= ((u16Temp >> 8) & Z80F_C);

	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Now overflow
	SET_OVERFLOW8(psCPU->z80A, u8Value, u16Temp);

	// Sign/zero
	psCPU->z80F |= sg_u8SZ[u16Temp & 0xff];

	// Store the result in A
	psCPU->z80A = (UINT8) u16Temp;
}

static void MZ80ADCA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT16 u16Temp;

	// 16 Bit quantity needed for overflow checkout, including carry
	u16Temp = u8Value + psCPU->z80A + (psCPU->z80F & Z80F_C);
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);

	// See if there's a carry generated
	psCPU->z80F |= ((u16Temp >> 8) & Z80F_C);

	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Now overflow
	SET_OVERFLOW8(psCPU->z80A, u8Value, u16Temp);

	// Sign/zero
	psCPU->z80F |= sg_u8SZ[u16Temp & 0xff];

	// Store the result in A
	psCPU->z80A = (UINT8) u16Temp;
}

static void MZ80SUBA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT16 u16Temp;

	u16Temp = psCPU->z80A - u8Value;
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);

	// See if there's a carry generated and set the negative flag
	psCPU->z80F |= ((u16Temp >> 8) & Z80F_C) | Z80F_N;

	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Now overflow
	SET_OVERFLOW8(psCPU->z80A, u8Value, u16Temp);

	// Sign/zero
	psCPU->z80F |= sg_u8SZ[u16Temp & 0xff];
	
	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Store the result in A
	psCPU->z80A = (UINT8) u16Temp;
}

static void MZ80SBCA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT16 u16Temp;

	u16Temp = psCPU->z80A - u8Value - (psCPU->z80F & Z80F_C);
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);

	// See if there's a carry generated and set the negative flag
	psCPU->z80F |= ((u16Temp >> 8) & Z80F_C) | Z80F_N;

	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Now overflow
	SET_OVERFLOW8(psCPU->z80A, u8Value, u16Temp);

	// Sign/zero
	psCPU->z80F |= sg_u8SZ[u16Temp & 0xff];
	
	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Store the result in A
	psCPU->z80A = (UINT8) u16Temp;
}

static void MZ80ANDA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Do the AND
	psCPU->z80A &= u8Value;

	// Now set the flags (pretty much everything gets clobbered
	psCPU->z80F = Z80F_H | sg_u8SZP[psCPU->z80A];
}

static void MZ80XORA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Do the XOR
	psCPU->z80A ^= u8Value;

	// Now set the flags (pretty much everything gets clobbered - no half carry like with AND
	psCPU->z80F = sg_u8SZP[psCPU->z80A];
}

static void MZ80ORA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	// Do the OR
	psCPU->z80A |= u8Value;

	// Now set the flags (pretty much everything gets clobbered - no half carry like with AND
	psCPU->z80F = sg_u8SZP[psCPU->z80A];
}

static void MZ80CPA(CONTEXTMZ80 *psCPU, UINT8 u8Value)
{
	UINT16 u16Temp;

	u16Temp = psCPU->z80A - u8Value;
	psCPU->z80F &= ~(Z80F_C | Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);

	// See if there's a carry generated and set the negative flag
	psCPU->z80F |= ((u16Temp >> 8) & Z80F_C) | Z80F_N;

	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Now overflow
	SET_OVERFLOW8(psCPU->z80A, u8Value, u16Temp);

	// Sign/zero
	psCPU->z80F |= sg_u8SZ[u16Temp & 0xff];
	
	// Compute half carry
	psCPU->z80F |= ((psCPU->z80A ^ u8Value ^ u16Temp) & Z80F_H);

	// Don't store the result - just update the flags and that's it
}

static UINT16 MZ80ADD16(CONTEXTMZ80 *psCPU, UINT16 u16Value1, UINT16 u16Value2)
{
	UINT32 u32Result = u16Value1 + u16Value2;

	// Only preserve Z, S, and overflow
	psCPU->z80F &= (Z80F_Z | Z80F_S | Z80F_PV);

	// Move in carry
	psCPU->z80F |= ((u32Result >> 16) & Z80F_C);

	// Now compute half carry
	psCPU->z80F |= (((u16Value1 ^ u16Value2 ^ u32Result) >> 8) & Z80F_H);

	// Truncation here is OK
	return((UINT16) u32Result);
}

static UINT16 MZ80ADC16(CONTEXTMZ80 *psCPU, UINT16 u16Value1, UINT16 u16Value2)
{
	UINT32 u32Result = u16Value1 + u16Value2 + (psCPU->z80F & Z80F_C);

	// Clear out everything except for setting carry, negative, and sign
	psCPU->z80F = (UINT8) (((u32Result >> 16) & Z80F_C) | ((u32Result >> 8) & Z80F_S));

	// If it's zero, set the zero flag
	if ((u32Result & 0xffff) == 0)
	{
		psCPU->z80F |= Z80F_Z;
	}
	
	// Set half carry if applicable
	psCPU->z80F |= (((u16Value1 ^ u16Value2 ^ u32Result) >> 8) & Z80F_H);

	// Set overflow
	SET_OVERFLOW16(u16Value1, u16Value2, u32Result);
	return((UINT16) u32Result);
}

static UINT16 MZ80SBC16(CONTEXTMZ80 *psCPU, UINT16 u16Value1, UINT16 u16Value2)
{
	UINT32 u32Result = u16Value1 - u16Value2 - (psCPU->z80F & Z80F_C);

	// Clear out everything except for setting carry, negative, and sign
	psCPU->z80F = (UINT8) (((u32Result >> 16) & Z80F_C) | ((u32Result >> 8) & Z80F_S) | Z80F_N);

	// If it's zero, set the zero flag
	if ((u32Result & 0xffff) == 0)
	{
		psCPU->z80F |= Z80F_Z;
	}
	
	// Set half carry if applicable
	psCPU->z80F |= (((u16Value1 ^ u16Value2 ^ u32Result) >> 8) & Z80F_H);

	// Set overflow
	SET_OVERFLOW16(u16Value1, u16Value2, u32Result);

	return((UINT16) u32Result);
}

static void MZ80INC8(CONTEXTMZ80 *psCPU, UINT8 *pu8Value)
{
	psCPU->z80F = sg_u8PreIncFlags[*pu8Value] | (psCPU->z80F & Z80F_C);
	*pu8Value = *pu8Value + 1;
}

static void MZ80DEC8(CONTEXTMZ80 *psCPU, UINT8 *pu8Value)
{
	psCPU->z80F = sg_u8PreDecFlags[*pu8Value] | (psCPU->z80F & Z80F_C);
	*pu8Value = *pu8Value - 1;
}

static const UINT8 sg_u8IntTiming[] = 
{
	13, 13, 19
};

BOOL MZ80INT(CONTEXTMZ80 *psCPU)
{
	// If iff1 is clear, bail out since we're already in interrupt context
	if (0 == (psCPU->z80iff & IFF1))
	{
		// Interrupt not taken. Let's latch it
		psCPU->z80iff |= IRQ_LATCH;
#ifdef _WIN32
		if ((sg_psTraceFile) && (sg_bTracing))
		{
			fprintf(sg_psTraceFile, "IRQ: Not taken due to IFF=0, latched\n");
		}
#endif
		return(FALSE);
	}

	if ((sg_psTraceFile) && (sg_bTracing))
	{
		fprintf(sg_psTraceFile, "IRQ: Taken\n");
	}

	// Unhalt the CPU if it's halted
	if (psCPU->bHalted)
	{
		psCPU->bHalted = FALSE;

		// Skip past halt instruction
		psCPU->z80pc++;
	}

	// Increment R
	INC_R(psCPU);

	// Clear IFF1, IFF2, and the IRQ latch
	psCPU->z80iff &= ~(IFF1 | IFF2 | IRQ_LATCH);

	// Push the current program counter on the stack
	PUSHWORD(psCPU->z80pc);

	// Figure out what our target address should be
	if (2 == psCPU->z80InterruptMode)
	{
		psCPU->z80pc = ((UINT16) psCPU->z80i << 8) | psCPU->u8Mode2LowAddr;

		// Now get it out of
		psCPU->z80pc = psCPU->pu8Base[psCPU->z80pc] | (psCPU->pu8Base[psCPU->z80pc + 1] << 8);
#ifdef _WIN32
		if ((sg_psTraceFile) && (sg_bTracing))
		{
			fprintf(sg_psTraceFile, "IRQ: Interrupt mode 2, PC=0x%.4x\n", psCPU->z80pc);
		}
#endif
	}
	else
	{
#ifdef _WIN32
		if ((sg_psTraceFile) && (sg_bTracing))
		{
			fprintf(sg_psTraceFile, "IRQ: Interrupt mode %u, PC=0x%.4x\n", psCPU->z80InterruptMode, psCPU->z80pc);
		}
#endif
		psCPU->z80pc = psCPU->z80intAddr;
	}

	// Add in the appropriate timing for this interrupt
	psCPU->u32ClockTicks += sg_u8IntTiming[psCPU->z80InterruptMode];

	return(TRUE);
}

static UINT8 *IndexedHandler(CONTEXTMZ80 *psCPU,
						     UINT8 *pu8PC,
							 UINT16 *pu16Index,
							 INT32 *ps32Cycles)
{
	UINT8 u8Op;

	u8Op = *pu8PC;
	pu8PC++;

	psCPU->u32ClockTicks += sg_u8Z80DDFDCycleCounts[u8Op];
	*ps32Cycles -= (INT32) sg_u8Z80DDFDCycleCounts[u8Op];
	INC_R(psCPU);

	switch (u8Op)
	{
		case 0x09:			// 0xfd 0x09 - ADD Ix, BC
		{
			*pu16Index = MZ80ADD16(psCPU, *pu16Index, psCPU->z80BC);
			break;
		}
		case 0x19:			// 0xfd 0x19 - ADD Ix, DE
		{
			*pu16Index = MZ80ADD16(psCPU, *pu16Index, psCPU->z80DE);
			break;
		}
		case 0x21:			// 0xfd 0x21 - LD Ix, nn
		{
			PC_WORD_FETCH(*pu16Index);
			break;
		}
		case 0x22:			// 0xfd 0x22 - LD (nn),Ix
		{
			UINT16 u16Temp;
			PC_WORD_FETCH(u16Temp);
			MZ80WriteByte(psCPU, u16Temp++, (UINT8) *pu16Index);
			MZ80WriteByte(psCPU, u16Temp, (UINT8) (*pu16Index >> 8));
			break;
		}
		case 0x23:			// 0xfd 0x23 - INC Ix
		{
			*pu16Index += 1;
			break;
		}
		case 0x24:			// 0xfd 0x24 - INC IX/YH
		{
			UINT8 u8Value = (*pu16Index >> 8);
			MZ80INC8(psCPU, &u8Value);
			*pu16Index = (*pu16Index & 0xff) | (u8Value << 8);
			break;
		}
		case 0x29:			// 0xfd 0x29 - ADD Ix, Ix
		{
			*pu16Index = MZ80ADD16(psCPU, *pu16Index, *pu16Index);
			break;
		}
		case 0x2a:			// 0xfd 0x2a - LD Ix,(nn)
		{
			UINT16 u16Temp;
			PC_WORD_FETCH(u16Temp);
			*pu16Index = MZ80ReadByte(psCPU, u16Temp++);
			*pu16Index |= (MZ80ReadByte(psCPU, u16Temp) << 8);
			break;
		}
		case 0x2b:			// 0xfd 0x2b - DEC Ix
		{
			*pu16Index -= 1;
			break;
		}
		case 0x34:			// 0xfd 0x34 - INC (Ix+n)
		{
			INT16 s16Temp;
			UINT8 u8Value;

			s16Temp = (*((INT8 *) pu8PC)) + *pu16Index;
			++pu8PC;
			u8Value = MZ80ReadByte(psCPU, (UINT16) s16Temp);
			MZ80INC8(psCPU, &u8Value);
			MZ80WriteByte(psCPU, (UINT16) s16Temp, u8Value);
			break;
		}
		case 0x35:			// 0xfd 0x34 - DEC (Ix+n)
		{
			INT16 s16Temp;
			UINT8 u8Value;

			s16Temp = (*((INT8 *) pu8PC)) + *pu16Index;
			++pu8PC;
			u8Value = MZ80ReadByte(psCPU, (UINT16) s16Temp);
			MZ80DEC8(psCPU, &u8Value);
			MZ80WriteByte(psCPU, (UINT16) s16Temp, u8Value);
			break;
		}
		case 0x36:			// 0xfd 0x36 nn xx - LD (Ix+nn), xx
		{
			INT16 s16Temp;

			s16Temp = (*((INT8 *) pu8PC)) + *pu16Index;
			++pu8PC;
			MZ80WriteByte(psCPU, (UINT16) s16Temp, *pu8PC);
			++pu8PC;
			break;
		}
		case 0x39:			// 0xfd 0x09 - ADD Ix, SP
		{
			*pu16Index = MZ80ADD16(psCPU,*pu16Index, psCPU->z80sp);
			break;
		}
		case 0x46:			// 0xdd 0x46 nn - LD B, (Ix+n)
		{
			psCPU->z80B = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x4e:			// 0xdd 0x4e nn - LD C, (Ix+n)
		{
			psCPU->z80C = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x56:			// 0xdd 0x56 nn - LD D, (Ix+n)
		{
			psCPU->z80D = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x5e:			// 0xdd 0x5e nn - LD E, (Ix+n)
		{
			psCPU->z80E = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x66:			// 0xdd 0x66 nn - LD H, (Ix+n)
		{
			psCPU->z80H = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x6e:			// 0xdd 0x6e nn - LD L, (Ix+n)
		{
			psCPU->z80L = MZ80ReadByte(psCPU, (UINT16) (*((INT8 *) pu8PC) + *pu16Index));
			++pu8PC;
			break;
		}
		case 0x6f:			// 0xdd 0x6f - LD IX/YL, A
		{
			*pu16Index = (*pu16Index & 0xff00) | psCPU->z80A;
			break;
		}
		case 0x70:			// 0xdd 0x70 nn - LD (Ix+nn), B
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80B);
			++pu8PC;
			break;
		}
		case 0x71:			// 0xdd 0x71 nn - LD (Ix+nn), C
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80C);
			++pu8PC;
			break;
		}
		case 0x72:			// 0xdd 0x72 nn - LD (Ix+nn), D
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80D);
			++pu8PC;
			break;
		}
		case 0x73:			// 0xdd 0x73 nn - LD (Ix+nn), E
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80E);
			++pu8PC;
			break;
		}
		case 0x74:			// 0xdd 0x74 nn - LD (Ix+nn), H
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80H);
			++pu8PC;
			break;
		}
		case 0x75:			// 0xdd 0x75 nn - LD (Ix+nn), L
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80L);
			++pu8PC;
			break;
		}
		case 0x77:			// 0xdd 0x77 nn - LD (Ix+nn), A
		{
			MZ80WriteByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index, psCPU->z80A);
			++pu8PC;
			break;
		}
		case 0x7c:			// 0xdd 0x7c - LD a, IX/YH
		{
			psCPU->z80A = (UINT8) (*pu16Index >> 8);
			break;
		}
		case 0x7d:			// 0xdd 0x7c - LD a, IX/YL
		{
			psCPU->z80A = (UINT8) *pu16Index;
			break;
		}
		case 0x7e:			// 0xfd 0x7e nn xx - LD A, (Ix+nn)
		{
			psCPU->z80A = MZ80ReadByte(psCPU, (UINT16) ((INT16) *((INT8 *)pu8PC)) + *pu16Index);
			++pu8PC;
			break;
		}
		case 0x86:			// 0xfd 0x86 nn - ADD A, (Ix+n)
		{
			MZ80ADDA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0x8e:			// 0xfd 0x8e nn - ADC A, (Ix+n)
		{
			MZ80ADCA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0x96:			// 0xfd 0x96 nn - SUB A, (Ix+n)
		{
			MZ80SUBA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0x9e:			// 0xfd 0x9e nn - SBC A, (Ix+n)
		{
			MZ80SBCA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0xa6:			// 0xfd 0xa6 nn - AND A, (Ix+n)
		{
			MZ80ANDA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			pu8PC++;
			break;
		}
		case 0xae:			// 0xfd 0xae nn - XOR A, (Ix+n)
		{
			MZ80XORA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0xb6:			// 0xfd 0xbe nn - OR A, (Ix+n)
		{
			MZ80ORA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0xbe:			// 0xfd 0xbe nn - CP (Ix+n)
		{
			MZ80CPA(psCPU, MZ80ReadByte(psCPU, *pu16Index + ((INT16) *((INT8 *) pu8PC))));
			++pu8PC;
			break;
		}
		case 0xcb:
		{
			// All DDCB or FDCB instructions
			UINT16 u16Addr;
			UINT8 u8Data;

			// Compute the IX/IY + n offset
			u16Addr = *pu16Index + ((INT16) (*((INT8 *) pu8PC)));
			pu8PC++;

			// All instructions need to do a read
			u8Data = MZ80ReadByte(psCPU, u16Addr);

			// Now get the operator
			u8Op = *pu8PC;
			++pu8PC;
			psCPU->u32ClockTicks += sg_u8Z80DDFDCBCycleCounts[u8Op];
			*ps32Cycles -= (INT32) sg_u8Z80DDFDCBCycleCounts[u8Op];

			switch (u8Op)
			{
				case 0x06:			// RLC (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80RLC(psCPU, u8Data));
					break;
				}
				case 0x0e:			// RRC (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80RRC(psCPU, u8Data));
					break;
				}
				case 0x16:			// RL (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80RL(psCPU, u8Data));
					break;
				}
				case 0x1e:			// RR (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80RR(psCPU, u8Data));
					break;
				}
				case 0x26:			// SLA (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80SLA(psCPU, u8Data));
					break;
				}
				case 0x2e:			// SRA (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80SRA(psCPU, u8Data));
					break;
				}
				case 0x36:			// SLL (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80SLL(psCPU, u8Data));
					break;
				}
				case 0x3e:			// SRL (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, MZ80SRL(psCPU, u8Data));
					break;
				}
				case 0x46:			// BIT 0-7, (Ix+n)
				case 0x4e:
				case 0x56:
				case 0x5e:
				case 0x66:
				case 0x6e:
				case 0x76:
				case 0x7e:
				{
					psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
					u8Op = (1 << ((u8Op >> 3) & 0x07));				// Get a mask for this bit instruction
					if (u8Data & u8Op)
					{
						// Bit is 1
						psCPU->z80F |= (u8Data & Z80F_S);
					}
					else
					{
						// Bit is 0
						psCPU->z80F |= (Z80F_Z | Z80F_PV);
					}
					break;
				}
				case 0x86:
				case 0x8e:
				case 0x96:
				case 0x9e:
				case 0xa6:
				case 0xae:
				case 0xb6:
				case 0xbe:			// RES 0-7, (Ix+n) 
				{
					MZ80WriteByte(psCPU, u16Addr, u8Data & ~(1 << ((u8Op >> 3) & 0x07)));
					break;
				}
				case 0xc6:
				case 0xce:
				case 0xd6:
				case 0xde:
				case 0xe6:
				case 0xee:
				case 0xf6:
				case 0xfe:			// SET 0-7, (Ix+n)
				{
					MZ80WriteByte(psCPU, u16Addr, u8Data | (1 << ((u8Op >> 3) & 0x07)));
					break;
				}
				default:
				{
					char u8String[100];
					UINT32 u32ExitCode;

					u32ExitCode = (UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase;
					pu8PC--; 	// Back up two instructions since it's not valid
#ifdef _WIN32
					if (sg_psTraceFile)
					{
						fclose(sg_psTraceFile);
					}
#endif
					sprintf(u8String, "Unknown DD/FD CB opcode at 0x%.4x: 0x%.2x", u32ExitCode, *(pu8PC));
					BASSERT_MSG(u8String);
				}
			}

			break;
		}
		case 0xe1:			// 0xfd 0xe1 - POP (Ix)
		{
			POPWORD(*pu16Index);
			break;
		}
		case 0xe3:			// 0xfd 0xe3 - EX (SP), Ix
		{
			UINT16 u16Temp;

			u16Temp = MZ80ReadByte(psCPU, psCPU->z80sp) + (MZ80ReadByte(psCPU, psCPU->z80sp + 1) << 8);
			MZ80WriteByte(psCPU, psCPU->z80sp, (UINT8) *pu16Index);
			MZ80WriteByte(psCPU, psCPU->z80sp + 1, (UINT8) (*pu16Index >> 8));
			*pu16Index = u16Temp;
			break;
		}
		case 0xe5:			// 0xfd 0xe5 - PUSH (Ix)
		{
			PUSHWORD(*pu16Index);
			break;
		}
		case 0xe9:			// 0xfd 0xe9 - JP (Ix)
		{
			pu8PC = psCPU->pu8CodeBase + *pu16Index;
			break;
		}
		case 0xf9:			// 0xfd 0xf9 - LD SP, Ix
		{
			psCPU->z80sp = *pu16Index;
			break;
		}
		default:
		{
			UINT32 u32ExitCode;
			char u8String[100];

			pu8PC -= 2; 	// Back up two instructions since it's not valid
			u32ExitCode = ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
			sprintf(u8String, "Unknown DD/FD opcode at 0x%.4x: 0x%.2x", u32ExitCode, *(pu8PC + 1));
#ifdef _WIN32
			if (sg_psTraceFile)
			{
				fclose(sg_psTraceFile);
			}
#endif
			BASSERT_MSG(u8String);
		}
	}

	return(pu8PC);
}

static BOOL sg_bPrintPC;

// Time to execute some cycles
UINT32 MZ80Exec(register CONTEXTMZ80 *psCPU,
				INT32 s32Cycles)
{
	register UINT8 *pu8PC;
	UINT8 u8Op;			// Used for fetching opcodes
	UINT32 u32ExitCode = CPU_EXEC_OK;
	INT32 s32InterruptTiming = 0;
	BOOL bHandleIRQ = FALSE;

	// Code fetch area
	pu8PC = psCPU->pu8CodeBase + psCPU->z80pc;

#ifdef _WIN32
	if ((NULL == sg_psTraceFile) && (sg_bTracing))
	{
		sg_psTraceFile = fopen(MZ80_TRACE_FILE, "w");
	}
#endif

	// If the CPU is halted, give it up
	if (psCPU->bHalted)
	{
		goto haltedState;
	}

	while (1)
	{
		UINT16 u16PC;
		
		// Outer loop is for interrupt handling

		while (s32Cycles > 0)
		{
			char u8Operation[100];
			char u8Argument[100];
			char u8OpcodeBytes[100];

execOneMore:
			INC_R(psCPU);
			if (sg_bTracing)
			{
/*
				(void) z80DisassembleOne((UINT32)((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase),
										 pu8PC,
										 u8OpcodeBytes,
										 u8Operation,
										 u8Argument);

				fprintf(sg_psTraceFile, "PC=%.4x AF=%.4x BC=%.4x DE=%.4x HL=%.4x IX=%.4x SP=%.4x %s %s\n", (UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase, 
								psCPU->z80AF, psCPU->z80BC, psCPU->z80DE, psCPU->z80HL, psCPU->z80IX, psCPU->z80sp, u8Operation, u8Argument);
				fflush(sg_psTraceFile); */
			}

			u16PC = (UINT16) ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
			if (0x18e8 == u16PC)
			{
//				sg_bTracing = TRUE;
			}

			if (sg_bTracing)
			{
				(void) z80DisassembleOne((UINT32)((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase),
										 pu8PC,
										 u8OpcodeBytes,
										 u8Operation,
										 u8Argument);
				DebugOut("PC=%.4x AF=%.4x BC=%.4x DE=%.4x HL=%.4x IX=%.4x IY=%.4x SP=%.4x %s %s\n", (UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase, 
								psCPU->z80AF, psCPU->z80BC, psCPU->z80DE, psCPU->z80HL, psCPU->z80IX, psCPU->z80IY, psCPU->z80sp, u8Operation, u8Argument);
			}

			u8Op = *pu8PC;
			pu8PC++;

			// Subtract it from the overall count
			psCPU->u32ClockTicks += sg_u8Z80MainCycleCounts[u8Op];
			s32Cycles -= (INT32) sg_u8Z80MainCycleCounts[u8Op];

			switch(u8Op)
			{
				case 0x00:					// 0x00 - NOP
				{
					break;
				}
				case 0x01:					// 0x01 - LD BC, nn
				{
					PC_WORD_FETCH(psCPU->z80BC);
					break;
				}
				case 0x02:					// 0x02 - LD (BC), A
				{
					MZ80WriteByte(psCPU, psCPU->z80BC, psCPU->z80A);
					break;
				}
				case 0x03:					// 0x03 - INC BC
				{
					psCPU->z80BC++;
					break;
				}
				case 0x04:					// 0x04 - INC B
				{
					MZ80INC8(psCPU, &psCPU->z80B);
					break;
				}
				case 0x05:					// 0x05 - DEC B
				{
					MZ80DEC8(psCPU, &psCPU->z80B);
					break;
				}
				case 0x06:					// 0x06 - LD B, n
				{
					PC_BYTE_FETCH(psCPU->z80B);
					break;
				}
				case 0x07:					// 0x07 - RLCA
				{
					psCPU->z80A = (psCPU->z80A >> 7) | (psCPU->z80A << 1);
					// Preserve only sign, zero, and parity
					psCPU->z80F = (psCPU->z80F & (Z80F_S | Z80F_Z | Z80F_PV)) | (psCPU->z80A & Z80F_C);
					break;
				}
				case 0x08:					// 0x08 - EX AF, AF'
				{
					UINT16 u16Temp;

					u16Temp = psCPU->z80AF;
					psCPU->z80AF = psCPU->z80afprime;
					psCPU->z80afprime = u16Temp;

					break;
				}
				case 0x09:					// 0x09 - ADD HL, BC
				{
					psCPU->z80HL = MZ80ADD16(psCPU, psCPU->z80HL, psCPU->z80BC);
					break;
				}
				case 0x0a:					// 0x0a - LD A,(BC)
				{
					psCPU->z80A = MZ80ReadByte(psCPU, psCPU->z80BC);
					break;
				}
				case 0x0b:					// 0x0b - DEC BC
				{
					psCPU->z80BC--;
					break;
				}
				case 0x0c:					// 0x0c - INC C
				{
					MZ80INC8(psCPU, &psCPU->z80C);
					break;
				}
				case 0x0d:					// 0x0d - DEC C
				{
					MZ80DEC8(psCPU, &psCPU->z80C);
					break;
				}
				case 0x0e:					// 0x0e - LD C, n
				{
					PC_BYTE_FETCH(psCPU->z80C);
					break;
				}
				case 0x0f:					// 0x0f - RRCA
				{
					// Preserve only sign, zero, and parity
					psCPU->z80F = (psCPU->z80F & (Z80F_S | Z80F_Z | Z80F_PV)) | (psCPU->z80A & Z80F_C);
					psCPU->z80A = (psCPU->z80A >> 1) | (psCPU->z80A << 7);
					break;
				}
				case 0x10:					// 0x10 - DJNZ xx
				{
					psCPU->z80B--;
					if (psCPU->z80B)
					{
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
						PC_REL_IMM();
					}
					else
					{
						++pu8PC;
					}
					break;
				}
				case 0x11:					// 0x11 - LD DE, nn
				{
					PC_WORD_FETCH(psCPU->z80DE);
					break;
				}
				case 0x12:					// 0x12 - LD (DE), A
				{
					MZ80WriteByte(psCPU, psCPU->z80DE, psCPU->z80A);
					break;
				}
				case 0x13:					// 0x13 - INC DE
				{
					psCPU->z80DE++;
					break;
				}
				case 0x14:					// 0x14 - INC D
				{
					MZ80INC8(psCPU, &psCPU->z80D);
					break;
				}
				case 0x15:					// 0x15 - DEC D
				{
					MZ80DEC8(psCPU, &psCPU->z80D);
					break;
				}
				case 0x16:					// 0x16 - LD D, n
				{
					PC_BYTE_FETCH(psCPU->z80D);
					break;
				}
				case 0x17:					// 0x17 - RLA
				{
					UINT8 u8Temp = psCPU->z80F & Z80F_C;
					psCPU->z80F = (psCPU->z80F & ~(Z80F_H | Z80F_N | Z80F_C)) | ((psCPU->z80A >> 7) & Z80F_C);
					psCPU->z80A = (psCPU->z80A << 1) | u8Temp;
					break;
				}
				case 0x18:					// 0x18 - JR xx
				{
					PC_REL_IMM();
					break;
				}
				case 0x19:					// 0x19 - ADD HL, DE
				{
					psCPU->z80HL = MZ80ADD16(psCPU, psCPU->z80HL, psCPU->z80DE);
					break;
				}
				case 0x1a:					// 0x1a - LD A,(DE)
				{
					psCPU->z80A = MZ80ReadByte(psCPU, psCPU->z80DE);
					break;
				}
				case 0x1b:					// 0x1b - DEC DE
				{
					psCPU->z80DE--;
					break;
				}
				case 0x1c:					// 0x1C - INC E
				{
					MZ80INC8(psCPU, &psCPU->z80E);
					break;
				}
				case 0x1d:					// 0x1d - DEC E
				{
					MZ80DEC8(psCPU, &psCPU->z80E);
					break;
				}
				case 0x1e:					// 0x1e - LD E, n
				{
					PC_BYTE_FETCH(psCPU->z80E);
					break;
				}
				case 0x1f:					// 0x1f - RRA
				{
					UINT8 u8Temp = (psCPU->z80F & Z80F_C) << 7;
					psCPU->z80F = (psCPU->z80F & ~(Z80F_C | Z80F_N | Z80F_H)) | (psCPU->z80A & Z80F_C);
					psCPU->z80A = (psCPU->z80A >> 1) | u8Temp;
					break;
				}
				case 0x20:					// 0x20 - JR NZ, n
				{
					if (0 == (psCPU->z80F & Z80F_Z))
					{
						PC_REL_IMM();
						s32Cycles -= 5;		// 5 Extra T states when the jump is taken
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past the relative byte
						++pu8PC;
					}
					break;
				}
				case 0x21:					// 0x21 - LD HL, nn
				{
					PC_WORD_FETCH(psCPU->z80HL);
					break;
				}
				case 0x22:					// 0x22 - LD (nn), HL
				{
					UINT16 u16Temp;
					PC_WORD_FETCH(u16Temp);
					MZ80WriteByte(psCPU, u16Temp++, psCPU->z80L);
					MZ80WriteByte(psCPU, u16Temp, psCPU->z80H);
					break;
				}
				
				case 0x23:					// 0x23 - INC HL
				{
					psCPU->z80HL++;
					break;
				}
				case 0x24:					// 0x24 - INC H
				{
					MZ80INC8(psCPU, &psCPU->z80H);
					break;
				}
				case 0x25:					// 0x25 - DEC H
				{
					MZ80DEC8(psCPU, &psCPU->z80H);
					break;
				}
				case 0x26:					// 0x26 - LD H, n
				{
					PC_BYTE_FETCH(psCPU->z80H);
					break;
				}
				case 0x27:					// 0x27 - DAA
				{
					UINT16 u16Index;

					u16Index = ((((psCPU->z80F & Z80F_C) |
								  ((psCPU->z80F & Z80F_H) >> 3) |
								  ((psCPU->z80F & Z80F_N) << 1)) << 8) | psCPU->z80A);
					psCPU->z80F = sg_u16DAATable[u16Index] >> 8;
					psCPU->z80A = (UINT8) sg_u16DAATable[u16Index];
					break;
				}
				case 0x28:					// 0x28 - JR Z, n
				{
					if (psCPU->z80F & Z80F_Z)
					{
						PC_REL_IMM();
						s32Cycles -= 5;		// 5 Extra T states when the jump is taken
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past the relative byte
						++pu8PC;
					}
					break;
				}
				case 0x29:					// 0x29 - ADD HL, HL
				{
					psCPU->z80HL = MZ80ADD16(psCPU, psCPU->z80HL, psCPU->z80HL);
					break;
				}
				case 0x2a:					// 0x2a - LD HL, (nn)
				{
					UINT16 u16Temp;
					PC_WORD_FETCH(u16Temp);
					psCPU->z80L = MZ80ReadByte(psCPU, u16Temp++);
					psCPU->z80H = MZ80ReadByte(psCPU, u16Temp);
					break;
				}
				case 0x2b:					// 0x2b - DEC HL
				{
					psCPU->z80HL--;
					break;
				}
				case 0x2c:					// 0x2c - INC L
				{
					MZ80INC8(psCPU, &psCPU->z80L);
					break;
				}
				case 0x2d:					// 0x2d - DEC L
				{
					MZ80DEC8(psCPU, &psCPU->z80L);
					break;
				}
				case 0x2e:					// 0x2e - LD L, n
				{
					PC_BYTE_FETCH(psCPU->z80L);
					break;
				}
				case 0x2f:					// 0x2f - CPL
				{
					psCPU->z80A = 0xff - psCPU->z80A;
					psCPU->z80F |= (Z80F_H | Z80F_N);
					break;
				}
				case 0x30:					// 0x38 - JR NC, n
				{
					if (0 == (psCPU->z80F & Z80F_C))
					{
						PC_REL_IMM();
						s32Cycles -= 5;		// 5 Extra T states when the jump is taken
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past the relative byte
						++pu8PC;
					}
					break;
				}
				case 0x31:					// 0x31 - LD SP, nn
				{
					PC_WORD_FETCH(psCPU->z80sp);
					break;
				}
				case 0x32:					// 0x32 nn nn - LD (nn), A
				{
					UINT16 u16Temp;
					PC_WORD_FETCH(u16Temp);
					MZ80WriteByte(psCPU, u16Temp, psCPU->z80A);
					break;
				}
				case 0x33:					// 0x33 - INC SP
				{
					psCPU->z80sp++;
					break;
				}
				case 0x34:					// 0x34 - INC (HL)
				{
					UINT8 u8Data;
					u8Data = MZ80ReadByte(psCPU, psCPU->z80HL);
					MZ80INC8(psCPU, &u8Data);
					MZ80WriteByte(psCPU, psCPU->z80HL, u8Data);
					break;
				}
				case 0x35:					// 0x35 - DEC (HL)
				{
					UINT8 u8Data;
					u8Data = MZ80ReadByte(psCPU, psCPU->z80HL);
					MZ80DEC8(psCPU, &u8Data);
					MZ80WriteByte(psCPU, psCPU->z80HL, u8Data);
					break;
				}
				case 0x36:					// 0x36 - LD (HL), n
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, *pu8PC);
					++pu8PC;
					break;
				}
				case 0x37:					// 0x37 - SCF
				{
					psCPU->z80F = (psCPU->z80F & ~ (Z80F_N | Z80F_H)) | Z80F_C;
					break;
				}
				case 0x38:					// 0x38 - JR C, n
				{
					if (psCPU->z80F & Z80F_C)
					{
						PC_REL_IMM();
						s32Cycles -= 5;		// 5 Extra T states when the jump is taken
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past the relative byte
						++pu8PC;
					}
					break;
				}
				case 0x39:					// 0x39 - ADD HL, SP
				{
					psCPU->z80HL = MZ80ADD16(psCPU, psCPU->z80HL, psCPU->z80sp);
					break;
				}
				case 0x3a:					// 0x3a - LD A, (nn)
				{
					psCPU->z80A = MZ80ReadByte(psCPU, *pu8PC + (*(pu8PC + 1) << 8));
					pu8PC += 2;
					break;
				}
				case 0x3b:					// 0x3b - DEC SP
				{
					psCPU->z80sp--;
					break;
				}
				case 0x3c:					// 0x3c - INC A
				{
					MZ80INC8(psCPU, &psCPU->z80A);
					break;
				}
				case 0x3d:					// 0x3d - DEC A
				{
					MZ80DEC8(psCPU, &psCPU->z80A);
					break;
				}
				case 0x3e:					// 0x3e - LD A, n
				{
					PC_BYTE_FETCH(psCPU->z80A);
					break;
				}
				case 0x3f:					// 0x3f - CCF
				{
					psCPU->z80F = (psCPU->z80F & ~(Z80F_N | Z80F_H)) | ((psCPU->z80F & Z80F_C) << 4);
					psCPU->z80F ^= Z80F_C;
					break;
				}
				case 0x40:	// 0x40 - LD B, B
				{
					psCPU->z80B = psCPU->z80B;
					break;
				}
				case 0x41:	// 0x41 - LD B, C
				{
					psCPU->z80B = psCPU->z80C;
					break;
				}
				case 0x42:	// 0x42 - LD B, D
				{
					psCPU->z80B = psCPU->z80D;
					break;
				}
				case 0x43:	// 0x43 - LD B, E
				{
					psCPU->z80B = psCPU->z80E;
					break;
				}
				case 0x44:	// 0x44 - LD B, H
				{
					psCPU->z80B = psCPU->z80H;
					break;
				}
				case 0x45:	// 0x45 - LD B, L
				{
					psCPU->z80B = psCPU->z80L;
					break;
				}
				case 0x46:	// 0x46 - LD B, (HL)
				{
					psCPU->z80B = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x47:	// 0x47 - LD B, A
				{
					psCPU->z80B = psCPU->z80A;
					break;
				}
				case 0x48:	// 0x48 - LD C, B
				{
					psCPU->z80C = psCPU->z80B;
					break;
				}
				case 0x49:	// 0x49 - LD C, C
				{
					psCPU->z80C = psCPU->z80C;
					break;
				}
				case 0x4a:	// 0x4a - LD C, D
				{
					psCPU->z80C = psCPU->z80D;
					break;
				}
				case 0x4b:	// 0x4b - LD C, E
				{
					psCPU->z80C = psCPU->z80E;
					break;
				}
				case 0x4c:	// 0x4c - LD C, H
				{
					psCPU->z80C = psCPU->z80H;
					break;
				}
				case 0x4d:	// 0x4d - LD C, L
				{
					psCPU->z80C = psCPU->z80L;
					break;
				}
				case 0x4e:	// 0x4e - LD C, (HL)
				{
					psCPU->z80C = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x4f:	// 0x4f - LD C, A
				{
					psCPU->z80C = psCPU->z80A;
					break;
				}
				case 0x50:	// 0x50 - LD D, B
				{
					psCPU->z80D = psCPU->z80B;
					break;
				}
				case 0x51:	// 0x51 - LD D, C
				{
					psCPU->z80D = psCPU->z80C;
					break;
				}
				case 0x52:	// 0x52 - LD D, D
				{
					psCPU->z80D = psCPU->z80D;
					break;
				}
				case 0x53:	// 0x53 - LD D, E
				{
					psCPU->z80D = psCPU->z80E;
					break;
				}
				case 0x54:	// 0x54 - LD D, H
				{
					psCPU->z80D = psCPU->z80H;
					break;
				}
				case 0x55:	// 0x55 - LD D, L
				{
					psCPU->z80D = psCPU->z80L;
					break;
				}
				case 0x56:	// 0x56 - LD D, (HL)
				{
					psCPU->z80D = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x57:	// 0x57 - LD D, A
				{
					psCPU->z80D = psCPU->z80A;
					break;
				}
				case 0x58:	// 0x58 - LD E, B
				{
					psCPU->z80E = psCPU->z80B;
					break;
				}
				case 0x59:	// 0x59 - LD E, C
				{
					psCPU->z80E = psCPU->z80C;
					break;
				}
				case 0x5a:	// 0x5a - LD E, D
				{
					psCPU->z80E = psCPU->z80D;
					break;
				}
				case 0x5b:	// 0x5b - LD E, E
				{
					psCPU->z80E = psCPU->z80E;
					break;
				}
				case 0x5c:	// 0x5c - LD E, H
				{
					psCPU->z80E = psCPU->z80H;
					break;
				}
				case 0x5d:	// 0x5d - LD E, L
				{
					psCPU->z80E = psCPU->z80L;
					break;
				}
				case 0x5e:	// 0x5e - LD E, (HL)
				{
					psCPU->z80E = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x5f:	// 0x5f - LD E, A
				{
					psCPU->z80E = psCPU->z80A;
					break;
				}
				case 0x60:	// 0x60 - LD H, B
				{
					psCPU->z80H = psCPU->z80B;
					break;
				}
				case 0x61:	// 0x61 - LD H, C
				{
					psCPU->z80H = psCPU->z80C;
					break;
				}
				case 0x62:	// 0x62 - LD H, D
				{
					psCPU->z80H = psCPU->z80D;
					break;
				}
				case 0x63:	// 0x63 - LD H, E
				{
					psCPU->z80H = psCPU->z80E;
					break;
				}
				case 0x64:	// 0x64 - LD H, H
				{
					psCPU->z80H = psCPU->z80H;
					break;
				}
				case 0x65:	// 0x65 - LD H, L
				{
					psCPU->z80H = psCPU->z80L;
					break;
				}
				case 0x66:	// 0x66 - LD H, (HL)
				{
					psCPU->z80H = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x67:	// 0x67 - LD H, A
				{
					psCPU->z80H = psCPU->z80A;
					break;
				}
				case 0x68:	// 0x68 - LD L, B
				{
					psCPU->z80L = psCPU->z80B;
					break;
				}
				case 0x69:	// 0x69 - LD L, C
				{
					psCPU->z80L = psCPU->z80C;
					break;
				}
				case 0x6a:	// 0x6a - LD L, D
				{
					psCPU->z80L = psCPU->z80D;
					break;
				}
				case 0x6b:	// 0x6b - LD L, E
				{
					psCPU->z80L = psCPU->z80E;
					break;
				}
				case 0x6c:	// 0x6c - LD L, H
				{
					psCPU->z80L = psCPU->z80H;
					break;
				}
				case 0x6d:	// 0x6d - LD L, L
				{
					psCPU->z80L = psCPU->z80L;
					break;
				}
				case 0x6e:	// 0x6e - LD L, (HL)
				{
					psCPU->z80L = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x6f:	// 0x6f - LD L, A
				{
					psCPU->z80L = psCPU->z80A;
					break;
				}
				case 0x70:	// 0x70 - LD (HL), B
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80B);
					break;
				}
				case 0x71:	// 0x71 - LD (HL), C
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80C);
					break;
				}
				case 0x72:	// 0x72 - LD (HL), D
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80D);
					break;
				}
				case 0x73:	// 0x73 - LD (HL), E
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80E);
					break;
				}
				case 0x74:	// 0x74 - LD (HL), H
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80H);
					break;
				}
				case 0x75:	// 0x75 - LD (HL), L
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80L);
					break;
				}
				case 0x76:	// 0x76 - HALT
				{
					pu8PC--;				// Back up on top of the halt instruction like we're supposed to
					psCPU->bHalted = TRUE;	// Note that we're halted
					goto haltedState;
					break;
				}
				case 0x77:	// 0x77 - LD (HL), A
				{
					MZ80WriteByte(psCPU, psCPU->z80HL, psCPU->z80A);
					break;
				}
				case 0x78:	// 0x78 - LD A, B
				{
					psCPU->z80A = psCPU->z80B;
					break;
				}
				case 0x79:	// 0x79 - LD A, C
				{
					psCPU->z80A = psCPU->z80C;
					break;
				}
				case 0x7a:	// 0x7a - LD A, D
				{
					psCPU->z80A = psCPU->z80D;
					break;
				}
				case 0x7b:	// 0x7b - LD A, E
				{
					psCPU->z80A = psCPU->z80E;
					break;
				}
				case 0x7c:	// 0x7c - LD A, H
				{
					psCPU->z80A = psCPU->z80H;
					break;
				}
				case 0x7d:	// 0x7d - LD A, L
				{
					psCPU->z80A = psCPU->z80L;
					break;
				}
				case 0x7e:	// 0x7e - LD A, (HL)
				{
					psCPU->z80A = MZ80ReadByte(psCPU, psCPU->z80HL);
					break;
				}
				case 0x7f:	// 0x7f - LD A, A
				{
					psCPU->z80A = psCPU->z80A;
					break;
				}
				case 0x80:	// 0x80 - ADD A, B
				{
					MZ80ADDA(psCPU, psCPU->z80B);
					break;
				}
				case 0x81:	// 0x81 - ADD A, C
				{
					MZ80ADDA(psCPU, psCPU->z80C);
					break;
				}
				case 0x82:	// 0x82 - ADD A, D
				{
					MZ80ADDA(psCPU, psCPU->z80D);
					break;
				}
				case 0x83:	// 0x83 - ADD A, E
				{
					MZ80ADDA(psCPU, psCPU->z80E);
					break;
				}
				case 0x84:	// 0x84 - ADD A, H
				{
					MZ80ADDA(psCPU, psCPU->z80H);
					break;
				}
				case 0x85:	// 0x85 - ADD A, L
				{
					MZ80ADDA(psCPU, psCPU->z80L);
					break;
				}
				case 0x86:	// 0x86 - ADD A, (HL)
				{
					MZ80ADDA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0x87:	// 0x87 - ADD A, A
				{
					MZ80ADDA(psCPU, psCPU->z80A);
					break;
				}
				case 0x88:	// 0x88 - ADC A, B
				{
					MZ80ADCA(psCPU, psCPU->z80B);
					break;
				}
				case 0x89:	// 0x89 - ADC A, C
				{
					MZ80ADCA(psCPU, psCPU->z80C);
					break;
				}
				case 0x8a:	// 0x8a - ADC A, D
				{
					MZ80ADCA(psCPU, psCPU->z80D);
					break;
				}
				case 0x8b:	// 0x8b - ADC A, E
				{
					MZ80ADCA(psCPU, psCPU->z80E);
					break;
				}
				case 0x8c:	// 0x8c - ADC A, H
				{
					MZ80ADCA(psCPU, psCPU->z80H);
					break;
				}
				case 0x8d:	// 0x8d - ADC A, L
				{
					MZ80ADCA(psCPU, psCPU->z80L);
					break;
				}
				case 0x8e:	// 0x8e - ADC A, (HL)
				{
					MZ80ADCA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0x8f:	// 0x8f - ADC A, A
				{
					MZ80ADCA(psCPU, psCPU->z80A);
					break;
				}
				case 0x90:	// 0x90 - SUB A, B
				{
					MZ80SUBA(psCPU, psCPU->z80B);
					break;
				}
				case 0x91:	// 0x91 - SUB A, C
				{
					MZ80SUBA(psCPU, psCPU->z80C);
					break;
				}
				case 0x92:	// 0x92 - SUB A, D
				{
					MZ80SUBA(psCPU, psCPU->z80D);
					break;
				}
				case 0x93:	// 0x93 - SUB A, E
				{
					MZ80SUBA(psCPU, psCPU->z80E);
					break;
				}
				case 0x94:	// 0x94 - SUB A, H
				{
					MZ80SUBA(psCPU, psCPU->z80H);
					break;
				}
				case 0x95:	// 0x95 - SUB A, L
				{
					MZ80SUBA(psCPU, psCPU->z80L);
					break;
				}
				case 0x96:	// 0x96 - SUB A, (HL)
				{
					MZ80SUBA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0x97:	// 0x97 - SUB A, A
				{
					MZ80SUBA(psCPU, psCPU->z80A);
					break;
				}
				case 0x98:	// 0x98 - SBC A, B
				{
					MZ80SBCA(psCPU, psCPU->z80B);
					break;
				}
				case 0x99:	// 0x99 - SBC A, C
				{
					MZ80SBCA(psCPU, psCPU->z80C);
					break;
				}
				case 0x9a:	// 0x9a - SBC A, D
				{
					MZ80SBCA(psCPU, psCPU->z80D);
					break;
				}
				case 0x9b:	// 0x9b - SBC A, E
				{
					MZ80SBCA(psCPU, psCPU->z80E);
					break;
				}
				case 0x9c:	// 0x9c - SBC A, H
				{
					MZ80SBCA(psCPU, psCPU->z80H);
					break;
				}
				case 0x9d:	// 0x9d - SBC A, L
				{
					MZ80SBCA(psCPU, psCPU->z80L);
					break;
				}
				case 0x9e:	// 0x9e - SBC A, (HL)
				{
					MZ80SBCA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0x9f:	// 0x9f - SBC A, A
				{
					MZ80SBCA(psCPU, psCPU->z80A);
					break;
				}
				case 0xa0:	// 0xa0 - AND A, B
				{
					MZ80ANDA(psCPU, psCPU->z80B);
					break;
				}
				case 0xa1:	// 0xa1 - AND A, C
				{
					MZ80ANDA(psCPU, psCPU->z80C);
					break;
				}
				case 0xa2:	// 0xa2 - AND A, D
				{
					MZ80ANDA(psCPU, psCPU->z80D);
					break;
				}
				case 0xa3:	// 0xa3 - AND A, E
				{
					MZ80ANDA(psCPU, psCPU->z80E);
					break;
				}
				case 0xa4:	// 0xa4 - AND A, H
				{
					MZ80ANDA(psCPU, psCPU->z80H);
					break;
				}
				case 0xa5:	// 0xa5 - AND A, L
				{
					MZ80ANDA(psCPU, psCPU->z80L);
					break;
				}
				case 0xa6:	// 0xa6 - AND A, (HL)
				{
					MZ80ANDA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0xa7:	// 0xa7 - AND A, A
				{
					MZ80ANDA(psCPU, psCPU->z80A);
					break;
				}
				case 0xa8:	// 0xa8 - XOR A, B
				{
					MZ80XORA(psCPU, psCPU->z80B);
					break;
				}
				case 0xa9:	// 0xa9 - XOR A, C
				{
					MZ80XORA(psCPU, psCPU->z80C);
					break;
				}
				case 0xaa:	// 0xaa - XOR A, D
				{
					MZ80XORA(psCPU, psCPU->z80D);
					break;
				}
				case 0xab:	// 0xab - XOR A, E
				{
					MZ80XORA(psCPU, psCPU->z80E);
					break;
				}
				case 0xac:	// 0xac - XOR A, H
				{
					MZ80XORA(psCPU, psCPU->z80H);
					break;
				}
				case 0xad:	// 0xad - XOR A, L
				{
					MZ80XORA(psCPU, psCPU->z80L);
					break;
				}
				case 0xae:	// 0xae - XOR A, (HL)
				{
					MZ80XORA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0xaf:	// 0xaf - XOR A, A
				{
					MZ80XORA(psCPU, psCPU->z80A);
					break;
				}
				case 0xb0:	// 0xb0 - OR A, B
				{
					MZ80ORA(psCPU, psCPU->z80B);
					break;
				}
				case 0xb1:	// 0xb1 - OR A, C
				{
					MZ80ORA(psCPU, psCPU->z80C);
					break;
				}
				case 0xb2:	// 0xb2 - OR A, D
				{
					MZ80ORA(psCPU, psCPU->z80D);
					break;
				}
				case 0xb3:	// 0xb3 - OR A, E
				{
					MZ80ORA(psCPU, psCPU->z80E);
					break;
				}
				case 0xb4:	// 0xb4 - OR A, H
				{
					MZ80ORA(psCPU, psCPU->z80H);
					break;
				}
				case 0xb5:	// 0xb5 - OR A, L
				{
					MZ80ORA(psCPU, psCPU->z80L);
					break;
				}
				case 0xb6:	// 0xb6 - OR A, (HL)
				{
					MZ80ORA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0xb7:	// 0xb7 - OR A, A
				{
					MZ80ORA(psCPU, psCPU->z80A);
					break;
				}
				case 0xb8:	// 0xb8 - CP A, B
				{
					MZ80CPA(psCPU, psCPU->z80B);
					break;
				}
				case 0xb9:	// 0xb9 - CP A, C
				{
					MZ80CPA(psCPU, psCPU->z80C);
					break;
				}
				case 0xba:	// 0xba - CP A, D
				{
					MZ80CPA(psCPU, psCPU->z80D);
					break;
				}
				case 0xbb:	// 0xbb - CP A, E
				{
					MZ80CPA(psCPU, psCPU->z80E);
					break;
				}
				case 0xbc:	// 0xbc - CP A, H
				{
					MZ80CPA(psCPU, psCPU->z80H);
					break;
				}
				case 0xbd:	// 0xbd - CP A, L
				{
					MZ80CPA(psCPU, psCPU->z80L);
					break;
				}
				case 0xbe:	// 0xbe - CP A, (HL)
				{
					MZ80CPA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL));
					break;
				}
				case 0xbf:	// 0xbf - CP A, A
				{
					MZ80CPA(psCPU, psCPU->z80A);
					break;
				}
				case 0xc0:					// 0xc0 - RET NZ
				{
					if (0 == (psCPU->z80F & Z80F_Z))
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xc1:				// 0xe1 - POP BC
				{
					POPWORD(psCPU->z80BC);
					break;
				}
				case 0xc2:					// 0xc2 - JP NZ, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_Z))
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xc3:					// 0xc3 - JP (nn)
				{
					PC_ABS_JMP();
					break;
				}
				case 0xc4:					// 0xc4 - CALL NZ, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_Z))
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				case 0xc5:				// 0xc5 - PUSH BC
				{
					PUSHWORD(psCPU->z80BC);
					break;
				}
				case 0xc6:				// 0xc6 - ADD A, n
				{
					MZ80ADDA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xc7:				// 0xc7 - RST 00h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xc8:					// 0xc8 - RET Z
				{
					if (psCPU->z80F & Z80F_Z)
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xc9:				// 0xc9 - RET
				{
					UINT16 u16Temp;

					POPWORD(u16Temp);
					pu8PC = psCPU->pu8CodeBase + u16Temp;
					break;
				}
				case 0xca:					// 0xc - JP Z, (nn)
				{
					if (psCPU->z80F & Z80F_Z)
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}

				// ***** CB INSTRUCTIONS *****
				case 0xcb:					// 0xcb xx - Instructions
				{
					u8Op = *pu8PC;
					pu8PC++;

					INC_R(psCPU);
					psCPU->u32ClockTicks += sg_u8Z80CBCycleCounts[u8Op];
					s32Cycles -= (INT32) sg_u8Z80CBCycleCounts[u8Op];

					switch (u8Op)
					{
						case 0x00:	// 0xcb 0x00 - RLC B
						{
							psCPU->z80B = MZ80RLC(psCPU, psCPU->z80B);
							break;
						}

						case 0x01:	// 0xcb 0x01 - RLC C
						{
							psCPU->z80C = MZ80RLC(psCPU, psCPU->z80C);
							break;
						}

						case 0x02:	// 0xcb 0x02 - RLC D
						{
							psCPU->z80D = MZ80RLC(psCPU, psCPU->z80D);
							break;
						}

						case 0x03:	// 0xcb 0x03 - RLC E
						{
							psCPU->z80E = MZ80RLC(psCPU, psCPU->z80E);
							break;
						}

						case 0x04:	// 0xcb 0x04 - RLC H
						{
							psCPU->z80H = MZ80RLC(psCPU, psCPU->z80H);
							break;
						}

						case 0x05:	// 0xcb 0x05 - RLC L
						{
							psCPU->z80L = MZ80RLC(psCPU, psCPU->z80L);
							break;
						}

						case 0x06:	// 0xcb 0x06 - RLC (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80RLC(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x07:	// 0xcb 0x07 - RLC A
						{
							psCPU->z80A = MZ80RLC(psCPU, psCPU->z80A);
							break;
						}

						case 0x08:	// 0xcb 0x08 - RRC B
						{
							psCPU->z80B = MZ80RRC(psCPU, psCPU->z80B);
							break;
						}

						case 0x09:	// 0xcb 0x09 - RRC C
						{
							psCPU->z80C = MZ80RRC(psCPU, psCPU->z80C);
							break;
						}

						case 0x0a:	// 0xcb 0x0a - RRC D
						{
							psCPU->z80D = MZ80RRC(psCPU, psCPU->z80D);
							break;
						}

						case 0x0b:	// 0xcb 0x0b - RRC E
						{
							psCPU->z80E = MZ80RRC(psCPU, psCPU->z80E);
							break;
						}

						case 0x0c:	// 0xcb 0x0c - RRC H
						{
							psCPU->z80H = MZ80RRC(psCPU, psCPU->z80H);
							break;
						}

						case 0x0d:	// 0xcb 0x0d - RRC L
						{
							psCPU->z80L = MZ80RRC(psCPU, psCPU->z80L);
							break;
						}

						case 0x0e:	// 0xcb 0x0e - RRC (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80RRC(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x0f:	// 0xcb 0x0f - RRC A
						{
							psCPU->z80A = MZ80RRC(psCPU, psCPU->z80A);
							break;
						}

						case 0x10:	// 0xcb 0x10 - RL B
						{
							psCPU->z80B = MZ80RL(psCPU, psCPU->z80B);
							break;
						}

						case 0x11:	// 0xcb 0x11 - RL C
						{
							psCPU->z80C = MZ80RL(psCPU, psCPU->z80C);
							break;
						}

						case 0x12:	// 0xcb 0x12 - RL D
						{
							psCPU->z80D = MZ80RL(psCPU, psCPU->z80D);
							break;
						}

						case 0x13:	// 0xcb 0x13 - RL E
						{
							psCPU->z80E = MZ80RL(psCPU, psCPU->z80E);
							break;
						}

						case 0x14:	// 0xcb 0x14 - RL H
						{
							psCPU->z80H = MZ80RL(psCPU, psCPU->z80H);
							break;
						}

						case 0x15:	// 0xcb 0x15 - RL L
						{
							psCPU->z80L = MZ80RL(psCPU, psCPU->z80L);
							break;
						}

						case 0x16:	// 0xcb 0x16 - RL (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80RL(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x17:	// 0xcb 0x17 - RL A
						{
							psCPU->z80A = MZ80RL(psCPU, psCPU->z80A);
							break;
						}

						case 0x18:	// 0xcb 0x18 - RR B
						{
							psCPU->z80B = MZ80RR(psCPU, psCPU->z80B);
							break;
						}

						case 0x19:	// 0xcb 0x19 - RR C
						{
							psCPU->z80C = MZ80RR(psCPU, psCPU->z80C);
							break;
						}

						case 0x1a:	// 0xcb 0x1a - RR D
						{
							psCPU->z80D = MZ80RR(psCPU, psCPU->z80D);
							break;
						}

						case 0x1b:	// 0xcb 0x1b - RR E
						{
							psCPU->z80E = MZ80RR(psCPU, psCPU->z80E);
							break;
						}

						case 0x1c:	// 0xcb 0x1c - RR H
						{
							psCPU->z80H = MZ80RR(psCPU, psCPU->z80H);
							break;
						}

						case 0x1d:	// 0xcb 0x1d - RR L
						{
							psCPU->z80L = MZ80RR(psCPU, psCPU->z80L);
							break;
						}

						case 0x1e:	// 0xcb 0x1e - RR (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80RR(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x1f:	// 0xcb 0x1f - RR A
						{
							psCPU->z80A = MZ80RR(psCPU, psCPU->z80A);
							break;
						}

						case 0x20:	// 0xcb 0x20 - SLA B
						{
							psCPU->z80B = MZ80SLA(psCPU, psCPU->z80B);
							break;
						}

						case 0x21:	// 0xcb 0x21 - SLA C
						{
							psCPU->z80C = MZ80SLA(psCPU, psCPU->z80C);
							break;
						}

						case 0x22:	// 0xcb 0x22 - SLA D
						{
							psCPU->z80D = MZ80SLA(psCPU, psCPU->z80D);
							break;
						}

						case 0x23:	// 0xcb 0x23 - SLA E
						{
							psCPU->z80E = MZ80SLA(psCPU, psCPU->z80E);
							break;
						}

						case 0x24:	// 0xcb 0x24 - SLA H
						{
							psCPU->z80H = MZ80SLA(psCPU, psCPU->z80H);
							break;
						}

						case 0x25:	// 0xcb 0x25 - SLA L
						{
							psCPU->z80L = MZ80SLA(psCPU, psCPU->z80L);
							break;
						}

						case 0x26:	// 0xcb 0x26 - SLA (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80SLA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x27:	// 0xcb 0x27 - SLA A
						{
							psCPU->z80A = MZ80SLA(psCPU, psCPU->z80A);
							break;
						}

						case 0x28:	// 0xcb 0x28 - SRA B
						{
							psCPU->z80B = MZ80SRA(psCPU, psCPU->z80B);
							break;
						}

						case 0x29:	// 0xcb 0x29 - SRA C
						{
							psCPU->z80C = MZ80SRA(psCPU, psCPU->z80C);
							break;
						}

						case 0x2a:	// 0xcb 0x2a - SRA D
						{
							psCPU->z80D = MZ80SRA(psCPU, psCPU->z80D);
							break;
						}

						case 0x2b:	// 0xcb 0x2b - SRA E
						{
							psCPU->z80E = MZ80SRA(psCPU, psCPU->z80E);
							break;
						}

						case 0x2c:	// 0xcb 0x2c - SRA H
						{
							psCPU->z80H = MZ80SRA(psCPU, psCPU->z80H);
							break;
						}

						case 0x2d:	// 0xcb 0x2d - SRA L
						{
							psCPU->z80L = MZ80SRA(psCPU, psCPU->z80L);
							break;
						}

						case 0x2e:	// 0xcb 0x2e - SRA (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80SRA(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x2f:	// 0xcb 0x2f - SRA A
						{
							psCPU->z80A = MZ80SRA(psCPU, psCPU->z80A);
							break;
						}

						case 0x30:	// 0xcb 0x30 - SLL B
						{
							psCPU->z80B = MZ80SLL(psCPU, psCPU->z80B);
							break;
						}

						case 0x31:	// 0xcb 0x31 - SLL C
						{
							psCPU->z80C = MZ80SLL(psCPU, psCPU->z80C);
							break;
						}

						case 0x32:	// 0xcb 0x32 - SLL D
						{
							psCPU->z80D = MZ80SLL(psCPU, psCPU->z80D);
							break;
						}

						case 0x33:	// 0xcb 0x33 - SLL E
						{
							psCPU->z80E = MZ80SLL(psCPU, psCPU->z80E);
							break;
						}

						case 0x34:	// 0xcb 0x34 - SLL H
						{
							psCPU->z80H = MZ80SLL(psCPU, psCPU->z80H);
							break;
						}

						case 0x35:	// 0xcb 0x35 - SLL L
						{
							psCPU->z80L = MZ80SLL(psCPU, psCPU->z80L);
							break;
						}

						case 0x36:	// 0xcb 0x36 - SLL (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80SLL(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x37:	// 0xcb 0x37 - SLL A
						{
							psCPU->z80A = MZ80SLL(psCPU, psCPU->z80A);
							break;
						}

						case 0x38:	// 0xcb 0x38 - SRL B
						{
							psCPU->z80B = MZ80SRL(psCPU, psCPU->z80B);
							break;
						}

						case 0x39:	// 0xcb 0x39 - SRL C
						{
							psCPU->z80C = MZ80SRL(psCPU, psCPU->z80C);
							break;
						}

						case 0x3a:	// 0xcb 0x3a - SRL D
						{
							psCPU->z80D = MZ80SRL(psCPU, psCPU->z80D);
							break;
						}

						case 0x3b:	// 0xcb 0x3b - SRL E
						{
							psCPU->z80E = MZ80SRL(psCPU, psCPU->z80E);
							break;
						}

						case 0x3c:	// 0xcb 0x3c - SRL H
						{
							psCPU->z80H = MZ80SRL(psCPU, psCPU->z80H);
							break;
						}

						case 0x3d:	// 0xcb 0x3d - SRL L
						{
							psCPU->z80L = MZ80SRL(psCPU, psCPU->z80L);
							break;
						}

						case 0x3e:	// 0xcb 0x3e - SRL (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80SRL(psCPU, MZ80ReadByte(psCPU, psCPU->z80HL)));
							break;
						}

						case 0x3f:	// 0xcb 0x3f - SRL A
						{
							psCPU->z80A = MZ80SRL(psCPU, psCPU->z80A);
							break;
						}

						case 0x40:	// 0xcb 0x40 - BIT 0, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x41:	// 0xcb 0x41 - BIT 0, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x42:	// 0xcb 0x42 - BIT 0, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x43:	// 0xcb 0x43 - BIT 0, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x44:	// 0xcb 0x44 - BIT 0, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x45:	// 0xcb 0x45 - BIT 0, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x46:	// 0xcb 0x46 - BIT 0, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x47:	// 0xcb 0x47 - BIT 0, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x01))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x48:	// 0xcb 0x48 - BIT 1, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x49:	// 0xcb 0x49 - BIT 1, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4a:	// 0xcb 0x4a - BIT 1, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4b:	// 0xcb 0x4b - BIT 1, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4c:	// 0xcb 0x4c - BIT 1, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4d:	// 0xcb 0x4d - BIT 1, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4e:	// 0xcb 0x4e - BIT 1, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x4f:	// 0xcb 0x4f - BIT 1, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x02))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x50:	// 0xcb 0x50 - BIT 2, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x51:	// 0xcb 0x51 - BIT 2, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x52:	// 0xcb 0x52 - BIT 2, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x53:	// 0xcb 0x53 - BIT 2, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x54:	// 0xcb 0x54 - BIT 2, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x55:	// 0xcb 0x55 - BIT 2, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x56:	// 0xcb 0x56 - BIT 2, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x57:	// 0xcb 0x57 - BIT 2, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x04))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x58:	// 0xcb 0x58 - BIT 3, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x59:	// 0xcb 0x59 - BIT 3, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5a:	// 0xcb 0x5a - BIT 3, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5b:	// 0xcb 0x5b - BIT 3, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5c:	// 0xcb 0x5c - BIT 3, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5d:	// 0xcb 0x5d - BIT 3, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5e:	// 0xcb 0x5e - BIT 3, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x5f:	// 0xcb 0x5f - BIT 3, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x08))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x60:	// 0xcb 0x60 - BIT 4, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x61:	// 0xcb 0x61 - BIT 4, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x62:	// 0xcb 0x62 - BIT 4, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x63:	// 0xcb 0x63 - BIT 4, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x64:	// 0xcb 0x64 - BIT 4, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x65:	// 0xcb 0x65 - BIT 4, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x66:	// 0xcb 0x66 - BIT 4, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x67:	// 0xcb 0x67 - BIT 4, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x10))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x68:	// 0xcb 0x68 - BIT 5, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x69:	// 0xcb 0x69 - BIT 5, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6a:	// 0xcb 0x6a - BIT 5, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6b:	// 0xcb 0x6b - BIT 5, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6c:	// 0xcb 0x6c - BIT 5, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6d:	// 0xcb 0x6d - BIT 5, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6e:	// 0xcb 0x6e - BIT 5, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x6f:	// 0xcb 0x6f - BIT 5, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x20))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x70:	// 0xcb 0x70 - BIT 6, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x71:	// 0xcb 0x71 - BIT 6, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x72:	// 0xcb 0x72 - BIT 6, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x73:	// 0xcb 0x73 - BIT 6, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x74:	// 0xcb 0x74 - BIT 6, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x75:	// 0xcb 0x75 - BIT 6, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x76:	// 0xcb 0x76 - BIT 6, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x77:	// 0xcb 0x77 - BIT 6, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x40))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x78:	// 0xcb 0x78 - BIT 7, B
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80B & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x79:	// 0xcb 0x79 - BIT 7, C
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80C & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7a:	// 0xcb 0x7a - BIT 7, D
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80D & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7b:	// 0xcb 0x7b - BIT 7, E
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80E & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7c:	// 0xcb 0x7c - BIT 7, H
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80H & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7d:	// 0xcb 0x7d - BIT 7, L
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80L & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7e:	// 0xcb 0x7e - BIT 7, (HL)
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (MZ80ReadByte(psCPU, psCPU->z80HL) & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x7f:	// 0xcb 0x7f - BIT 7, A
						{
							psCPU->z80F = (psCPU->z80F & Z80F_C) | Z80F_H;	// Preserve only carry and set half carry
							if (0 == (psCPU->z80A & 0x80))
							{
								psCPU->z80F |= (Z80F_Z | Z80F_PV);
							}
							else
							{
								psCPU->z80F |= Z80F_S;
							}
							break;
						}

						case 0x80:	// 0xcb 0x80 - RES 0, B
						{
							psCPU->z80B &= ~0x01;
							break;
						}

						case 0x81:	// 0xcb 0x81 - RES 0, C
						{
							psCPU->z80C &= ~0x01;
							break;
						}

						case 0x82:	// 0xcb 0x82 - RES 0, D
						{
							psCPU->z80D &= ~0x01;
							break;
						}

						case 0x83:	// 0xcb 0x83 - RES 0, E
						{
							psCPU->z80E &= ~0x01;
							break;
						}

						case 0x84:	// 0xcb 0x84 - RES 0, H
						{
							psCPU->z80H &= ~0x01;
							break;
						}

						case 0x85:	// 0xcb 0x85 - RES 0, L
						{
							psCPU->z80L &= ~0x01;
							break;
						}

						case 0x86:	// 0xcb 0x86 - RES 0, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x01);
							break;
						}

						case 0x87:	// 0xcb 0x87 - RES 0, A
						{
							psCPU->z80A &= ~0x01;
							break;
						}

						case 0x88:	// 0xcb 0x88 - RES 1, B
						{
							psCPU->z80B &= ~0x02;
							break;
						}

						case 0x89:	// 0xcb 0x89 - RES 1, C
						{
							psCPU->z80C &= ~0x02;
							break;
						}

						case 0x8a:	// 0xcb 0x8a - RES 1, D
						{
							psCPU->z80D &= ~0x02;
							break;
						}

						case 0x8b:	// 0xcb 0x8b - RES 1, E
						{
							psCPU->z80E &= ~0x02;
							break;
						}

						case 0x8c:	// 0xcb 0x8c - RES 1, H
						{
							psCPU->z80H &= ~0x02;
							break;
						}

						case 0x8d:	// 0xcb 0x8d - RES 1, L
						{
							psCPU->z80L &= ~0x02;
							break;
						}

						case 0x8e:	// 0xcb 0x8e - RES 1, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x02);
							break;
						}

						case 0x8f:	// 0xcb 0x8f - RES 1, A
						{
							psCPU->z80A &= ~0x02;
							break;
						}

						case 0x90:	// 0xcb 0x90 - RES 2, B
						{
							psCPU->z80B &= ~0x04;
							break;
						}

						case 0x91:	// 0xcb 0x91 - RES 2, C
						{
							psCPU->z80C &= ~0x04;
							break;
						}

						case 0x92:	// 0xcb 0x92 - RES 2, D
						{
							psCPU->z80D &= ~0x04;
							break;
						}

						case 0x93:	// 0xcb 0x93 - RES 2, E
						{
							psCPU->z80E &= ~0x04;
							break;
						}

						case 0x94:	// 0xcb 0x94 - RES 2, H
						{
							psCPU->z80H &= ~0x04;
							break;
						}

						case 0x95:	// 0xcb 0x95 - RES 2, L
						{
							psCPU->z80L &= ~0x04;
							break;
						}

						case 0x96:	// 0xcb 0x96 - RES 2, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x04);
							break;
						}

						case 0x97:	// 0xcb 0x97 - RES 2, A
						{
							psCPU->z80A &= ~0x04;
							break;
						}

						case 0x98:	// 0xcb 0x98 - RES 3, B
						{
							psCPU->z80B &= ~0x08;
							break;
						}

						case 0x99:	// 0xcb 0x99 - RES 3, C
						{
							psCPU->z80C &= ~0x08;
							break;
						}

						case 0x9a:	// 0xcb 0x9a - RES 3, D
						{
							psCPU->z80D &= ~0x08;
							break;
						}

						case 0x9b:	// 0xcb 0x9b - RES 3, E
						{
							psCPU->z80E &= ~0x08;
							break;
						}

						case 0x9c:	// 0xcb 0x9c - RES 3, H
						{
							psCPU->z80H &= ~0x08;
							break;
						}

						case 0x9d:	// 0xcb 0x9d - RES 3, L
						{
							psCPU->z80L &= ~0x08;
							break;
						}

						case 0x9e:	// 0xcb 0x9e - RES 3, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x08);
							break;
						}

						case 0x9f:	// 0xcb 0x9f - RES 3, A
						{
							psCPU->z80A &= ~0x08;
							break;
						}

						case 0xa0:	// 0xcb 0xa0 - RES 4, B
						{
							psCPU->z80B &= ~0x10;
							break;
						}

						case 0xa1:	// 0xcb 0xa1 - RES 4, C
						{
							psCPU->z80C &= ~0x10;
							break;
						}

						case 0xa2:	// 0xcb 0xa2 - RES 4, D
						{
							psCPU->z80D &= ~0x10;
							break;
						}

						case 0xa3:	// 0xcb 0xa3 - RES 4, E
						{
							psCPU->z80E &= ~0x10;
							break;
						}

						case 0xa4:	// 0xcb 0xa4 - RES 4, H
						{
							psCPU->z80H &= ~0x10;
							break;
						}

						case 0xa5:	// 0xcb 0xa5 - RES 4, L
						{
							psCPU->z80L &= ~0x10;
							break;
						}

						case 0xa6:	// 0xcb 0xa6 - RES 4, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x10);
							break;
						}

						case 0xa7:	// 0xcb 0xa7 - RES 4, A
						{
							psCPU->z80A &= ~0x10;
							break;
						}

						case 0xa8:	// 0xcb 0xa8 - RES 5, B
						{
							psCPU->z80B &= ~0x20;
							break;
						}

						case 0xa9:	// 0xcb 0xa9 - RES 5, C
						{
							psCPU->z80C &= ~0x20;
							break;
						}

						case 0xaa:	// 0xcb 0xaa - RES 5, D
						{
							psCPU->z80D &= ~0x20;
							break;
						}

						case 0xab:	// 0xcb 0xab - RES 5, E
						{
							psCPU->z80E &= ~0x20;
							break;
						}

						case 0xac:	// 0xcb 0xac - RES 5, H
						{
							psCPU->z80H &= ~0x20;
							break;
						}

						case 0xad:	// 0xcb 0xad - RES 5, L
						{
							psCPU->z80L &= ~0x20;
							break;
						}

						case 0xae:	// 0xcb 0xae - RES 5, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x20);
							break;
						}

						case 0xaf:	// 0xcb 0xaf - RES 5, A
						{
							psCPU->z80A &= ~0x20;
							break;
						}

						case 0xb0:	// 0xcb 0xb0 - RES 6, B
						{
							psCPU->z80B &= ~0x40;
							break;
						}

						case 0xb1:	// 0xcb 0xb1 - RES 6, C
						{
							psCPU->z80C &= ~0x40;
							break;
						}

						case 0xb2:	// 0xcb 0xb2 - RES 6, D
						{
							psCPU->z80D &= ~0x40;
							break;
						}

						case 0xb3:	// 0xcb 0xb3 - RES 6, E
						{
							psCPU->z80E &= ~0x40;
							break;
						}

						case 0xb4:	// 0xcb 0xb4 - RES 6, H
						{
							psCPU->z80H &= ~0x40;
							break;
						}

						case 0xb5:	// 0xcb 0xb5 - RES 6, L
						{
							psCPU->z80L &= ~0x40;
							break;
						}

						case 0xb6:	// 0xcb 0xb6 - RES 6, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x40);
							break;
						}

						case 0xb7:	// 0xcb 0xb7 - RES 6, A
						{
							psCPU->z80A &= ~0x40;
							break;
						}

						case 0xb8:	// 0xcb 0xb8 - RES 7, B
						{
							psCPU->z80B &= ~0x80;
							break;
						}

						case 0xb9:	// 0xcb 0xb9 - RES 7, C
						{
							psCPU->z80C &= ~0x80;
							break;
						}

						case 0xba:	// 0xcb 0xba - RES 7, D
						{
							psCPU->z80D &= ~0x80;
							break;
						}

						case 0xbb:	// 0xcb 0xbb - RES 7, E
						{
							psCPU->z80E &= ~0x80;
							break;
						}

						case 0xbc:	// 0xcb 0xbc - RES 7, H
						{
							psCPU->z80H &= ~0x80;
							break;
						}

						case 0xbd:	// 0xcb 0xbd - RES 7, L
						{
							psCPU->z80L &= ~0x80;
							break;
						}

						case 0xbe:	// 0xcb 0xbe - RES 7, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) & ~0x80);
							break;
						}

						case 0xbf:	// 0xcb 0xbf - RES 7, A
						{
							psCPU->z80A &= ~0x80;
							break;
						}

						case 0xc0:	// 0xcb 0xc0 - SET 0, B
						{
							psCPU->z80B |= 0x01;
							break;
						}

						case 0xc1:	// 0xcb 0xc1 - SET 0, C
						{
							psCPU->z80C |= 0x01;
							break;
						}

						case 0xc2:	// 0xcb 0xc2 - SET 0, D
						{
							psCPU->z80D |= 0x01;
							break;
						}

						case 0xc3:	// 0xcb 0xc3 - SET 0, E
						{
							psCPU->z80E |= 0x01;
							break;
						}

						case 0xc4:	// 0xcb 0xc4 - SET 0, H
						{
							psCPU->z80H |= 0x01;
							break;
						}

						case 0xc5:	// 0xcb 0xc5 - SET 0, L
						{
							psCPU->z80L |= 0x01;
							break;
						}

						case 0xc6:	// 0xcb 0xc6 - SET 0, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x01);
							break;
						}

						case 0xc7:	// 0xcb 0xc7 - SET 0, A
						{
							psCPU->z80A |= 0x01;
							break;
						}

						case 0xc8:	// 0xcb 0xc8 - SET 1, B
						{
							psCPU->z80B |= 0x02;
							break;
						}

						case 0xc9:	// 0xcb 0xc9 - SET 1, C
						{
							psCPU->z80C |= 0x02;
							break;
						}

						case 0xca:	// 0xcb 0xca - SET 1, D
						{
							psCPU->z80D |= 0x02;
							break;
						}

						case 0xcb:	// 0xcb 0xcb - SET 1, E
						{
							psCPU->z80E |= 0x02;
							break;
						}

						case 0xcc:	// 0xcb 0xcc - SET 1, H
						{
							psCPU->z80H |= 0x02;
							break;
						}

						case 0xcd:	// 0xcb 0xcd - SET 1, L
						{
							psCPU->z80L |= 0x02;
							break;
						}

						case 0xce:	// 0xcb 0xce - SET 1, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x02);
							break;
						}

						case 0xcf:	// 0xcb 0xcf - SET 1, A
						{
							psCPU->z80A |= 0x02;
							break;
						}

						case 0xd0:	// 0xcb 0xd0 - SET 2, B
						{
							psCPU->z80B |= 0x04;
							break;
						}

						case 0xd1:	// 0xcb 0xd1 - SET 2, C
						{
							psCPU->z80C |= 0x04;
							break;
						}

						case 0xd2:	// 0xcb 0xd2 - SET 2, D
						{
							psCPU->z80D |= 0x04;
							break;
						}

						case 0xd3:	// 0xcb 0xd3 - SET 2, E
						{
							psCPU->z80E |= 0x04;
							break;
						}

						case 0xd4:	// 0xcb 0xd4 - SET 2, H
						{
							psCPU->z80H |= 0x04;
							break;
						}

						case 0xd5:	// 0xcb 0xd5 - SET 2, L
						{
							psCPU->z80L |= 0x04;
							break;
						}

						case 0xd6:	// 0xcb 0xd6 - SET 2, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x04);
							break;
						}

						case 0xd7:	// 0xcb 0xd7 - SET 2, A
						{
							psCPU->z80A |= 0x04;
							break;
						}

						case 0xd8:	// 0xcb 0xd8 - SET 3, B
						{
							psCPU->z80B |= 0x08;
							break;
						}

						case 0xd9:	// 0xcb 0xd9 - SET 3, C
						{
							psCPU->z80C |= 0x08;
							break;
						}

						case 0xda:	// 0xcb 0xda - SET 3, D
						{
							psCPU->z80D |= 0x08;
							break;
						}

						case 0xdb:	// 0xcb 0xdb - SET 3, E
						{
							psCPU->z80E |= 0x08;
							break;
						}

						case 0xdc:	// 0xcb 0xdc - SET 3, H
						{
							psCPU->z80H |= 0x08;
							break;
						}

						case 0xdd:	// 0xcb 0xdd - SET 3, L
						{
							psCPU->z80L |= 0x08;
							break;
						}

						case 0xde:	// 0xcb 0xde - SET 3, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x08);
							break;
						}

						case 0xdf:	// 0xcb 0xdf - SET 3, A
						{
							psCPU->z80A |= 0x08;
							break;
						}

						case 0xe0:	// 0xcb 0xe0 - SET 4, B
						{
							psCPU->z80B |= 0x10;
							break;
						}

						case 0xe1:	// 0xcb 0xe1 - SET 4, C
						{
							psCPU->z80C |= 0x10;
							break;
						}

						case 0xe2:	// 0xcb 0xe2 - SET 4, D
						{
							psCPU->z80D |= 0x10;
							break;
						}

						case 0xe3:	// 0xcb 0xe3 - SET 4, E
						{
							psCPU->z80E |= 0x10;
							break;
						}

						case 0xe4:	// 0xcb 0xe4 - SET 4, H
						{
							psCPU->z80H |= 0x10;
							break;
						}

						case 0xe5:	// 0xcb 0xe5 - SET 4, L
						{
							psCPU->z80L |= 0x10;
							break;
						}

						case 0xe6:	// 0xcb 0xe6 - SET 4, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x10);
							break;
						}

						case 0xe7:	// 0xcb 0xe7 - SET 4, A
						{
							psCPU->z80A |= 0x10;
							break;
						}

						case 0xe8:	// 0xcb 0xe8 - SET 5, B
						{
							psCPU->z80B |= 0x20;
							break;
						}

						case 0xe9:	// 0xcb 0xe9 - SET 5, C
						{
							psCPU->z80C |= 0x20;
							break;
						}

						case 0xea:	// 0xcb 0xea - SET 5, D
						{
							psCPU->z80D |= 0x20;
							break;
						}

						case 0xeb:	// 0xcb 0xeb - SET 5, E
						{
							psCPU->z80E |= 0x20;
							break;
						}

						case 0xec:	// 0xcb 0xec - SET 5, H
						{
							psCPU->z80H |= 0x20;
							break;
						}

						case 0xed:	// 0xcb 0xed - SET 5, L
						{
							psCPU->z80L |= 0x20;
							break;
						}

						case 0xee:	// 0xcb 0xee - SET 5, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x20);
							break;
						}

						case 0xef:	// 0xcb 0xef - SET 5, A
						{
							psCPU->z80A |= 0x20;
							break;
						}

						case 0xf0:	// 0xcb 0xf0 - SET 6, B
						{
							psCPU->z80B |= 0x40;
							break;
						}

						case 0xf1:	// 0xcb 0xf1 - SET 6, C
						{
							psCPU->z80C |= 0x40;
							break;
						}

						case 0xf2:	// 0xcb 0xf2 - SET 6, D
						{
							psCPU->z80D |= 0x40;
							break;
						}

						case 0xf3:	// 0xcb 0xf3 - SET 6, E
						{
							psCPU->z80E |= 0x40;
							break;
						}

						case 0xf4:	// 0xcb 0xf4 - SET 6, H
						{
							psCPU->z80H |= 0x40;
							break;
						}

						case 0xf5:	// 0xcb 0xf5 - SET 6, L
						{
							psCPU->z80L |= 0x40;
							break;
						}

						case 0xf6:	// 0xcb 0xf6 - SET 6, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x40);
							break;
						}

						case 0xf7:	// 0xcb 0xf7 - SET 6, A
						{
							psCPU->z80A |= 0x40;
							break;
						}

						case 0xf8:	// 0xcb 0xf8 - SET 7, B
						{
							psCPU->z80B |= 0x80;
							break;
						}

						case 0xf9:	// 0xcb 0xf9 - SET 7, C
						{
							psCPU->z80C |= 0x80;
							break;
						}

						case 0xfa:	// 0xcb 0xfa - SET 7, D
						{
							psCPU->z80D |= 0x80;
							break;
						}

						case 0xfb:	// 0xcb 0xfb - SET 7, E
						{
							psCPU->z80E |= 0x80;
							break;
						}

						case 0xfc:	// 0xcb 0xfc - SET 7, H
						{
							psCPU->z80H |= 0x80;
							break;
						}

						case 0xfd:	// 0xcb 0xfd - SET 7, L
						{
							psCPU->z80L |= 0x80;
							break;
						}

						case 0xfe:	// 0xcb 0xfe - SET 7, (HL)
						{
							MZ80WriteByte(psCPU, psCPU->z80HL, MZ80ReadByte(psCPU, psCPU->z80HL) | 0x80);
							break;
						}

						case 0xff:	// 0xcb 0xff - SET 7, A
						{
							psCPU->z80A |= 0x80;
							break;
						}
						default:
						{
							char u8String[100];
							pu8PC -= 2; 	// Back up two instructions since it's not valid
							u32ExitCode = ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
							sprintf(u8String, "Unknown CB opcode at 0x%.4x: 0x%.2x", u32ExitCode, *(pu8PC + 1));
#ifdef _WIN32
							if (sg_psTraceFile)
							{
								fclose(sg_psTraceFile);
							}
#endif

							BASSERT_MSG(u8String);
							goto errorExit;
						}
					}

					break;
				}

				case 0xcc:					// 0xcc - CALL Z, (nn)
				{
					if (psCPU->z80F & Z80F_Z)
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				case 0xcd:					// 0xcd - CALL (nn)
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
					pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
					break;
				}
				case 0xce:					// 0xce - ADC, A, n
				{
					MZ80ADCA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xcf:				// 0xcf - RST 08h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xd0:					// 0xd0 - RET NC
				{
					if (0 == (psCPU->z80F & Z80F_C))
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xd1:					// 0xd1 - POP DE
				{
					POPWORD(psCPU->z80DE);
					break;
				}
				case 0xd2:					// 0xd2 - JP NC, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_C))
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip over base address
						pu8PC += 2;
					}
					break;
				}
				case 0xd3:					// 0xd3 n - OUT (n), A
				{
					MZ80OutPort(psCPU, *pu8PC, psCPU->z80A);
					++pu8PC;
					break;
				}
				case 0xd4:					// 0xc4 - CALL NC, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_C))
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				case 0xd5:					// 0xd5 - PUSH DE
				{
					PUSHWORD(psCPU->z80DE);
					break;
				}
				case 0xd6:					// 0xd6 - SUB A, n
				{
					MZ80SUBA(psCPU, *pu8PC);
					pu8PC++;
					break;
				}
				case 0xd7:					// 0xd7 - RST 10h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xd8:					// 0xd8 - RET C
				{
					if (psCPU->z80F & Z80F_C)
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xd9:					// 0xd9 - EXX
				{
					UINT16 u16Temp;

					u16Temp = psCPU->z80BC;
					psCPU->z80BC = psCPU->z80bcprime;
					psCPU->z80bcprime = u16Temp;

					u16Temp = psCPU->z80DE;
					psCPU->z80DE = psCPU->z80deprime;
					psCPU->z80deprime = u16Temp;

					u16Temp = psCPU->z80HL;
					psCPU->z80HL = psCPU->z80hlprime;
					psCPU->z80hlprime = u16Temp;

					break;
				}
				case 0xda:					// 0xda - JP C, (nn)
				{
					if (psCPU->z80F & Z80F_C)
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xdb:					// 0xdb n - IN A, (n)
				{
					psCPU->z80A = MZ80InPort(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xdc:					// 0xdc - CALL C, (nn)
				{
					if (psCPU->z80F & Z80F_C)
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				// ***** DD INSTRUCTIONS *****
				case 0xdd:					// 0xdd xx - Instructions
				{
					pu8PC = IndexedHandler(psCPU,
										   pu8PC,
										   &psCPU->z80IX,
										   &s32Cycles);
					break;
				}
				case 0xde:					// 0xde xx - SBC A, n
				{
					MZ80SBCA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xdf:				// 0xdf - RST 18h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xe0:					// 0xd0 - RET PO
				{
					if (0 == (psCPU->z80F & Z80F_PV))
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xe1:				// 0xe1 - POP HL
				{
					POPWORD(psCPU->z80HL);
					break;
				}
				case 0xe2:					// 0xe2 - JP O, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_PV))
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xe3:			// 0xfd 0xe3 - EX (SP), HL
				{
					UINT16 u16Temp;

					u16Temp = MZ80ReadByte(psCPU, psCPU->z80sp) + (MZ80ReadByte(psCPU, psCPU->z80sp + 1) << 8);
					MZ80WriteByte(psCPU, psCPU->z80sp, psCPU->z80L);
					MZ80WriteByte(psCPU, psCPU->z80sp + 1, psCPU->z80H);
					psCPU->z80HL = u16Temp;
					break;
				}
				case 0xe4:					// 0xe4 - CALL PO, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_PV))
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				case 0xe5:				// 0xe5 - PUSH HL
				{
					PUSHWORD(psCPU->z80HL);
					break;
				}
				case 0xe6:				// 0xe6 - AND A, n
				{
					MZ80ANDA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xe7:					// 0xe7 - RST 20h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xe8:					// 0xe8 - RET PE
				{
					if (psCPU->z80F & Z80F_PV)
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xe9:					// 0xe9 - JP (HL)
				{
					pu8PC = psCPU->pu8CodeBase + psCPU->z80HL;
					break;
				}
				case 0xea:					// 0xe2 - JP PE, (nn)
				{
					if (psCPU->z80F & Z80F_PV)
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xeb:					// 0xec - EX DE, HL
				{
					UINT16 u16Temp;

					u16Temp = psCPU->z80HL;
					psCPU->z80HL = psCPU->z80DE;
					psCPU->z80DE = u16Temp;
					break;
				}
				case 0xec:					// 0xec - CALL PE, (nn)
				{
					if (psCPU->z80F & Z80F_PV)
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}

				// ***** ED INSTRUCTIONS *****
				case 0xed:					// 0xed xx - Instructions
				{
					INC_R(psCPU);
					u8Op = *pu8PC;
					pu8PC++;

					psCPU->u32ClockTicks += sg_u8Z80EDCycleCounts[u8Op];
					s32Cycles -= (INT32) sg_u8Z80EDCycleCounts[u8Op];

					switch (u8Op)
					{
						case 0x00:
						{
							// Dummy code - IAR compiler workaround (bug)
							psCPU->z80B = psCPU->z80B;
							break;
						}
						case 0x40:			// 0x40 - IN B, (C)
						{
							psCPU->z80B = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x41:			// 0x41 - OUT (C), B
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80B);
							break;
						}
						case 0x42:			// 0xed 0x42 - SBC HL, BC
						{
							psCPU->z80HL = MZ80SBC16(psCPU, psCPU->z80HL, psCPU->z80BC);
							break;
						}
						case 0x43:			// 0xed 0x43 - LD (nn), BC
						{
							UINT16 u16Temp;

							u16Temp = *pu8PC | (*(pu8PC + 1) << 8);
							pu8PC += 2;
							MZ80WriteByte(psCPU, u16Temp++, psCPU->z80C);
							MZ80WriteByte(psCPU, u16Temp, psCPU->z80B);
							break;
						}
						case 0x44:			// 0xed 0x44 - NEG
						{
							UINT8 u8Temp = psCPU->z80A;
							psCPU->z80A = 0;
							MZ80SUBA(psCPU, u8Temp);
							break;
						}
						case 0x47:			// 0xed 0x47 - LD I, A
						{
							psCPU->z80i = psCPU->z80A;
							break;
						}
						case 0x48:			// 0x40 - IN C, (C)
						{
							psCPU->z80C = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x4a:			// 0x4a - ADC HL, BC
						{
							psCPU->z80HL = MZ80ADC16(psCPU, psCPU->z80HL, psCPU->z80BC);
							break;
						}
						case 0x4b:			// 0x4b - LD BC, (nn)
						{
							UINT16 u16Temp;
							PC_WORD_FETCH(u16Temp);
							psCPU->z80C = MZ80ReadByte(psCPU, u16Temp++);
							psCPU->z80B = MZ80ReadByte(psCPU, u16Temp);
							break;
						}
						case 0x4d:			// 0xed 0x4d - RETI
						{
							UINT16 u16Temp;
							
							// Copy IFF2 into IFF1
							psCPU->z80iff = (psCPU->z80iff & ~IFF1) | ((psCPU->z80iff & IFF2) >> 1) | (psCPU->z80iff & IRQ_LATCH);
							POPWORD(u16Temp);
							pu8PC = psCPU->pu8CodeBase + u16Temp;
							break;
						}
						case 0x45:			// 0xed 0x45 - RETN
						{
							UINT16 u16Temp;

							// Copy IFF2 into IFF1
							psCPU->z80iff = (psCPU->z80iff & ~IFF1) | ((psCPU->z80iff & IFF2) >> 1) | (psCPU->z80iff & IRQ_LATCH);
							POPWORD(u16Temp);
							pu8PC = psCPU->pu8CodeBase + u16Temp;

							if (psCPU->z80iff & IRQ_LATCH)
							{
								bHandleIRQ = TRUE;
								s32InterruptTiming = s32Cycles;
								s32Cycles = 0;

							}
							goto execOneMore;

							// No break necessary
						}
						case 0x46:			// 0xED 0x46 - IM 0
						case 0x4e:			// 0xED 0x4e - IM 0 (undocumented)
						{
							psCPU->z80InterruptMode = 0;
							break;
						}
						case 0x49:			// 0x49 - OUT (C), C
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80C);
							break;
						}
						case 0x50:			// 0x50 - IN D, (C)
						{
							psCPU->z80D = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x51:			// 0x51 - OUT (C), D
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80D);
							break;
						}
						case 0x52:			// 0xed 0x52 - SBC HL, DE
						{
							psCPU->z80HL = MZ80SBC16(psCPU, psCPU->z80HL, psCPU->z80DE);
							break;
						}
						case 0x53:			// 0xed 0x53 - LD (nn), DE
						{
							UINT16 u16Temp;

							u16Temp = *pu8PC | (*(pu8PC + 1) << 8);
							pu8PC += 2;
							MZ80WriteByte(psCPU, u16Temp++, psCPU->z80E);
							MZ80WriteByte(psCPU, u16Temp, psCPU->z80D);
							break;
						}
						case 0x56:			// 0xED 0x56 - IM 1
						{
							psCPU->z80InterruptMode = 1;
							break;
						}
						case 0x57:			// 0xED 0x57 - LD A, I
						{
							psCPU->z80A = psCPU->z80i;
							psCPU->z80F &= ~(Z80F_N | Z80F_PV | Z80F_H | Z80F_Z | Z80F_S);
							psCPU->z80F |= sg_u8SZ[psCPU->z80i];
							if (psCPU->z80iff & IFF2)
							{
								psCPU->z80F |= Z80F_PV;
							}
							break;
						}
						case 0x58:			// 0x58 - IN E, (C)
						{
							psCPU->z80E = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x59:			// 0x59 - OUT (C), E
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80E);
							break;
						}
						case 0x5a:			// 0x5a - ADC HL, DE
						{
							psCPU->z80HL = MZ80ADC16(psCPU, psCPU->z80HL, psCPU->z80DE);
							break;
						}
						case 0x5b:					// 0x5b - LD DE, (nn)
						{
							UINT16 u16Temp;
							PC_WORD_FETCH(u16Temp);
							psCPU->z80E = MZ80ReadByte(psCPU, u16Temp++);
							psCPU->z80D = MZ80ReadByte(psCPU, u16Temp);
							break;
						}
						case 0x5e:			// 0xED 0x5e - IM 2
						{
							psCPU->z80InterruptMode = 2;
							break;
						}
						case 0x5f:			// 0xed 0x5f - LD A, R
						{
							psCPU->z80A = ((psCPU->z80r & 0x7f) | psCPU->z80rBit8);
							psCPU->z80F = sg_u8SZ[psCPU->z80A] | (psCPU->z80F & Z80F_C);
							if (psCPU->z80iff & IFF2)
							{
								psCPU->z80F |= Z80F_PV;
							}
							break;
						}
						case 0x60:			// 0x60 - IN H, (C)
						{
							psCPU->z80H = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x61:			// 0x61 - OUT (C), H
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80H);
							break;
						}
						case 0x62:			// 0xed 0x62 - SBC HL, HL
						{
							psCPU->z80HL = MZ80SBC16(psCPU, psCPU->z80HL, psCPU->z80HL);
							break;
						}
						case 0x63:			// 0xed 0x63 - LD (nn), HL
						{
							UINT16 u16Temp;

							u16Temp = *pu8PC | (*(pu8PC + 1) << 8);
							pu8PC += 2;
							MZ80WriteByte(psCPU, u16Temp++, psCPU->z80L);
							MZ80WriteByte(psCPU, u16Temp, psCPU->z80H);
							break;
						}
						case 0x67:			// 0xed 0x67 - RRD
						{
							UINT8 u8Temp;
							UINT8 u8Nibble;

							u8Temp = MZ80ReadByte(psCPU, psCPU->z80HL);
							u8Nibble = psCPU->z80A & 0x0f;
							psCPU->z80A &= 0xf0;
							psCPU->z80A |= (u8Temp & 0xf);
							u8Temp >>= 4;
							u8Temp |= (u8Nibble << 4);
							MZ80WriteByte(psCPU, psCPU->z80HL, u8Temp);
							psCPU->z80F = (psCPU->z80F & Z80F_C) | sg_u8SZP[psCPU->z80A];

							break;
						}
						case 0x68:			// 0x68 - IN L, (C)
						{
							psCPU->z80L = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x69:			// 0x69 - OUT (C), L
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80L);
							break;
						}
						case 0x6a:			// 0x6a - ADC HL, HL
						{
							psCPU->z80HL = MZ80ADC16(psCPU, psCPU->z80HL, psCPU->z80HL);
							break;
						}
						case 0x6b:			// 0x6b - LD HL, (nn)
						{
							UINT16 u16Temp;
							PC_WORD_FETCH(u16Temp);
							psCPU->z80L = MZ80ReadByte(psCPU, u16Temp++);
							psCPU->z80H = MZ80ReadByte(psCPU, u16Temp);
							break;
						}
						case 0x72:			// 0xed 0x72 - SBC HL, SP
						{
							psCPU->z80HL = MZ80SBC16(psCPU, psCPU->z80HL, psCPU->z80sp);
							break;
						}
						case 0x73:			// 0xed 0x73 - LD (nn), SP
						{
							UINT16 u16Temp;

							u16Temp = *pu8PC | (*(pu8PC + 1) << 8);
							pu8PC += 2;
							MZ80WriteByte(psCPU, u16Temp++, (UINT8) psCPU->z80sp);
							MZ80WriteByte(psCPU, u16Temp, (UINT8) (psCPU->z80sp >> 8));
							break;
						}
						case 0x78:			// 0x78 - IN A, (C)
						{
							psCPU->z80A = MZ80InPort(psCPU, psCPU->z80C);
							break;
						}
						case 0x79:			// 0x79 - OUT (C), A
						{
							MZ80OutPort(psCPU, psCPU->z80C, psCPU->z80A);
							break;
						}
						case 0x7a:			// 0x7a - ADC HL, SP
						{
							psCPU->z80HL = MZ80ADC16(psCPU, psCPU->z80HL, psCPU->z80sp);
							break;
						}
						case 0xa0:			// 0xed 0xa0 - LDI
						{
							MZ80WriteByte(psCPU, psCPU->z80DE++, MZ80ReadByte(psCPU, psCPU->z80HL++));
							psCPU->z80BC--;
							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N);
							if (psCPU->z80BC)
							{
								// Means the count is non-zero. Back the program counter
								// up so it'll reexecute the LDIR instruction again

								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
							}
							else
							{
								// Means the count is zero. Fall through
								psCPU->z80F &= ~Z80F_PV;	// Clear the parity/overflow flag
							}

							break;
						}
						case 0xa1:			// 0xed 0xa1 - CPI
						{
							UINT8 u8Temp;
							UINT8 u8Result;

							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N | Z80F_Z | Z80F_S);
							u8Temp = MZ80ReadByte(psCPU, psCPU->z80HL++);
							u8Result = psCPU->z80A - u8Temp;
							psCPU->z80F |= (Z80F_N) | (sg_u8SZ[u8Result]) | ((psCPU->z80A ^ u8Temp ^ u8Result) & Z80F_H);
							psCPU->z80BC--;
							s32Cycles -= 21;
							psCPU->u32ClockTicks += 21;

							if (psCPU->z80BC)
							{
								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
							}
							break;
						}

						case 0xb0:			// 0xed 0xb0 - LDIR
						{
							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N);
//							do
							{
								MZ80WriteByte(psCPU, psCPU->z80DE++, MZ80ReadByte(psCPU, psCPU->z80HL++));
								psCPU->z80BC--;
								s32Cycles -= 21;
								psCPU->u32ClockTicks += 21;
							}
//							while ((psCPU->z80BC) && (s32Cycles > 0));

							if (psCPU->z80BC)
							{
								// Means the count is non-zero. Back the program counter
								// up so it'll reexecute the LDIR instruction again

								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
								pu8PC -= 2;
							}
							else
							{
								// Means the count is zero. Fall through
								psCPU->z80F &= ~Z80F_PV;	// Clear the parity/overflow flag
								s32Cycles += 5;			// Give 5 cycles back for a non-jump
								psCPU->u32ClockTicks -= 5;
							}
							break;
						}
						case 0xb1:			// 0xed 0xb1 - CPIR
						{
							UINT8 u8Temp;
							UINT8 u8Result;

							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N | Z80F_Z | Z80F_S);
							u8Temp = MZ80ReadByte(psCPU, psCPU->z80HL++);
							u8Result = psCPU->z80A - u8Temp;
							psCPU->z80F |= (Z80F_N) | (sg_u8SZ[u8Result]) | ((psCPU->z80A ^ u8Temp ^ u8Result) & Z80F_H);
							psCPU->z80BC--;
							s32Cycles -= 21;
							psCPU->u32ClockTicks += 21;

							if (psCPU->z80BC)
							{
								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
							}

							if ((psCPU->z80BC) && (u8Result))
							{
								// Means the count is non-zero. Back the program counter
								// up so it'll reexecute the LDIR instruction again

								pu8PC -= 2;
							}
							else
							{
								// Means the count is zero or we matched. Fall through
								s32Cycles += 5;			// Give 5 cycles back for a non-jump
								psCPU->u32ClockTicks -= 5;
							}
							break;
						}
						case 0xb8:			// 0xed 0xb8 - LDDR
						{
							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N);
//							do
							{
								MZ80WriteByte(psCPU, psCPU->z80DE--, MZ80ReadByte(psCPU, psCPU->z80HL--));
								psCPU->z80BC--;
								s32Cycles -= 21;
								psCPU->u32ClockTicks += 21;
							}
//							while ((psCPU->z80BC) && (s32Cycles > 0));

							if (psCPU->z80BC)
							{
								// Means the count is non-zero. Back the program counter
								// up so it'll reexecute the LDIR instruction again

								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
								pu8PC -= 2;
							}
							else
							{
								// Means the count is zero. Fall through
								psCPU->z80F &= ~Z80F_PV;	// Clear the parity/overflow flag
								s32Cycles += 5;			// Give 5 cycles back for a non-jump
								psCPU->u32ClockTicks -= 5;
							}
							break;
						}
						case 0xb9:			// 0xed 0xb9 - CPDR
						{
							UINT8 u8Temp;
							UINT8 u8Result;

							psCPU->z80F &= ~(Z80F_PV | Z80F_H | Z80F_N | Z80F_Z | Z80F_S);
							u8Temp = MZ80ReadByte(psCPU, psCPU->z80HL--);
							u8Result = psCPU->z80A - u8Temp;
							psCPU->z80F |= (Z80F_N | Z80F_S) | (sg_u8SZ[u8Result]) | ((psCPU->z80A ^ u8Temp ^ u8Result) & Z80F_H);
							psCPU->z80BC--;
							s32Cycles -= 21;
							psCPU->u32ClockTicks += 21;

							if (psCPU->z80BC)
							{
								psCPU->z80F |= Z80F_PV;	// Set the parity/overflow flag
							}

							if ((psCPU->z80BC) && (u8Result))
							{
								// Means the count is non-zero. Back the program counter
								// up so it'll reexecute the CPDR instruction again

								pu8PC -= 2;
							}
							else
							{
								// Means the count is zero or we matched. Fall through
								s32Cycles += 5;			// Give 5 cycles back for a non-jump
								psCPU->u32ClockTicks -= 5;
							}
							break;
						}
					
						default:
						{
							char u8String[100];
							pu8PC -= 2; 	// Back up two instructions since it's not valid
							u32ExitCode = ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
							sprintf(u8String, "Unknown ED opcode at 0x%.4x: 0x%.2x", u32ExitCode, *(pu8PC + 1));
#ifdef _WIN32
							if (sg_psTraceFile)
							{
								fclose(sg_psTraceFile);
							}
#endif

							BASSERT_MSG(u8String);
							goto errorExit;
						}
					}

					break;
				}

				case 0xee:				// 0xee - XOR A, n
				{
					MZ80XORA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}
				case 0xef:				// 0xef - RST 28h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xf0:					// 0xf0 - RET P
				{
					if (0 == (psCPU->z80F & Z80F_S))
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xf1:					// 0xf1 - POP AF
				{
					POPWORD(psCPU->z80AF);
					break;
				}
				case 0xf2:					// 0xf2 - JP P, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_S))
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xf3:					// 0xf3 - DI
				{
					psCPU->z80iff &= ~(IFF1 | IFF2);
					break;
				}
				case 0xf4:					// 0xf4 - CALL P, (nn)
				{
					if (0 == (psCPU->z80F & Z80F_S))
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}
				case 0xf5:					// 0xf5 - PUSH AF
				{
					PUSHWORD(psCPU->z80AF);
					break;
				}
				case 0xf6:					// 0xf6 - XOR A, N
				{
					MZ80ORA(psCPU, *pu8PC);
					pu8PC++;
					break;
				}
				case 0xf7:					// 0xf7 - RST 30h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				case 0xf8:					// 0xf8 - RET M
				{
					if (psCPU->z80F & Z80F_S)
					{
						UINT16 u16Temp;

						POPWORD(u16Temp);
						pu8PC = psCPU->pu8CodeBase + u16Temp;
						s32Cycles -= 6;
						psCPU->u32ClockTicks += 6;
					}
					break;
				}
				case 0xf9:					// 0xf9 - LD SP, HL
				{
					psCPU->z80sp = psCPU->z80HL;
					break;
				}
				case 0xfa:					// 0xfa - JP M, (nn)
				{
					if (psCPU->z80F & Z80F_S)
					{
						PC_ABS_JMP();
						s32Cycles -= 5;
						psCPU->u32ClockTicks += 5;
					}
					else
					{
						// Skip past address
						pu8PC += 2;
					}
					break;
				}
				case 0xfb:				// 0xfb - EI
				{
					psCPU->z80iff |= (IFF1 | IFF2);

					// If we have a pending interrupt, then record our current timing data
					// and force the loop to exit, but exit just one more instruction

					if (psCPU->z80iff & IRQ_LATCH)
					{
						bHandleIRQ = TRUE;
						s32InterruptTiming = s32Cycles;
						s32Cycles = 0;
					}

					goto execOneMore;	// Make sure the next instruction gets executed no matter what
				}
				case 0xfc:					// 0xfc - CALL M, (nn)
				{
					if (psCPU->z80F & Z80F_S)
					{
						PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase)) + 2));
						pu8PC = psCPU->pu8CodeBase + *pu8PC + (*(pu8PC + 1) << 8);
						s32Cycles -= 7;
						psCPU->u32ClockTicks += 7;
					}
					else
					{
						pu8PC += 2;
					}
					break;
				}

				// ***** FD INSTRUCTIONS *****
				case 0xfd:					// 0xfd xx - Instructions
				{
					pu8PC = IndexedHandler(psCPU,
										   pu8PC,
										   &psCPU->z80IY,
										   &s32Cycles);
					break;
				}

				case 0xfe:					// 0xfe xx - CP A, xxh
				{
					MZ80CPA(psCPU, *pu8PC);
					++pu8PC;
					break;
				}

				case 0xff:				// 0xff - RST 38h
				{
					PUSHWORD(((((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase))));
					pu8PC = psCPU->pu8CodeBase + (u8Op & 0x38);
					break;
				}
				default:
				{
					char u8String[100];
					// Invalid instruction
					--pu8PC;		// Back up one instruction since it's not valid
					u32ExitCode = ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
					sprintf(u8String, "Unknown base opcode at 0x%.4x: 0x%.2x", u32ExitCode, *pu8PC);
#ifdef _WIN32
					if (sg_psTraceFile)
					{
						fclose(sg_psTraceFile);
					}
#endif
					BASSERT_MSG(u8String);
					goto errorExit;
				}
			}
		}

		// If we have an interrupt to handle, then do so
		if (bHandleIRQ)
		{
			// Clear pending IRQ line
			psCPU->z80iff &= ~IRQ_LATCH;

			// Restore the program counter
			psCPU->z80pc = (UINT16) ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);

			// Better do an interrupt via the handler
			bHandleIRQ = MZ80INT(psCPU);

			// This should've taken. If it didn't, there's a bug in the handling of
			// the interrupts
			BASSERT(bHandleIRQ);

			// No interrupt to handle
			bHandleIRQ = FALSE;

			// Subtract our "one safe instruction" execution timing away from the remainder
			// of time to execute
			s32Cycles = s32InterruptTiming + s32Cycles;

			// Now subtract away some for the interrupt we've just taken
			s32Cycles -= sg_u8IntTiming[psCPU->z80InterruptMode];

			// Now that the PC has been changed, recalculate it
			pu8PC = psCPU->pu8CodeBase + psCPU->z80pc;

			// Now everything is in sync. Fall through and continue execution.
		}
		else
		{
			// No interrupt to handle - just bail out
			break;
		}
	}

	// Fall through here - will exit the code properly with CPU_EXEC_OK

errorExit:
#ifdef _WIN32
	if (sg_psTraceFile)
	{
		fflush(sg_psTraceFile);
	}
#endif

	// Put the program counter back
	psCPU->z80pc = (UINT16) ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
	return(u32ExitCode);

haltedState:
	s32Cycles += s32InterruptTiming;
	s32InterruptTiming = 0;

	if (s32Cycles > 0)
	{
		// Figure out how many halt cycles are available to us (halt instruction)
		// Quantize it as if we've actually executed all of the halt cycles
		INT32 s32WholeHaltCycles = ((s32Cycles / sg_u8Z80MainCycleCounts[0x76]) + 1);	

		// Add it to the overall clock ticks
		psCPU->u32ClockTicks += (UINT32) (s32WholeHaltCycles * sg_u8Z80MainCycleCounts[0x76]);

		// Now increment R by the appropriate amount - preserve bit 7
		psCPU->z80r = (psCPU->z80r + (UINT8) s32WholeHaltCycles);
	}

	psCPU->z80pc = (UINT16) ((UINT32) pu8PC - (UINT32) psCPU->pu8CodeBase);
	return(u32ExitCode);
}


void MZ80NMI(CONTEXTMZ80 *psCPU)
{
	// Unhalt the CPU if it's halted
	if (psCPU->bHalted)
	{
		psCPU->bHalted = FALSE;

		// Skip past halt instruction
		psCPU->z80pc++;
	}

	// Push the current program counter on the stack
	PUSHWORD(psCPU->z80pc);

	// Move IFF1 to IFF2 and clear IFF1
	psCPU->z80iff = ((psCPU->z80iff & IFF1) << 1) | (psCPU->z80iff & IRQ_LATCH);

	// Move to address 0x66 - NMI
	psCPU->z80pc = 0x66;

	// Increment the R register
	INC_R(psCPU);

	// Add some NMI processing time
	psCPU->u32ClockTicks += 11;
}

UINT32 MZ80GetTicks(CONTEXTMZ80 *psCPU, 
					BOOL bResetTicks)
{
	UINT32 u32Ticks = psCPU->u32ClockTicks;

	if (bResetTicks)
	{
		psCPU->u32ClockTicks = 0;
	}

	return(u32Ticks);
}

// This will completely clear out the MZ80 init structure
void MZ80Init(CONTEXTMZ80 *psCPU)
{
	memset((void *) psCPU, 0, sizeof(*psCPU));
}

