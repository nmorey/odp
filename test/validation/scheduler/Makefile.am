include ../Makefile.inc

noinst_LTLIBRARIES = libodpscheduler.la
libodpscheduler_la_SOURCES = scheduler.c

bin_PROGRAMS = scheduler_main$(EXEEXT)
dist_scheduler_main_SOURCES = scheduler_main.c
scheduler_main_LDADD = libodpscheduler.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = scheduler.h
