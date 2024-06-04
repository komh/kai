kai.obj kai.dll_obj: kai.c kai_internal.h kai.h kai_atomic.h kai_spinlock.h \
 kai_instance.h kai_debug.h kai_dart.h kai_uniaud.h kai_mixer.h \
 speex/speex_resampler.h kai_audiobuffer.h kai_server.h
kai_dart.obj kai_dart.dll_obj: kai_dart.c kai_internal.h kai.h kai_atomic.h kai_spinlock.h \
 kai_instance.h kai_debug.h kai_audiobuffer.h kai_dart.h
kai_uniaud.obj kai_uniaud.dll_obj: kai_uniaud.c kai_internal.h kai.h kai_atomic.h \
 kai_spinlock.h kai_instance.h kai_debug.h kai_audiobuffer.h kai_uniaud.h \
 uniaud.h unidef.h
resample.obj resample.dll_obj: speex/resample.c speex/speex_resampler.h speex/arch.h \
 speex/stack_alloc.h
kai_audiobuffer.obj kai_audiobuffer.dll_obj: kai_audiobuffer.c kai_internal.h kai.h kai_atomic.h \
 kai_spinlock.h kai_instance.h kai_debug.h kai_audiobuffer.h
kai_instance.obj kai_instance.dll_obj: kai_instance.c kai_internal.h kai.h kai_atomic.h \
 kai_spinlock.h kai_instance.h kai_debug.h kai_mixer.h \
 speex/speex_resampler.h kai_audiobuffer.h
kai_debug.obj kai_debug.dll_obj: kai_debug.c kai_internal.h kai.h kai_atomic.h kai_spinlock.h \
 kai_instance.h kai_debug.h
kai_mixer.obj kai_mixer.dll_obj: kai_mixer.c kai_internal.h kai.h kai_atomic.h kai_spinlock.h \
 kai_instance.h kai_debug.h kai_server.h kai_mixer.h \
 speex/speex_resampler.h kai_audiobuffer.h
kai_spinlock.obj kai_spinlock.dll_obj: kai_spinlock.c kai_internal.h kai.h kai_atomic.h \
 kai_spinlock.h kai_instance.h kai_debug.h
kai_server.obj kai_server.dll_obj: kai_server.c kai_internal.h kai.h kai_atomic.h \
 kai_spinlock.h kai_instance.h kai_debug.h kai_server.h
kai_atomic.obj kai_atomic.dll_obj : kai_atomic.asm \
  os2section.inc

kaisrv.obj: kaisrv.c kai_internal.h kai.h kai_atomic.h kai_spinlock.h \
 kai_instance.h kai_debug.h kai_server.h
kaidemo.obj: kaidemo.c kai.h
kaidemo2.obj: kaidemo2.c kai.h
kaidemo3.obj: kaidemo3.c kai.h
