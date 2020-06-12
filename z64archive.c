#include "z64archive.h"

void makeArchive(uint8_t* inROM, uint8_t* outROM)
{
	z64dma_t tab;
	uint32_t tabSize, tabCount, tabStart;
	uint32_t fileSize, fileCount, i;
	uint32_t* fileTab;
	FILE* file;

	/* Find DMAtable info */
	tabStart = findTable(outROM);
	fileTab = (uint32_t*)(outROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	fileCount = 0;

	/* Find the number of compressed files in the ROM */
	/* Ignore first 3 files, as they're never compressed */
	for(i = 3; i < tabCount; i++)
	{
		getTableEnt(&tab, fileTab, i);

		if(tab.endP != 0 && tab.endP != 0xFFFFFFFF)
			fileCount++;
	}

	/* Open output file */
	file = fopen("ARCHIVE.bin", "wb");
	if(file == NULL)
	{
		perror("ARCHIVE.bin");
		fprintf(stderr, "Error: Could not create archive\n");
		return;
	}

	/* Write the archive data */
	fwrite(&fileCount, sizeof(uint32_t), 1, file);

	/* Write the fileSize and data for each ref & src */
	for(i = 3; i < tabCount; i++)
	{
		getTableEnt(&tab, fileTab, i);

		if(tab.endP != 0 && tab.endP != 0xFFFFFFFF)
		{
			/* Write the size and data for the decompressed portion */
			fileSize = tab.endV - tab.startV;
			fwrite(&fileSize, sizeof(uint32_t), 1, file);
			fwrite(inROM + tab.startV, 1, fileSize, file);

			/* Write the size and data for the compressed portion */
			fileSize = tab.endP - tab.startP;
			fwrite(&fileSize, sizeof(uint32_t), 1, file);
			fwrite((outROM + tab.startP), 1, fileSize, file);
		}
	}

	fclose(file);
}

void readArchive(z64archive_t* archive, FILE* file)
{
	int32_t junk, i, tempSize;

	/* Get number of files */
	junk = fread(&(archive->fileCount), sizeof(uint32_t), 1, file);

	/* Allocate space for files and sizes */
	archive->refSize = malloc(sizeof(uint32_t) * archive->fileCount);
	archive->srcSize = malloc(sizeof(uint32_t) * archive->fileCount);
	archive->ref = malloc(sizeof(uint8_t*) * archive->fileCount);
	archive->src = malloc(sizeof(uint8_t*) * archive->fileCount);

	/* Read in file size and then file data */
	for(i = 0; i < archive->fileCount; i++)
	{
		/* Decompressed "Reference" file */
		junk = fread(&tempSize, sizeof(uint32_t), 1, file);
		archive->ref[i] = malloc(tempSize);
		archive->refSize[i] = tempSize;
		junk = fread(archive->ref[i], 1, tempSize, file);

		/* Compressed "Source" file */
		junk = fread(&tempSize, sizeof(uint32_t), 1, file);
		archive->src[i] = malloc(tempSize);
		archive->srcSize[i] = tempSize;
		junk = fread(archive->src[i], 1, tempSize, file);
	}
}
