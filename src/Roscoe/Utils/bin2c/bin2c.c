#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv)
{
	FILE *psFile;
	uint8_t u8Column = 0;

	if (argc < 3)
	{
		printf("Usage: bin2c file variable_name\n");
		return(1);
	}

	psFile = fopen(argv[1], "rb");
	if (NULL == psFile)
	{
		printf("Can't open file '%s'\n", argv[1]);
		return(1);
	}

	printf("#include <stdint.h>\n\n");
	printf("const uint8_t %s[] __attribute__ ((aligned (8)))=\n{\n", argv[2]);

	while (feof(psFile) == 0)
	{
		uint8_t u8Data;

		u8Data = fgetc(psFile);
		if (feof(psFile))
		{

		}
		else
		{
			if (0 == u8Column)
			{
				printf("\t");
			}

			printf("0x%.2x, ", u8Data);
			if (15 == u8Column)
			{
				printf("\n");
				u8Column = 0;
			}
			else
			{
				u8Column++;
			}
		}
	}

	printf("};\n");

	printf("\n");

	printf("const uint32_t %sSize = sizeof(%s);\n", argv[2], argv[2]);

	return(0);
}
