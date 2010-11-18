# Makefile for OpenWatcom/WMAKE
.ERASE

.SUFFIXES :
.SUFFIXES : .exe .lib .obj .c .h

CC = wcc386
CFLAGS = -zq -wx -bm -d0 -oaxt -DINLINE=inline

LINK = wlink
LFLAGS = option quiet

RM = del

.c.obj :
    $(CC) $(CFLAGS) -fo=$@ $[@

all : .SYMBOLIC kai.lib kaidemo.exe kaidemo2.exe

kai.lib : kai.obj kai_dart.obj kai_uniaud.obj
    -$(RM) $@
    wlib -b $@ $<

kai.obj : kai.c kai.h kai_internal.h kai_dart.h kai_uniaud.h

kai_dart.obj : kai_dart.c kai.h kai_internal.h kai_dart.h

kai_uniaud.obj : kai_uniaud.c uniaud.h unidef.h kai.h kai_internal.h kai_uniaud.h

kaidemo.exe : kaidemo.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

kaidemo.obj : kaidemo.c kai.h

kaidemo2.exe : kaidemo2.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

kaidemo2.obj : kaidemo2.c kai.h

clean : .SYMBOLIC
    -$(RM) *.bak
    -$(RM) *.obj
    -$(RM) *.lib
    -$(RM) *.exe
