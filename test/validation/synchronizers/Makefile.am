include ../Makefile.inc

noinst_LTLIBRARIES = libodpsynchronizers.la
libodpsynchronizers_la_SOURCES = synchronizers.c

bin_PROGRAMS = synchronizers_main$(EXEEXT)
dist_synchronizers_main_SOURCES = synchronizers_main.c
synchronizers_main_LDADD = libodpsynchronizers.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = synchronizers.h
