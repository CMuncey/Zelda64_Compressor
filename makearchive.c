#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000
#define byteSwap(x, y) asm("bswap %%eax" : "=a"(x) : "a"(y))

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
	uint32_t start;
	uint32_t len;
}index_entry;

uint8_t *inROM;
uint32_t *fileTab;

uint32_t findTable();
void getTableEnt(table_t*, uint32_t*, uint32_t);

int main(int argc, char *argv[]) {
	table_t tab;
	uint32_t tabSize, tabCount;
	FILE *input = fopen(argv[1], "rb");
	inROM = malloc(COMPSIZE);
	fread(inROM, 1, COMPSIZE, input);
	fclose(input);

	uint32_t table = findTable();
	fileTab = (uint32_t*)(inROM + table);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	FILE *out = fopen("ARCHIVE.bin", "wb");
	uint32_t writecount = tabCount - 3;
	fwrite(&writecount, sizeof(uint32_t), 1, out);
	int i = 3;
	uint32_t curpos = (sizeof(index_entry) * (tabCount - 3)) + sizeof(uint32_t);
	index_entry *entries = malloc(sizeof(index_entry) * (tabCount - 3));
	for (; i <= tabCount; i++) {
		getTableEnt(&tab, fileTab, i);
		index_entry *entry = &entries[i - 3];
		if (tab.endP != 0) {
			entry->len = tab.endP - tab.startP;
		}
		else {
			entry->len = tab.endV - tab.startV;
		}

		entry->start = curpos;
		curpos += entry->len;
	}
	fwrite(entries, sizeof(index_entry), tabCount - 3, out);

	for (i = 3; i <= tabCount; i++) {
		getTableEnt(&tab, fileTab, i);
		index_entry *entry = &entries[i - 3];
		fwrite(inROM + tab.startP, 1, entry->len, out);
	}

	free(entries);
	fclose(out);
	free(inROM);
	return 0;
}

uint32_t findTable()
{
	uint32_t i, temp;
	uint32_t* tempROM;

	i = 0;
	tempROM = (uint32_t*)inROM;

	while (i + 4 < UINTSIZE)
	{
		/* This marks the begining of the filetable */
		byteSwap(temp, tempROM[i]);
		if (temp == 0x7A656C64)
		{
			byteSwap(temp, tempROM[i + 1]);
			if (temp == 0x61407372)
			{
				byteSwap(temp, tempROM[i + 2]);
				if ((temp & 0xFF000000) == 0x64000000)
				{
					/* Find first entry in file table */
					i += 8;
					byteSwap(temp, tempROM[i]);
					while (temp != 0x00001060)
					{
						i += 4;
						byteSwap(temp, tempROM[i]);
					}
					return((i - 4) * sizeof(uint32_t));
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
	byteSwap(tab->startV, files[i * 4]);
	byteSwap(tab->endV, files[(i * 4) + 1]);
	byteSwap(tab->startP, files[(i * 4) + 2]);
	byteSwap(tab->endP, files[(i * 4) + 3]);
}