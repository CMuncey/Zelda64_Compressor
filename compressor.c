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

#if BYTE_ORDER==LITTLE_ENDIAN
#define htobe32(x) _internal_htobe32(x)
#elif BYTE_ORDER==BIG_ENDIAN
#define htobe32(x) (x)
#endif

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
	table_t* tab;
}
args_t;

/* Functions */
uint32_t findTable();
void getTableEnt(table_t*, uint32_t*, uint32_t);
void* thread_func(void*);
void errorCheck(int, char**);
uint32_t _internal_htobe32(uint32_t);

/* Globals */
uint8_t* inROM;
uint8_t* outROM;
uint32_t* realTab;
uint32_t numThreads;
pthread_mutex_t lock;

int main(int argc, char** argv)
{
	FILE* file;
	uint32_t* fileTab;
	int32_t tabStart, tabSize, tabCount;
	int32_t size;
	int32_t i;
	char* name;
	args_t* args;
	table_t tab;
	pthread_t thread;

	errorCheck(argc, argv);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	inROM = malloc(DCMPSIZE);
	outROM = calloc(COMPSIZE, sizeof(uint8_t));
	fread(inROM, DCMPSIZE, 1, file);
	fclose(file);
	//i = 0;
	//while(i < COMPSIZE)
		//outROM[i] = i++;

	/* Find the file table and relevant info */
	/* Location, Size, Number of elements */
	tabStart = findTable();
	memcpy(outROM, inROM, tabStart);
	fileTab = (uint32_t*)(inROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	
	/* Read in compression table */
	file = fopen("table.bin", "rb");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	realTab = malloc(size);
	fread(realTab, size, 1, file);
	memcpy(outROM + tabStart, realTab, tabSize);
	fclose(file);

	/* Initialise some stuff */
	pthread_mutex_init(&lock, NULL);
	numThreads = 0;
	i = 3;

	/* Create all the threads */
	while(i < tabCount)
	{
		args = malloc(sizeof(args_t));
		args->tab = malloc(sizeof(table_t));

		getTableEnt(args->tab, realTab, i);
		args->num = i;

		pthread_mutex_lock(&lock);
		numThreads++;
		pthread_mutex_unlock(&lock);

		pthread_create(&thread, NULL, thread_func, args);

		i++;
	}

	/* Wait for all of the threads to finish */
	while(numThreads != 0)
	{
		printf("~%d threads remaining\n", numThreads);
		fflush(stdout);
		sleep(5);
	}

	/* Clean up stuff */	
	pthread_mutex_destroy(&lock);
	free(realTab);
	free(inROM);

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
	sprintf(name, "%s-comp.z64", name);
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
	int i;
	uint32_t* temp;

	i = 0;
	temp = (uint32_t*)inROM;

	while(i+4 < UINTSIZE)
	{
		/* This marks the begining of the filetable */
		if(htobe32(temp[i]) == 0x7A656C64)
		{
			if(htobe32(temp[i+1]) == 0x61407372)
			{
				if((htobe32(temp[i+2]) & 0xFF000000) == 0x64000000)
				{
					/* Find first entry in file table */
					i += 8;
					while(htobe32(temp[i]) != 0x00001060)
						i += 4;
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
	tab->startV = htobe32(files[i*4]);
	tab->endV   = htobe32(files[(i*4)+1]);
	tab->startP = htobe32(files[(i*4)+2]);
	tab->endP   = htobe32(files[(i*4)+3]);
}

void* thread_func(void* arg)
{
	args_t* a;
	table_t* t;
	int i;

	a = arg;
	t = a->tab;
	i = a->num;

	/* Setup the stuff*/
	a->src_size = t->endV - t->startV;
	a->dst_size = a->src_size + 0x160;
	a->src = malloc(a->src_size);
	a->dst = calloc(a->dst_size, sizeof(uint8_t));
	memcpy(a->src, inROM + t->startV, a->src_size);

	/* If needed, compress and fix size */
	/* Otherwise, just copy src into outROM */
	if(t->endP != 0x00000000)
	{	
		yaz0_encode(a->src, a->src_size, a->dst, &(a->dst_size));
		a->dst_size = ((a->dst_size + 31) & -16);
		memcpy(outROM + t->startP, a->dst, a->dst_size);
	}
	else
	{
		memcpy(outROM + t->startP, a->src, a->src_size);
	}

	/* Free memory to avoid leaks */
	free(a->src);
	free(a->dst);
	free(a->tab);
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
	file = fopen("table.bin", "rb");
	if(file == NULL)
	{
		perror("table.bin");
		fclose(file);
		exit(1);
	}
	fclose(file);
}
uint32_t _internal_htobe32(uint32_t in) {
	return (in >> 24) & 0xff |
		(in << 8) & 0xff0000 |
		(in >> 8) & 0xff00 |
		(in << 24) & 0xff000000;
}