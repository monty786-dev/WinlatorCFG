# make            -> builds winecfg-x64.exe and winecfg-x86.exe (needs mingw-w64)
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter -municode -mwindows -static
LIBS    = -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -ladvapi32 -lshell32 -lwininet -lmsimg32

all: winecfg-x64.exe winecfg-x86.exe

winecfg-x64.exe: winecfg.c winecfg.rc winecfg.manifest
	x86_64-w64-mingw32-windres winecfg.rc -O coff -o res64.o
	x86_64-w64-mingw32-gcc $(CFLAGS) winecfg.c res64.o -o $@ $(LIBS)
	x86_64-w64-mingw32-strip $@

winecfg-x86.exe: winecfg.c winecfg.rc winecfg.manifest
	i686-w64-mingw32-windres winecfg.rc -O coff -o res32.o
	i686-w64-mingw32-gcc $(CFLAGS) winecfg.c res32.o -o $@ $(LIBS)
	i686-w64-mingw32-strip $@

clean:
	rm -f *.o *.exe
