include ../Makefile.inc

noinst_LTLIBRARIES = libodpcpumask.la
libodpcpumask_la_SOURCES = cpumask.c
libodpcpumask_la_LIBADD = $(LIBCPUMASK_COMMON)

bin_PROGRAMS = cpumask_main$(EXEEXT)
dist_cpumask_main_SOURCES = cpumask_main.c
cpumask_main_LDADD = libodpcpumask.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = cpumask.h
