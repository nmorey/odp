include ../Makefile.inc

noinst_LTLIBRARIES = libodpqueue.la
libodpqueue_la_SOURCES = queue.c

bin_PROGRAMS = queue_main$(EXEEXT)
dist_queue_main_SOURCES = queue_main.c
queue_main_LDADD = libodpqueue.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = queue.h
