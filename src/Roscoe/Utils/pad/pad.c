#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv)
{
	FILE *psFile;
	uint8_t *pu8Buffer;
	uint32_t u32Size;
	uint32_t u32SizeAdjusted;
	int s32Result;

	if (argc < 4)
	{
		printf("Usage: pad sourcefile destfile quantize size\n");
		exit(1);
	}

	psFile = fopen(argv[1], "rb");
	if (NULL == psFile)
	{
		printf("Can't open '%s' for reading\n", argv[1]);
		exit(1);
	}

	fseek(psFile, 0, SEEK_END);
	u32Size = ftell(psFile);
	u32SizeAdjusted = u32Size + (atol(argv[3]) - (u32Size % atol(argv[3])));
	fseek(psFile, 0, SEEK_SET);

	pu8Buffer = malloc(u32SizeAdjusted);
	if (NULL == pu8Buffer)
	{
		printf("Failed to malloc\n");
		exit(1);
	}

	memset((void *) pu8Buffer, 0xff, u32SizeAdjusted);
	s32Result = fread(pu8Buffer, 1, u32Size, psFile);
	if (s32Result != u32Size)
	{
		printf("Tried to read %u bytes, read %d bytes\n", u32Size, s32Result);
		exit(1);
	}

	fclose(psFile);

	printf("Read %d bytes, quantized to %u bytes\n", s32Result, u32SizeAdjusted);

	psFile = fopen(argv[2], "wb");
	if (NULL == psFile)
	{
		printf("Can't open file '%s' for writing\n", argv[2]);
		exit(1);
	}

	s32Result = fwrite(pu8Buffer, 1, u32SizeAdjusted, psFile);
	if (s32Result != u32SizeAdjusted)
	{
		printf("Tried to write %u bytes, wrote %d bytes\n", u32SizeAdjusted, s32Result);
		exit(1);
	}

	fclose(psFile);
	return(0);
}
