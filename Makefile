# Makefile for kLIBC/GNU Make
.PHONY : all

.SUFFIXES : .exe .dll .def .a .lib .o .c .h

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include

ifeq ($(INSTALL),)
INSTALL=ginstall
endif

CC = gcc
CFLAGS = -Wall -O3 -DINLINE=inline -DOS2EMX_PLAIN_CHAR -funsigned-char
LDFLAGS = -Zomf -Zhigh-mem

AR = ar

RM = rm -f

BLDLEVEL_VENDOR := OS/2 Factory
BLDLEVEL_VERSION_MACRO := KAI_VERSION
BLDLEVEL_VERSION_FILE := kai.h
BLDLEVEL_VERSION := $(shell sed -n -e "s/^[ \t]*\#[ \t]*define[ \t]\+$(BLDLEVEL_VERSION_MACRO)[ \t]\+\"\(.*\)\"/\1/p" $(BLDLEVEL_VERSION_FILE))
BLDLEVEL_DATE := $(shell LANG=C date +"\" %F %T %^Z  \"")
BLDLEVEL_HOST = $(shell echo $(HOSTNAME) | cut -b -11)
BLDLEVEL := @\#$(BLDLEVEL_VENDOR):$(BLDLEVEL_VERSION)\#@\#\#1\#\#$(BLDLEVEL_DATE)$(BLDLEVEL_HOST)::::::@@

include kaidll.mk

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

.a.lib :
	emxomf -o $@ $<

all : kai.a kai.lib kai_dll.a kai_dll.lib $(KAIDLL) \
      kaidemo.exe kaidemo2.exe

kai.a : kai.o kai_dart.o kai_uniaud.o
	$(AR) rc $@ $^

kai_dll.a : $(KAIDLL)
	emximp -o $@ $(KAIDLL)

$(KAIDLL): kai.o kai_dart.o kai_uniaud.o $(KAIDLLDEF)
	$(CC) -Zdll $(LDFLAGS) -o $@ $^
	echo $(BLDLEVEL)K Audio Interface >> $@

$(KAIDLLDEF):
	echo LIBRARY $(KAIDLLNAME) INITINSTANCE TERMINSTANCE > $@
	echo DATA MULTIPLE NONSHARED >> $@

kai.o: kai.c kai.h kai_internal.h kai_dart.h kai_uniaud.h

kai_dart.o : kai_dart.c kai.h kai_internal.h kai_dart.h

kai_uniaud.o : kai_uniaud.c uniaud.h unidef.h kai.h kai_internal.h kai_uniaud.h

kaidemo.exe : kaidemo.o kai.lib
	$(CC) $(LDFLAGS) -o $@ $^ -lmmpm2
	echo $(BLDLEVEL)KAI demo >> $@

kaidemo.o : kaidemo.c kai.h

kaidemo2.exe : kaidemo2.o kai.lib
	$(CC) $(LDFLAGS) -o $@ $^ -lmmpm2
	echo $(BLDLEVEL)KAI demo >> $@

kaidemo2.o : kaidemo2.c kai.h

clean :
	$(RM) *.bak
	$(RM) *.o
	$(RM) *.a
	$(RM) *.obj
	$(RM) *.lib
	$(RM) *.def
	$(RM) $(KAIDLL)
	$(RM) *.exe

distclean : clean
	$(RM) libkai-*

src : kai.c kai.h kai_internal.h kai_dart.c kai_dart.h kai_uniaud.c \
      kai_uniaud.h kaidll.mk \
      kaidemo.c kaidemo2.c demo1.wav demo2.wav demo3.wav \
      Makefile Makefile.icc Makefile.wat \
      uniaud.h unidef.h unierrno.h uniaud.dll
	$(RM) libkai-$(VER)-src.zip
	$(RM) -r libkai-$(VER)
	mkdir libkai-$(VER)
	cp $^ libkai-$(VER)
	zip -rpSm libkai-$(VER)-src.zip libkai-$(VER)

install : kai.a kai.lib kai_dll.a kai_dll.lib $(KAIDLL) kai.h
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)$(INCDIR)
	$(INSTALL) kai.a $(DESTDIR)$(LIBDIR)
	$(INSTALL) kai.lib $(DESTDIR)$(LIBDIR)
	$(INSTALL) kai_dll.a $(DESTDIR)$(LIBDIR)
	$(INSTALL) kai_dll.lib $(DESTDIR)$(LIBDIR)
	$(INSTALL) $(KAIDLL) $(DESTDIR)$(LIBDIR)
	$(INSTALL) kai.h $(DESTDIR)$(INCDIR)

uninstall :
	$(RM) $(DESTDIR)$(LIBDIR)/kai.a $(DESTDIR)$(LIBDIR)/kai.lib
	$(RM) $(DESTDIR)$(LIBDIR)/kai_dll.a $(DESTDIR)$(LIBDIR)/kai_dll.lib
	$(RM) $(DESTDIR)$(LIBDIR)/$(KAIDLL)
	$(RM) $(DESTDIR)$(INCDIR)/kai.h
