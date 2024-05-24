#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char **argv)
{
	FILE *fp;
	int s32FileSize;
	char *pu8FileData = NULL;
	char u8OddFilename[100];
	char u8EvenFilename[100];
	char *pePtr = NULL;
	int s32Loop = 0;

	if (argc < 2)
	{
		printf("Usage: %s filename\n", argv[0]);
		return(1);
	}

	fp = fopen(argv[1], "rb");
	if (NULL == fp)
	{
		printf("Can't open file '%s' for reading\n", argv[1]);
		return(2);
	}

	fseek(fp, 0, SEEK_END);
	s32FileSize = ftell(fp);
	printf("File is %d bytes long\n", s32FileSize);
	fseek(fp, 0, SEEK_SET);

	pu8FileData = malloc(s32FileSize);
	assert(pu8FileData);
	fread(pu8FileData, 1, s32FileSize, fp);
	fclose(fp);

	pePtr = argv[1];
	while (*pePtr && *pePtr != '.')
	{
		pePtr++;
	}
	*pePtr = '\0';

	sprintf(u8OddFilename, "%sOdd.bin", argv[1]);
	sprintf(u8EvenFilename, "%sEven.bin", argv[1]);

	printf("Even = %s\n", u8EvenFilename);
	printf("Odd  = %s\n", u8OddFilename);

	fp = fopen(u8OddFilename, "wb");
	assert(fp);
	for (s32Loop = 1; s32Loop < s32FileSize; s32Loop += 2)
	{
		fputc(pu8FileData[s32Loop], fp);
	}
	fclose(fp);

	fp = fopen(u8EvenFilename, "wb");
	assert(fp);
	for (s32Loop = 0; s32Loop < s32FileSize; s32Loop += 2)
	{
		fputc(pu8FileData[s32Loop], fp);
	}
	fclose(fp);

	return(0);
}
