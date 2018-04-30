# OoT_Compressor

---

This is a compressor for The Legend of Zelda: Ocarina of Time. It takes a decompressed OoT ROM and a table (Use tableExtract for this), and creates a compressed version of the ROM. From what I've noticed, emulators don't seem to like decompressed ROMs very much, and they occasionally crash when trying to pause. This problem goes away\* with compressed ROMs. Also, if you're trying to inject a game into a .wad file for a Virtual Console, it won't accept decompressed ROMs, so you'll want to compress it.

This program will probably make your CPU fans run very loud, as it pins every core in your processor to 100% for a while. This is done to make it much faster than other compressors, at the cost of a bit of noise.

\*Results may vary

---

Compiling the compressor: make compressor 

Compiling the table extractor: make tableExtractor

Compiling both: make all

~~You'll probably want MinGW for this if you're on Windows. That's what I used anyway.~~ Untested on Windows, will update soon(tm).

---

Usage: yaz0_comp

Notes: At the moment (It will change, probably with the Windows compatibility fix), the program assumes you have a decompressed ROM of OoT called "ZOOTDEC.z64" (No quotes) and a file table called "table.txt" (Also no quotes) in the same directory/folder as the program. It will produce a compressed ROM called "zoot-tmp.z64" (Again, no quotes) in the same directory/folder.
