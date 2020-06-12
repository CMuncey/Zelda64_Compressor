CC = gcc

CFLAGS = -s -O3

LIBS = -lpthread

EXECUTABLES = Compress

all: $(EXECUTABLES)

.SUFFIXES: .c .o
Compress: bSwap.o z64dma.o z64archive.o z64yaz0.o z64compressor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm $(EXECUTABLES) *.o
