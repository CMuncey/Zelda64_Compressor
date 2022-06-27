#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/* Local includes */
#include "z64dma.h"
#include "z64archive.h"
#include "z64yaz0.h"
#include "z64consts.h"
#include "bSwap.h"
#include "z64crc.h"

/* Needed to compile on Windows */
#ifdef _WIN32
#include <Windows.h>
#endif

/* Number of extra bytes to add to compression buffer */
/* Worst case scenario every 8 bytes of input will need 1 more */
/* byte of output. Also need room for the yaz0 header, 16 bytes */
/* dstSize = srcSize + (srcSize >> 3) + 16 */
/* But this is easier and seems to work */
#define COMPBUFF 0x250

/* Temporary storage for output data */
typedef struct
{
	z64dma_t table;
	uint8_t* data;
	uint8_t  comp;
	uint32_t size;
}
output_t;

/* Functions {{{1 */
void*    threadFunc(void*);
void     errorCheck(int, char**);
int32_t  getNumCores();
int32_t  getNext();
uint8_t  compare(uint32_t, uint8_t*, uint8_t*);
/* 1}}} */

/* Globals {{{1 */
char* inName;
char* outName;
uint8_t* inROM;
uint8_t* outROM;
uint8_t* refTab;
pthread_mutex_t filelock;
pthread_mutex_t countlock;
int32_t numFiles, nextFile;
int32_t arcCount, outSize;
uint32_t* fileTab;
z64archive_t* archive;
output_t* out;
/* 1}}} */

