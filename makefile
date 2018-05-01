CC = gcc 

CFLAGS = -g

EXECUTABLES: compressor tableExtractor

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

compressor: compressor.o
	$(CC) $(CFLAGS) -O3 -o Compress.out compressor.o -lpthread

tableExtractor: tableExtractor.o
	$(CC) $(CFLAGS) -O3 -o TabExt.out tableExtractor.o

clean:
	rm Compress.out TabExt.out *.o
