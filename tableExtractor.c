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
    int32_t tabStart, tabSize, tabCount, i;
    uint8_t* refTab;
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

    /* Fill refTab with 1 for compressed, 0 otherwise */
    refTab = malloc(tabCount);
    for(i = 0; i < tabCount; i++)
    {
        tab = getTableEnt(i);
        refTab[i] = (tab.endP == 0) ? '0' : '1';
    }

    /* Write fileTab to table.bin */    
    file = fopen("table.txt", "w");
    fwrite(refTab, tabCount, 1, file);
    fclose(file);
    free(inROM);
    free(refTab);
    
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
        temp = byteSwap_32(tempROM[i]);
        if(temp == 0x7A656C64)
        {
            temp = byteSwap_32(tempROM[i+1]);
            if(temp == 0x61407372)
            {
                temp = byteSwap_32(tempROM[i+2]);
                if((temp & 0xFF000000) == 0x64000000)
                {
                    /* Find first entry in file table */
                    i += 8;
                    temp = byteSwap_32(tempROM[i]);
                    while(temp != 0x00001060)
                    {
                        i += 4;
                        temp = byteSwap_32(tempROM[i]);
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

table_t getTableEnt(uint32_t i)
{
    table_t tab;

    tab.startV = byteSwap_32(fileTab[i*4]);
    tab.endV   = byteSwap_32(fileTab[(i*4)+1]);
    tab.startP = byteSwap_32(fileTab[(i*4)+2]);
    tab.endP   = byteSwap_32(fileTab[(i*4)+3]);

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
