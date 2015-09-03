include ../Makefile.inc

noinst_LTLIBRARIES = libodpthread.la
libodpthread_la_SOURCES = thread.c
libodpthread_la_CFLAGS = $(AM_CFLAGS) -DTEST_THRMASK
libodpthread_la_LIBADD = $(LIBTHRMASK_COMMON)

bin_PROGRAMS = thread_main$(EXEEXT)
dist_thread_main_SOURCES = thread_main.c
thread_main_LDADD = libodpthread.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = thread.h
