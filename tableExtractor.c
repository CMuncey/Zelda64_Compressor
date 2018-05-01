#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "yaz0.c"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000

/* Structs */
typedef struct 
{
	uint8_t* src;
	uint8_t* dst;
	int src_size;
	int dst_size;
}
args_t;

typedef struct
{
	uint32_t startV;
	uint32_t endV;
	uint32_t startP;
	uint32_t endP;	
}
table_t;

/* Functions */
uint32_t findTable();
table_t getTableEnt();
void errorCheck(int, char**);

/* Globals */
uint8_t* inROM;
uint32_t* fileTab;

int main(int argc, char** argv)
{
	FILE* file;
	int32_t tabStart, tabSize, tabCount;
	int32_t size;
	int32_t i; 
	args_t* args;
	table_t tab;

	errorCheck(argc, argv);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	inROM = malloc(COMPSIZE);
	fread(inROM, COMPSIZE, 1, file);
	fclose(file);

	/* Find file table, write to fileTab */
	tabStart = findTable();
	fileTab = (uint32_t*)(inROM + tabStart);
	tab = getTableEnt(2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	fileTab = malloc(tabSize);
	memcpy(fileTab, inROM + tabStart, tabSize);
	free(inROM);

	/* Write fileTab to table.bin */	
	file = fopen("table.bin", "wb");
	fwrite(fileTab, tabSize, 1, file);
	fclose(file);
	free(fileTab);
	
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

table_t getTableEnt(uint32_t i)
{
	table_t tab;

	tab.startV = htobe32(fileTab[i*4]);
	tab.endV   = htobe32(fileTab[(i*4)+1]);
	tab.startP = htobe32(fileTab[(i*4)+2]);
	tab.endP   = htobe32(fileTab[(i*4)+3]);

	return(tab);
}

void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: TabExt [Input ROM]\n");
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
	if(i != COMPSIZE)
	{
		fprintf(stderr, "Warning: Invalid input ROM size\n");
		exit(1);
	}
}
