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
SPEEX_CFLAGS = -DOUTSIDE_SPEEX -DEXPORT= -DRANDOM_PREFIX=kai -DFLOATING_POINT
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
      kaidemo.exe kaidemo2.exe kaidemo3.exe

kai.a : kai.o kai_dart.o kai_uniaud.o kai_audiobuffer.o kai_instance.o \
        speex/resample.o
	$(AR) rc $@ $^

kai_dll.a : $(KAIDLL)
	emximp -o $@ $(KAIDLL)

$(KAIDLL): kai.o kai_dart.o kai_uniaud.o kai_audiobuffer.o kai_instance.o \
           speex/resample.o $(KAIDLLDEF)
	$(CC) -Zdll $(LDFLAGS) -o $@ $^
	echo $(BLDLEVEL)K Audio Interface >> $@

$(KAIDLLDEF):
	echo LIBRARY $(KAIDLLNAME) INITINSTANCE TERMINSTANCE > $@
	echo DATA MULTIPLE NONSHARED >> $@

kai.o: kai.c kai.h kai_internal.h kai_instance.h kai_mixer.h \
       kai_dart.h kai_uniaud.h speex/speex_resampler.h

kai_instance.o: kai_instance.c kai_instance.h kai.h kai_mixer.h \
                speex/speex_resampler.h

kai_dart.o : kai_dart.c kai.h kai_internal.h kai_dart.h kai_audiobuffer.h

kai_uniaud.o : kai_uniaud.c uniaud.h unidef.h kai.h kai_internal.h \
               kai_uniaud.h kai_audiobuffer.h

kai_audiobuffer.o: kai_audiobuffer.c kai_audiobuffer.h

speex/resample.o: speex/resample.c speex/speex_resampler.h \
                  speex/arch.h speex/stack_alloc.h

kai.o kai_instance.o speex/resample.o: CFLAGS += $(SPEEX_CFLAGS)

kaidemo.exe : kaidemo.o kai.lib
	$(CC) $(LDFLAGS) -o $@ $^ -lmmpm2
	echo $(BLDLEVEL)KAI demo >> $@

kaidemo.o : kaidemo.c kai.h

kaidemo2.exe : kaidemo2.o kai.lib
	$(CC) $(LDFLAGS) -o $@ $^ -lmmpm2
	echo $(BLDLEVEL)KAI demo for multiple instances >> $@

kaidemo2.o : kaidemo2.c kai.h

kaidemo3.exe : kaidemo3.o kai.lib
	$(CC) $(LDFLAGS) -o $@ $^ -lmmpm2
	echo $(BLDLEVEL)KAI demo for mixer streams >> $@

kaidemo3.o : kaidemo3.c kai.h

clean :
	$(RM) *.bak speex/*.bak
	$(RM) *.o speex/*.o
	$(RM) *.a
	$(RM) *.obj speex/*.obj
	$(RM) *.lib
	$(RM) *.def
	$(RM) $(KAIDLL)
	$(RM) *.exe

distclean : clean
	$(RM) libkai-*

KAI_SRCS := kai.c kai.h kai_internal.h kai_dart.c kai_dart.h kai_uniaud.c \
            kai_uniaud.h kai_audiobuffer.c kai_audiobuffer.h kaidll.mk \
            kai_instance.c kai_instance.h kai_mixer.h \
            kaidemo.c kaidemo2.c kaidemo3.c demo1.wav demo2.wav demo3.wav \
            Makefile Makefile.icc Makefile.wat \
            uniaud.h unidef.h unierrno.h

SPEEX_SRCS := speex/arch.h speex/fixed_generic.h speex/resample.c \
              speex/resample_neon.h speex/resample_sse.h \
              speex/speex_config_types.h speex/speex_resampler.h \
              speex/stack_alloc.h \

src : $(KAI_SRCS) $(SPEEX_SRCS)
	$(RM) libkai-$(VER)-src.zip
	$(RM) -r libkai-$(VER)
	mkdir libkai-$(VER)
	mkdir libkai-$(VER)/speex
	cp $(KAI_SRCS) libkai-$(VER)
	cp $(SPEEX_SRCS) libkai-$(VER)/speex
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
