# Zelda64 Compressor

---

This is a compressor for Zelda64 ROMS. These include Ocarina of Time and Majora's Mask, though romhacks should work in theory. It takes a decompressed Zelda64 ROM and a compression exclusion list (Use tableExtract for this), and creates a compressed version of the ROM. From what I've noticed, emulators don't seem to like decompressed ROMs very much, and they occasionally crash when trying to pause. This problem goes away\* with compressed ROMs. Also, if you're trying to inject a game into a .wad file for a Virtual Console, it won't accept decompressed ROMs, so you'll want to compress it.

This program will probably make your CPU fans run very loud, as it pins every core in your processor to 100% for a while. This is done to make it much faster than other compressors (if you can even find any others), at the cost of a bit of noise.

The compressor will also make a file called `ARCHIVE.bin`. This file is used to speed up subsequent compressoions by allowing the compressor to copy over similar files instead of compressing them again. This won't be too useful if you're only compressing vanilla ROMS (Well, it will compress in a few milliseconds, but I'm not sure why you would be compressing the same ROM over and over), but it should prove very useful if you're making ROM hacks or using the OoT Item Randomiser, because if you're only changing a few files, it won't compress the entire ROM again, and will instead only compress what it needs to. Archives for one game will not be very useful for compressing another, though it probably shouldn't cause any real issues.

\*Results may vary

---

Compiling the compressor (Mac/Linux): gcc -s -O3 -o Compress.out compressor.c -lpthread

Compiling the compressor (Windows): gcc -s -O3 -o Compress.exe compressor.c -l:libpthread.a

Compiling the table extractor (Mac/Linux): gcc -s -O3 -o TabExt.out tableExtractor.c

Compiling the table extractor (Windows): gcc -s -O3 -o TabExt.exe tableExtractor.c

I got this to compile on Windows, but I needed a specific version of minGW to do it. I downloaded msys2 (https://www.msys2.org/) then used `pacman -S mingw-w64-x86_64-gcc` to get a version that could compile it, so hopefully that will work.

---

Compressor Usage: Compress.exe [Input ROM] \<Output ROM\>

Table Extractor usage: TabExt.exe [Input ROM]

Notes: In order for the compressor to work, you'll need a `dmaTable.dat` file, which is created by the table extractor, or it can be created manually. This file contains the index of each file in the ROM that shouldn't be compressed. The formatting shouldn't matter as long as the numbers are separated by only whitespace. The compressor should work on any Zelda64 ROM, but it has only been tested on NTSC 1.0 Ocarina of Time and Majora's Mask roms. If the bootrom length has been changed, then the compressor probably won't function properly.
