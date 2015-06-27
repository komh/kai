# Makefile for OpenWatcom/WMAKE
.ERASE

.SUFFIXES :
.SUFFIXES : .exe .dll .def .lib .dll_obj .obj .c .h

CC = wcc386
CFLAGS = -zq -wx -bm -d0 -oaxt -DINLINE=inline

LINK = wlink
LFLAGS = option quiet

RM = del

!include kaidll.mk

STATIC_OBJECTS = kai.obj kai_dart.obj kai_uniaud.obj
DLL_OBJECTS = $(STATIC_OBJECTS:.obj=.dll_obj)

.c.obj : .AUTODEPEND
    $(CC) $(CFLAGS) -fo=$@ $[@

.c.dll_obj: .AUTODEPEND
    $(CC) $(CFLAGS) -bd -fo=$@ $[@

all : .SYMBOLIC kai.lib kai_dll.lib $(KAIDLL) kaidemo.exe kaidemo2.exe

kai.lib : $(STATIC_OBJECTS)
    -$(RM) $@
    wlib -b $@ $(STATIC_OBJECTS)

kai_dll.lib: $(KAIDLL)
    -$(RM) $@
    wlib -b $@ +$(KAIDLL)

$(KAIDLL): $(DLL_OBJECTS) $(KAIDLLDEF)
    $(LINK) $(LFLAGS) @$(KAIDLLDEF) file { $(DLL_OBJECTS) }

$(KAIDLLDEF):
    %create $@
    %append $@ system os2v2 dll initinstance terminstance
    %append $@ name $(KAIDLLNAME)
    %append $@ option manyautodata
    awk '{print "export "$$1 }' $(KAIDLLSYM) >> $@

kaidemo.exe : kaidemo.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

kaidemo2.exe : kaidemo2.obj kai.lib
    $(LINK) $(LFLAGS) system os2v2 name $@ file { $< } library mmpm2

clean : .SYMBOLIC
    -$(RM) *.bak
    -$(RM) *.obj
    -$(RM) *.dll_obj
    -$(RM) *.lib
    -$(RM) *.def
    -$(RM) $(KAIDLL)
    -$(RM) *.exe
