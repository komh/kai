# Makefile for OpenWatcom/WMAKE
.ERASE

.SUFFIXES :
.SUFFIXES : .exe .dll .def .lib .dll_obj .obj .c .h .asm

AS = nasm
ASFLAGS = -f obj

CC = wcc386
CFLAGS = -zq -bt=os2 -wx -bm -d0 -oaxt -DINLINE=inline
SPEEX_CFLAGS = -DOUTSIDE_SPEEX -DEXPORT= -DRANDOM_PREFIX=kai -DFLOATING_POINT

LINK = wlink
LFLAGS = option quiet

RM = rm -f

!include kaidll.mk

STATIC_OBJECTS = kai.obj kai_dart.obj kai_uniaud.obj kai_audiobuffer.obj &
                 kai_instance.obj speex/resample.obj kai_debug.obj &
                 kai_mixer.obj kai_atomic.obj kai_spinlock.obj
DLL_OBJECTS = $(STATIC_OBJECTS:.obj=.dll_obj)

.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -o $@ $<

.asm.dll_obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -o $@ $<

.c: speex   # find .c in speex, too

.c.obj : .AUTODEPEND
    $(CC) $(CFLAGS) $(SPEEX_CFLAGS) -fo=$@ $[@

.c.dll_obj: .AUTODEPEND
    $(CC) $(CFLAGS) $(SPEEX_CFLAGS) -bd -fo=$@ $[@

all : .SYMBOLIC kai.lib kai_dll.lib $(KAIDLL) &
      kaidemo.exe kaidemo2.exe kaidemo3.exe

kai.lib : $(STATIC_OBJECTS)
    -$(RM) $@
    wlib -b -c -q $@ $(STATIC_OBJECTS)

kai_dll.lib: $(KAIDLL)
    -$(RM) $@
    wlib -b -c -q $@ +$(KAIDLL)

$(KAIDLL): $(DLL_OBJECTS) $(KAIDLLDEF)
    $(LINK) $(LFLAGS) @$(KAIDLLDEF) file { $(DLL_OBJECTS) }

$(KAIDLLDEF):
    %create $@
    %append $@ system os2v2_dll initinstance terminstance
    %append $@ name $(KAIDLLNAME)
    %append $@ option manyautodata

kaidemo.exe : kaidemo.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

kaidemo2.exe : kaidemo2.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

kaidemo3.exe : kaidemo3.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

clean : .SYMBOLIC
    -$(RM) *.bak
    -$(RM) *.obj
    -$(RM) *.dll_obj
    -$(RM) *.lib
    -$(RM) *.def
    -$(RM) $(KAIDLL)
    -$(RM) *.exe
    -cd speex
    -$(RM) *.bak
    -$(RM) *.obj
    -$(RM) *.dll_obj
    -cd ..

