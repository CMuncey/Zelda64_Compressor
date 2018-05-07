#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "yaz0.c"
#include "crc.c"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000
#define DCMPSIZE 0x4000000
#define byteSwap(x, y) asm("bswap %%eax" : "=a"(x) : "a"(y))

/* Structs */
typedef struct
{
	uint32_t startV;
	uint32_t endV;
	uint32_t startP;
	uint32_t endP;	
}
table_t;

typedef struct 
{
	uint32_t num;
	uint8_t* src;
	uint8_t* dst;
	int src_size;
	int dst_size;
	table_t  tab;
}
args_t;

typedef struct
{
	table_t table;
	uint8_t* data;
	uint8_t  comp;
	uint32_t size;
}
output_t;

/* Functions */
uint32_t findTable();
void getTableEnt(table_t*, uint32_t*, uint32_t);
void* thread_func(void*);
void errorCheck(int, char**);

/* Globals */
uint8_t* inROM;
uint8_t* outROM;
uint8_t* refTab;
uint32_t numThreads;
pthread_mutex_t lock;
output_t* out;

int main(int argc, char** argv)
{
	FILE* file;
	uint32_t* fileTab;
	int32_t tabStart, tabSize, tabCount;
	volatile int32_t prev, prev_comp;
	int32_t i, size;
	char* name;
	args_t* args;
	table_t tab;
	pthread_t thread;

	errorCheck(argc, argv);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	inROM = malloc(DCMPSIZE);
	fread(inROM, DCMPSIZE, 1, file);
	fclose(file);

	/* Find the file table and relevant info */
	tabStart = findTable();
	fileTab = (uint32_t*)(inROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	
	/* Read in compression reference */
	file = fopen("table.txt", "r");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	refTab = malloc(size);
	for(i = 0; i < size; i++)
		refTab[i] = (fgetc(file) == '1') ? 1 : 0;
	fclose(file);

	/* Initialise some stuff */
	out = malloc(sizeof(output_t) * tabCount);
	pthread_mutex_init(&lock, NULL);
	numThreads = 0;

	/* Create all the threads */
	for(i = 3; i < tabCount; i++)
	{
		args = malloc(sizeof(args_t));

		getTableEnt(&(args->tab), fileTab, i);
		args->num = i;

		pthread_mutex_lock(&lock);
		numThreads++;
		pthread_mutex_unlock(&lock);

		pthread_create(&thread, NULL, thread_func, args);
	}

	/* Wait for all of the threads to finish */
	while(numThreads != 0)
	{
		printf("~%d threads remaining\n", numThreads);
		fflush(stdout);
		sleep(5);
	}

	/* Setup for copying to outROM */
	outROM = calloc(COMPSIZE, sizeof(uint8_t));
	memcpy(outROM, inROM, tabStart + tabSize);
	pthread_mutex_destroy(&lock);
	prev = tabStart + tabSize;
	prev_comp = refTab[2];
	tabStart += 0x20;
	free(refTab);
	free(inROM);

	/* Copy to outROM loop */
	for(i = 3; i < tabCount; i++)
	{
		tab = out[i].table;
		size = out[i].size;
		tabStart += 0x10;

		/* Finish table and copy to outROM */
		if(tab.startV != tab.endV)
		{
			tab.startP = prev;
			if(out[i].comp)
				tab.endP = tab.startP + size;
			memcpy(outROM + tab.startP, out[i].data, size);
			byteSwap(tab.startV, tab.startV);
			byteSwap(tab.endV, tab.endV);
			byteSwap(tab.startP, tab.startP);
			byteSwap(tab.endP, tab.endP);
			memcpy(outROM + tabStart, &tab, sizeof(table_t));
		}

		/* Setup for next iteration */
		prev += size;
		prev_comp = out[i].comp;

		free(out[i].data);
	}
	free(out);

	/* Make and fill the output ROM */
	i = 0;
	size = strlen(argv[1]);
	name = malloc(size + 5);
	strcpy(name, argv[1]);
	while(i < size)
	{
		if(name[i] == '.')
		{
			name[i] = '\0';
			break;
		}
		i++;
	}
	strcat(name, "-comp.z64");
	file = fopen(name, "wb");
	fwrite(outROM, COMPSIZE, 1, file);
	fclose(file);
	free(outROM);

	/* Fix the CRC using some kind of magic or something */
	fix_crc(name);
	free(name);
	
	return(0);
}

uint32_t findTable()
{
	uint32_t i, temp;
	uint32_t* tempROM;

	i = 0;
	tempROM = (uint32_t*)inROM;

	while(i+4 < UINTSIZE)
	{
		/* This marks the begining of the filetable */
		byteSwap(temp, tempROM[i]);
		if(temp == 0x7A656C64)
		{
			byteSwap(temp, tempROM[i+1]);
			if(temp == 0x61407372)
			{
				byteSwap(temp, tempROM[i+2]);
				if((temp & 0xFF000000) == 0x64000000)
				{
					/* Find first entry in file table */
					i += 8;
					byteSwap(temp, tempROM[i]);
					while(temp != 0x00001060)
					{
						i += 4;
						byteSwap(temp, tempROM[i]);
					}
					return((i-4) * sizeof(uint32_t));
				}
			}
		}

		i += 4;
	}

	fprintf(stderr, "Error: Couldn't find file table\n");
	exit(1);
}

void getTableEnt(table_t* tab, uint32_t* files, uint32_t i)
{
	byteSwap(tab->startV, files[i*4]);
	byteSwap(tab->endV,   files[(i*4)+1]);
	byteSwap(tab->startP, files[(i*4)+2]);
	byteSwap(tab->endP,   files[(i*4)+3]);
}

void* thread_func(void* arg)
{
	args_t* a;
	table_t t;
	int i;

	a = arg;
	t = a->tab;
	i = a->num;

	/* Setup the src*/
	a->src_size = t.endV - t.startV;
	a->src = inROM + t.startV;

	/* If needed, compress and fix size */
	/* Otherwise, just copy src into outROM */
	if(refTab[i])
	{
		a->dst_size = a->src_size + 0x160;
		a->dst = calloc(a->dst_size, sizeof(uint8_t));
		yaz0_encode(a->src, a->src_size, a->dst, &(a->dst_size));
		a->src_size = ((a->dst_size + 31) & -16);
		out[i].comp = 1;
		out[i].data = malloc(a->src_size);
		memcpy(out[i].data, a->dst, a->src_size);
		free(a->dst);
	}
	else
	{
		out[i].comp = 0;
		out[i].data = malloc(a->src_size);
		memcpy(out[i].data, a->src, a->src_size);
	}

	out[i].table = t;
	out[i].size = a->src_size;
	free(a);

	/* Decrement thread counter */
	pthread_mutex_lock(&lock);
	numThreads--;
	pthread_mutex_unlock(&lock);

	return(NULL);
}

void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: Compress [Input ROM]\n");
		exit(1);
	}

	/* Check that input ROM exists */
	file = fopen(argv[1], "rb");
	if(file == NULL)
	{
		perror(argv[1]);
		fclose(file);
		exit(1);
	}

	/* Check that input ROM is correct size */
	fseek(file, 0, SEEK_END);
	i = ftell(file);
	fclose(file);	
	if(i != DCMPSIZE)
	{
		fprintf(stderr, "Warning: Invalid input ROM size\n");
		exit(1);
	}

	/* Check that table.bin exists */
	file = fopen("table.txt", "r");
	if(file == NULL)
	{
		perror("table.txt");
		fprintf(stderr, "If you do not have a table.txt file, please use TabExt first\n");
		fclose(file);
		exit(1);
	}
	fclose(file);
}