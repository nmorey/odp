include ../Makefile.inc

noinst_LTLIBRARIES = libodptimer.la
libodptimer_la_SOURCES = timer.c

bin_PROGRAMS = timer_main$(EXEEXT)
dist_timer_main_SOURCES = timer_main.c
timer_main_LDADD = libodptimer.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = timer.h
