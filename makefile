CC = gcc 

CFLAGS = -g

EXECUTABLES: compressor tableExtractor

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

compressor: compressor.o
	$(CC) $(CFLAGS) -o yaz0_comp compressor.o -lpthread

tableExtractor: tableExtractor.o
	$(CC) $(CFLAGS) -o tableExt tableExtractor.o

clean:
	rm yaz0_comp tableExt *.o