/* int main(int, char**) {{{1 */
int main(int argc, char** argv)
{
	FILE* file;
	int32_t tabStart, tabSize, tabCount, junk;
	volatile int32_t prev;
	int32_t i, j, size, numCores, tempSize;
	pthread_t* threads;
	z64dma_t tab;

	errorCheck(argc, argv);
	printf("Zelda64 Compressor, Version 3\n");
	fflush(stdout);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	fseek(file, 0, SEEK_END);
	tempSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	inROM = calloc(tempSize, sizeof(uint8_t));
	junk = fread(inROM, tempSize, 1, file);
	fclose(file);

	/* Read archive if it exists*/
	file = fopen("ARCHIVE.bin", "rb");
	if(file != NULL)
	{
		/* Get number of files */
		printf("Loading Archive.\n");
		fflush(stdout);
		archive = malloc(sizeof(z64archive_t));
		readArchive(archive, file);
		fclose(file);
	}
	else
	{
		printf("No archive found, this could take a while.\n");
		fflush(stdout);
		archive = NULL;
	}

	/* Find the file table and relevant info */
	tabStart = findTable(inROM);
	fileTab = (uint32_t*)(inROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;

	/* Allocate space for the exclusion list */
	/* Default to 1 (compress), set exclusions to 0 */
	file = fopen("dmaTable.dat", "r");
	size = tabCount - 1;
	refTab = malloc(sizeof(uint8_t) * size);
	memset(refTab, 1, size);

	/* The first 3 files are never compressed */
	/* They should never be given to the compression function anyway though */
	refTab[0] = refTab[1] = refTab[2] = 0;

	/* Read in the rest of the exclusion list */
	for(i = 0; fscanf(file, "%d", &j) == 1; i++)
	{
		/* Make sure the number is within the dmaTable */
		if(j > size || j < -size)
		{
			fprintf(stderr, "Error: Entry %d in dmaTable.dat is out of bounds\n", i);
			exit(1);
		}

		/* If j was negative, the file shouldn't exist */
		/* Otherwise, set file to not compress */
		if(j < 0)
			refTab[-j] = 2;
		else
			refTab[j] = 0;
	}
	fclose(file);

	/* Initialise some stuff */
	out = malloc(sizeof(output_t) * tabCount);
	pthread_mutex_init(&filelock, NULL);
	pthread_mutex_init(&countlock, NULL);
	numFiles = tabCount;
	outSize = COMPSIZE;
	nextFile = 3;
	arcCount = 0;

	/* Get CPU core count */
	numCores = getNumCores();
	threads = malloc(sizeof(pthread_t) * numCores);
	printf("Detected %d cores.\n", (numCores));
	printf("Starting compression.\n");
	fflush(stdout);

	/* Create all the threads */
	for(i = 0; i < numCores; i++)
		pthread_create(&threads[i], NULL, threadFunc, NULL);

	/* Wait for all of the threads to finish */
	for(i = 0; i < numCores; i++)
		pthread_join(threads[i], NULL);
	printf("\n");

	/* Get size of new ROM */
	/* Start with size of first 3 files */
	tempSize = tabStart + tabSize;
	for(i = 3; i < tabCount; i++)
		tempSize += out[i].size;

	/* If ROM is too big, update size */
	if(tempSize > outSize)
		outSize = tempSize;

	/* Setup for copying to outROM */
	printf("Files compressed, writing new ROM.\n");
	outROM = calloc(outSize, sizeof(uint8_t));
	memcpy(outROM, inROM, tabStart + tabSize);
	prev = tabStart + tabSize;
	tabStart += 0x20;

	/* Free some stuff */
	pthread_mutex_destroy(&filelock);
	pthread_mutex_destroy(&countlock);
	if(archive != NULL)
	{
		free(archive->ref);
		free(archive->src);
		free(archive->refSize);
		free(archive->srcSize);
		free(archive);
	}
	free(threads);
	free(refTab);

	/* Write data to outROM */
	for(i = 3; i < tabCount; i++)
	{
		tab = out[i].table;
		size = out[i].size;
		tabStart += 0x10;

		/* Finish table and copy to outROM */
		if(tab.startV != tab.endV)
		{
			/* Set up physical addresses */
			tab.startP = prev;
			if(out[i].comp == 1)
				tab.endP = tab.startP + size;
			else if(out[i].comp == 2)
				tab.startP = tab.endP = 0xFFFFFFFF;

			/* If the file existed, write it */
			if(tab.startP != 0xFFFFFFFF)
				memcpy(outROM + tab.startP, out[i].data, size);

			/* Write the table entry */
			tab.startV = bSwap32(tab.startV);
			tab.endV   = bSwap32(tab.endV);
			tab.startP = bSwap32(tab.startP);
			tab.endP   = bSwap32(tab.endP);
			memcpy(outROM + tabStart, &tab, sizeof(z64dma_t));
		}

		prev += size;
		if(out[i].data != NULL)
			free(out[i].data);
	}
	free(out);

	/* Fix the CRC before writing the ROM */
	fixCRC(outROM);

	/* Make and fill the output ROM */
	file = fopen(outName, "wb");
	fwrite(outROM, outSize, 1, file);
	fclose(file);

	/* Make the archive if needed */
	if(archive == NULL)
	{
		printf("Creating archive.\n");
		makeArchive(inROM, outROM);
	}

	/* Free up the last bit of memory */
	if(argc != 3)
		free(outName);
	free(inROM);
	free(outROM);

	printf("Compression complete.\n");

	return(0);
}
/* 1}}} */

/* void* threadFunc(void*) {{{1 */
void* threadFunc(void* null)
{
	uint8_t* src;
	uint8_t* dst;
	z64dma_t   t;
	int32_t i, nextArchive, size, srcSize;

	while((i = getNext()) != -1)
	{
		/* Setup the src */
		getTableEnt(&(t), fileTab, i);
		srcSize = t.endV - t.startV;
		src = inROM + t.startV;

		/* If refTab is 1, compress */
		/* If refTab is 2, file shouldn't exist (OoT doesn't have this) */
		/* Otherwise, just copy src into out */
		if(refTab[i] == 1)
		{
			pthread_mutex_lock(&countlock);
			nextArchive = arcCount++;
			pthread_mutex_unlock(&countlock);

			/* If uncompressed is the same as archive, just copy/paste the compressed */
			/* Otherwise, compress it manually */
			//(memcmp(src, archive->ref[nextArchive], archive->refSize[nextArchive]) == 0))
			if((archive != NULL) && compare(archive->refSize[nextArchive], src, archive->ref[nextArchive]))
			{
				out[i].comp = 1;
				size = archive->srcSize[nextArchive];
				out[i].data = malloc(size);
				memcpy(out[i].data, archive->src[nextArchive], size);
			}
			else
			{
				size = srcSize + COMPBUFF;
				dst = calloc(size, sizeof(uint8_t));
				yaz0_encode(src, srcSize, dst, &(size));
				out[i].comp = 1;
				out[i].data = malloc(size);
				memcpy(out[i].data, dst, size);
				free(dst);
			}

			if(archive != NULL)
			{
				free(archive->ref[nextArchive]);
				free(archive->src[nextArchive]);
			}
		}
		else if(refTab[i] == 2)
		{
			out[i].comp = 2;
			size = 0;
			out[i].data = NULL;
		}
		else
		{
			out[i].comp = 0;
			size = srcSize;
			out[i].data = malloc(size);
			memcpy(out[i].data, src, size);
		}

		/* Set up the table entry and size */
		out[i].table = t;
		out[i].size = size;
	}

	return(NULL);
}
/* 1}}} */

/* int32_t getNumCores() {{{1 */
int32_t getNumCores()
{
	/* Windows */
	#ifdef _WIN32

		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return(info.dwNumberOfProcessors);

	/* Mac */
	#elif MACOS

		int nm[2];
		size_t len;
		uint32_t count;

		len = 4;
		nm[0] = CTL_HW;
		nm[1] = HW_AVAILCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);

		if (count < 1)
		{
			nm[1] = HW_NCPU;
			sysctl(nm, 2, &count, &len, NULL, 0);
			if (count < 1)
				count = 1;
		}
		return(count);

	/* Linux */
	#else

		return(sysconf(_SC_NPROCESSORS_ONLN));

	#endif
}
/* 1}}} */

