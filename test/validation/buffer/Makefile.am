include ../Makefile.inc

noinst_LTLIBRARIES = libodpbuffer.la
libodpbuffer_la_SOURCES = buffer.c

bin_PROGRAMS = buffer_main$(EXEEXT)
dist_buffer_main_SOURCES = buffer_main.c
buffer_main_LDADD = libodpbuffer.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = buffer.h