/* int32_t getNext() {{{1 */
int32_t getNext()
{
	int32_t file;
	double percent;

	pthread_mutex_lock(&filelock);

	file = nextFile++;

	/* Progress tracker */
	/* This lies, it says complete as soon as it hands out the number */
	/* That's why it hangs on 100% for a bit, it's still compressing */
	if (file < numFiles)
	{
		percent = (file+1) * 100;
		percent /= numFiles;
		printf("\33[2K\r%d/%d files complete (%.2lf%%)", file+1, numFiles, percent);
		fflush(stdout);
	}
	else
	{
		file = -1;
	}

	pthread_mutex_unlock(&filelock);

	return(file);
}
/* 1}}} */

/* void errorCheck(int, char**) {{{1 */
void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s [Input ROM] <Output ROM>\n", argv[0]);
		exit(1);
	}

	/* Check that input ROM exists & has permissions */
	inName = argv[1];
	file = fopen(inName, "rb");
	if(file == NULL)
	{
		perror(inName);
		exit(1);
	}

	/* Check that dmaTable.dat exists & has permissions */
	file = fopen("dmaTable.dat", "r");
	if(file == NULL)
	{
		perror("dmaTable.dat");
		fprintf(stderr, "Please make a dmaTable.dat file first\n");
		exit(1);
	}

	/* Check that output ROM is writeable */
	/* Create output filename if needed */
	if(argc < 3)
	{
		i = strlen(inName) + 6;
		outName = malloc(i);
		strcpy(outName, inName);
		for(; i >= 0; i--)
		{
			if(outName[i] == '.')
			{
				outName[i] = '\0';
				break;
			}
		}
		strcat(outName, "-comp.z64");
		file = fopen(outName, "wb");
		if(file == NULL)
		{
			perror(outName);
			free(outName);
			exit(1);
		}
		fclose(file);
	}
	else
	{
		outName = argv[2];
		file = fopen(outName, "wb");
		if(file == NULL)
		{
			perror(outName);
			exit(1);
		}
		fclose(file);
	}
}

uint8_t compare(uint32_t s, uint8_t* a, uint8_t* b)
{
	int i, j;
	uint64_t* a2;
	uint64_t* b2;

	/* Do extra comparisons first to get to multiple of 8 */
	for(i = 0, j = s % 8; i < j; ++i)
		if(a[i] != b[i])
			return(0);

	/* Just to make the next part easier */
	a2 = (uint64_t*)(a + j);
	b2 = (uint64_t*)(b + j);

	/* Do rest of comparisons 8 at a time */
	for(i = 0, j = ((s - j) >> 3); i < j; ++i)
		if(*a2++ != *b2++)
			return(0);

	/* They matched */
	return(1);
}
/* 1}}} */
